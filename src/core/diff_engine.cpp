#include "core/diff_engine.hpp"
#include "core/matcher.hpp"
#include "core/patch_format.hpp"
#include "io/stream_writer.hpp"
#include "crypto/sha256.hpp"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <fstream>

namespace bindiff {

// ============== DiffEngine 实现 ==============

DiffEngine::DiffEngine(const DiffOptions& options)
    : options_(options)
{
}

DiffEngine::~DiffEngine() = default;

Result DiffEngine::create_diff(
    const std::string& old_path,
    const std::string& new_path,
    const std::string& patch_path,
    ProgressCallback* callback
) {
    Result result;
    auto start = std::chrono::high_resolution_clock::now();
    
    // 1. 检查文件是否存在
    if (!file_exists(old_path)) {
        result.error = "原文件不存在: " + old_path;
        return result;
    }
    if (!file_exists(new_path)) {
        result.error = "新文件不存在: " + new_path;
        return result;
    }
    
    // 2. 打开文件
    MMapFile old_file, new_file;
    if (!old_file.open(old_path)) {
        result.error = "无法打开原文件: " + old_file.error();
        return result;
    }
    if (!new_file.open(new_path)) {
        result.error = "无法打开新文件: " + new_file.error();
        return result;
    }
    
    // 3. 计算 SHA256
    std::array<uint8_t, 32> old_hash, new_hash;
    if (options_.verify) {
        if (callback) {
            callback->on_progress(0.0f, "计算原文件 SHA256");
        }
        old_hash = SHA256::compute(old_file.data(), old_file.size());
        
        if (callback) {
            callback->on_progress(0.3f, "计算新文件 SHA256");
        }
        new_hash = SHA256::compute(new_file.data(), new_file.size());
    } else {
        old_hash.fill(0);
        new_hash.fill(0);
    }
    
    // 4. 初始化线程池
    init_thread_pool();
    
    // 5. 并行构建全局索引
    if (callback) {
        callback->on_progress(0.35f, "构建全局索引");
    }
    global_matcher_ = std::make_unique<BlockMatcher>(32);
    
    // 使用与线程池相同的线程数
    int index_threads = options_.num_threads;
    if (index_threads <= 0) {
        index_threads = static_cast<int>(std::thread::hardware_concurrency());
        if (index_threads <= 0) index_threads = 4;
    }
    global_matcher_->build_index_parallel(old_file.data(), old_file.size(), 32, index_threads);
    
    // 6. 分块处理
    if (callback) {
        callback->on_progress(0.4f, "分析文件差异");
    }
    auto blocks = process_all_blocks(old_file, new_file, callback);
    
    // 检查是否所有块都成功
    for (const auto& block : blocks) {
        if (!block.success) {
            result.error = "处理块失败: " + block.error;
            return result;
        }
    }
    
    // 6. 写入 patch 文件
    if (callback) {
        callback->on_progress(0.9f, "写入补丁文件");
    }
    if (!write_patch_file(patch_path, old_file, new_file, blocks, old_hash, new_hash)) {
        result.error = "写入补丁文件失败";
        return result;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.success = true;
    result.bytes_processed = new_file.size();
    result.elapsed_seconds = std::chrono::duration<double>(end - start).count();
    
    if (callback) {
        callback->on_complete(result);
    }
    
    return result;
}

void DiffEngine::init_thread_pool() {
    int threads = options_.num_threads;
    if (threads <= 0) {
        threads = static_cast<int>(std::thread::hardware_concurrency());
        if (threads <= 0) threads = 4;
    }
    thread_pool_ = std::make_unique<ThreadPool>(threads);
    block_processor_ = std::make_unique<BlockProcessor>(options_.block_size, options_.compression_level);
}

std::vector<BlockResult> DiffEngine::process_all_blocks(
    MMapFile& old_file,
    MMapFile& new_file,
    ProgressCallback* callback
) {
    uint64_t new_size = new_file.size();
    uint32_t num_blocks = static_cast<uint32_t>(
        (new_size + options_.block_size - 1) / options_.block_size
    );
    
    if (num_blocks == 0) num_blocks = 1;  // 空文件至少有1个块
    
    std::vector<BlockResult> results(num_blocks);
    std::vector<std::future<BlockResult>> futures;
    
    // 提交所有块处理任务
    for (uint32_t i = 0; i < num_blocks; ++i) {
        uint64_t start = i * options_.block_size;
        uint64_t end = std::min(start + options_.block_size, new_size);
        uint64_t block_size = end - start;
        
        futures.push_back(thread_pool_->submit([this, i, &old_file, &new_file, start, block_size]() {
            const byte* new_data = new_file.data() + start;
            size_t new_block_size = static_cast<size_t>(block_size);
            
            const byte* old_data = old_file.data();
            size_t old_size = static_cast<size_t>(old_file.size());
            
            // 使用全局索引
            return block_processor_->process_block(
                i, old_data, old_size, new_data, new_block_size, 
                global_matcher_.get()
            );
        }));
    }
    
    // 收集结果
    for (size_t i = 0; i < futures.size(); ++i) {
        results[i] = futures[i].get();
        
        if (callback) {
            float progress = 0.4f + 0.5f * (i + 1) / num_blocks;
            callback->on_progress(progress, "处理数据块");
        }
    }
    
    return results;
}

bool DiffEngine::write_patch_file(
    const std::string& path,
    const MMapFile& old_file,
    const MMapFile& new_file,
    const std::vector<BlockResult>& blocks,
    const std::array<uint8_t, 32>& old_hash,
    const std::array<uint8_t, 32>& new_hash
) {
    // 使用 ofstream 直接写文件
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    
    // 1. 写入 Header
    PatchHeader header;
    header.init(options_.block_size, old_file.size(), new_file.size());
    std::memcpy(header.old_sha256, old_hash.data(), 32);
    std::memcpy(header.new_sha256, new_hash.data(), 32);
    header.num_blocks = static_cast<uint32_t>(blocks.size());
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(header));
    
    // 2. 先写入块索引占位符
    uint64_t index_pos = static_cast<uint64_t>(file.tellp());
    std::vector<uint64_t> block_offsets(blocks.size(), 0);
    file.write(reinterpret_cast<const char*>(block_offsets.data()), 
               blocks.size() * sizeof(uint64_t));
    
    // 3. 写入块数据
    for (size_t i = 0; i < blocks.size(); ++i) {
        block_offsets[i] = static_cast<uint64_t>(file.tellp());
        
        // 写入 original_size
        uint32_t orig_size = blocks[i].original_size;
        file.write(reinterpret_cast<const char*>(&orig_size), sizeof(orig_size));
        
        // 写入 compressed_size
        uint32_t comp_size = static_cast<uint32_t>(blocks[i].data.size());
        file.write(reinterpret_cast<const char*>(&comp_size), sizeof(comp_size));
        
        // 写入压缩数据
        if (comp_size > 0) {
            file.write(reinterpret_cast<const char*>(blocks[i].data.data()), comp_size);
        }
    }
    
    // 4. 回填块索引
    file.seekp(static_cast<std::streamoff>(index_pos));
    file.write(reinterpret_cast<const char*>(block_offsets.data()), 
               blocks.size() * sizeof(uint64_t));
    
    return file.good();
}

} // namespace bindiff
