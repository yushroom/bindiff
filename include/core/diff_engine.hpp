#pragma once

#include "core/operations.hpp"
#include "io/mmap_file.hpp"
#include "utils/thread_pool.hpp"
#include "compress/compressor.hpp"
#include "types.hpp"
#include <memory>

namespace bindiff {

// ============== 分块处理器 ==============

struct BlockResult {
    uint32_t block_index;
    std::vector<uint8_t> data;  // 压缩后的操作序列
    bool success = false;
    std::string error;
};

class BlockProcessor {
public:
    BlockProcessor(uint32_t block_size, int compression_level = 1);
    ~BlockProcessor();
    
    // 处理单个块 (可并行)
    BlockResult process_block(
        uint32_t block_index,
        const byte* old_data, size_t old_size,
        const byte* new_data, size_t new_size
    );
    
    // 从块数据重建文件
    bool reconstruct_block(
        uint32_t block_index,
        const byte* old_data, size_t old_size,
        const std::vector<uint8_t>& block_data,
        byte* output, size_t output_size
    );

private:
    uint32_t block_size_;
    int compression_level_;
    std::unique_ptr<Compressor> compressor_;
};

// ============== 差分引擎 ==============

class DiffEngine {
public:
    DiffEngine(const DiffOptions& options = {});
    ~DiffEngine();
    
    Result create_diff(
        const std::string& old_path,
        const std::string& new_path,
        const std::string& patch_path,
        ProgressCallback* callback = nullptr
    );

private:
    void init_thread_pool();
    std::vector<BlockResult> process_all_blocks(
        MMapFile& old_file,
        MMapFile& new_file,
        ProgressCallback* callback
    );
    bool write_patch_file(
        const std::string& path,
        const MMapFile& old_file,
        const MMapFile& new_file,
        const std::vector<BlockResult>& blocks
    );
    
    DiffOptions options_;
    std::unique_ptr<ThreadPool> thread_pool_;
    std::unique_ptr<BlockProcessor> block_processor_;
};

// ============== 补丁引擎 ==============

class PatchEngine {
public:
    PatchEngine(const PatchOptions& options = {});
    ~PatchEngine();
    
    Result apply_patch(
        const std::string& old_path,
        const std::string& patch_path,
        const std::string& new_path,
        ProgressCallback* callback = nullptr
    );

private:
    bool read_patch_header(const std::string& path);
    bool reconstruct_all_blocks(
        MMapFile& old_file,
        const std::string& patch_path,
        const std::string& output_path,
        ProgressCallback* callback
    );
    
    PatchOptions options_;
    PatchInfo patch_info_;
    std::vector<uint64_t> block_offsets_;
};

} // namespace bindiff
