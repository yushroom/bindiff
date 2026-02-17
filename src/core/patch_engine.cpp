#include "core/patch_engine.hpp"
#include "core/patch_format.hpp"
#include "core/operations.hpp"
#include "core/block_processor.hpp"
#include "io/stream_writer.hpp"
#include "crypto/sha256.hpp"
#include <fstream>
#include <chrono>
#include <cstring>

namespace bindiff {

// ============== PatchEngine 实现 ==============

PatchEngine::PatchEngine(const PatchOptions& options)
    : options_(options)
{
}

PatchEngine::~PatchEngine() = default;

Result PatchEngine::apply_patch(
    const std::string& old_path,
    const std::string& patch_path,
    const std::string& new_path,
    ProgressCallback* callback
) {
    Result result;
    auto start = std::chrono::high_resolution_clock::now();
    
    // 1. 检查文件
    if (!file_exists(old_path)) {
        result.error = "原文件不存在: " + old_path;
        return result;
    }
    if (!file_exists(patch_path)) {
        result.error = "补丁文件不存在: " + patch_path;
        return result;
    }
    
    // 2. 读取 patch header
    if (!read_patch_header(patch_path)) {
        result.error = "无效的补丁文件";
        return result;
    }
    
    // 3. 打开原文件
    MMapFile old_file;
    if (!old_file.open(old_path)) {
        result.error = "无法打开原文件: " + old_file.error();
        return result;
    }
    
    // 4. 验证原文件大小
    if (old_file.size() != patch_info_.old_size) {
        result.error = "原文件大小不匹配";
        return result;
    }
    
    // 5. 验证原文件 SHA256
    if (options_.verify) {
        if (callback) {
            callback->on_progress(0.1f, "验证原文件");
        }
        // TODO: 验证 SHA256
    }
    
    // 6. 重建文件
    if (callback) {
        callback->on_progress(0.2f, "应用补丁");
    }
    if (!reconstruct_all_blocks(old_file, patch_path, new_path, callback)) {
        result.error = "重建文件失败";
        return result;
    }
    
    // 7. 验证新文件
    if (options_.verify) {
        if (callback) {
            callback->on_progress(0.9f, "验证新文件");
        }
        // TODO: 验证 SHA256
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    result.success = true;
    result.bytes_processed = patch_info_.new_size;
    result.elapsed_seconds = std::chrono::duration<double>(end - start).count();
    
    if (callback) {
        callback->on_complete(result);
    }
    
    return result;
}

bool PatchEngine::read_patch_header(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    
    PatchHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (!file || !header.is_valid()) {
        return false;
    }
    
    patch_info_.version = header.version;
    patch_info_.block_size = header.block_size;
    patch_info_.old_size = header.old_size;
    patch_info_.new_size = header.new_size;
    patch_info_.num_blocks = header.num_blocks;
    patch_info_.old_sha256 = SHA256::to_hex(header.old_sha256, 32);
    patch_info_.new_sha256 = SHA256::to_hex(header.new_sha256, 32);
    
    // 读取块索引
    block_offsets_.resize(header.num_blocks);
    file.seekg(PatchHeader::SIZE);
    file.read(reinterpret_cast<char*>(block_offsets_.data()), 
              header.num_blocks * sizeof(uint64_t));
    
    return true;
}

bool PatchEngine::reconstruct_all_blocks(
    MMapFile& old_file,
    const std::string& patch_path,
    const std::string& output_path,
    ProgressCallback* callback
) {
    std::ifstream patch_file(patch_path, std::ios::binary);
    if (!patch_file) {
        return false;
    }
    
    std::ofstream output(output_path, std::ios::binary);
    if (!output) {
        return false;
    }
    
    // 预分配文件大小
    if (patch_info_.new_size > 0) {
        output.seekp(patch_info_.new_size - 1);
        output.put(0);
        output.seekp(0);
    }
    
    // 创建块处理器
    BlockProcessor processor(patch_info_.block_size, 1);
    
    // 处理每个块
    std::vector<uint8_t> output_buffer;
    output_buffer.reserve(patch_info_.block_size);
    
    for (uint32_t i = 0; i < patch_info_.num_blocks; ++i) {
        // 读取块偏移
        patch_file.seekg(block_offsets_[i]);
        
        // 读取原始大小
        uint32_t original_size;
        patch_file.read(reinterpret_cast<char*>(&original_size), sizeof(original_size));
        
        // 读取压缩后大小
        uint32_t compressed_size;
        patch_file.read(reinterpret_cast<char*>(&compressed_size), sizeof(compressed_size));
        
        // 读取压缩数据
        std::vector<uint8_t> compressed_data(compressed_size);
        if (compressed_size > 0) {
            patch_file.read(reinterpret_cast<char*>(compressed_data.data()), compressed_size);
        }
        
        // 计算块输出大小
        uint64_t block_start = static_cast<uint64_t>(i) * patch_info_.block_size;
        size_t block_output_size = static_cast<size_t>(std::min(
            static_cast<uint64_t>(patch_info_.block_size),
            patch_info_.new_size - block_start
        ));
        
        // 重建块
        output_buffer.resize(block_output_size);
        if (!processor.reconstruct_block(
            i,
            old_file.data(), static_cast<size_t>(old_file.size()),
            compressed_data,
            original_size,
            output_buffer.data(),
            block_output_size
        )) {
            return false;
        }
        
        // 写入输出
        output.seekp(block_start);
        output.write(reinterpret_cast<char*>(output_buffer.data()), block_output_size);
        
        if (callback) {
            float progress = 0.2f + 0.7f * (i + 1) / patch_info_.num_blocks;
            callback->on_progress(progress, "应用补丁");
        }
    }
    
    return true;
}

} // namespace bindiff
