#pragma once

#include "core/operations.hpp"
#include "core/matcher.hpp"
#include "io/mmap_file.hpp"
#include "utils/thread_pool.hpp"
#include "compress/compressor.hpp"
#include "types.hpp"
#include <memory>

namespace bindiff {

// ============== 分块处理器 ==============

struct BlockResult {
    uint32_t block_index;
    std::vector<uint8_t> data;  // 压缩后的数据
    uint32_t original_size = 0; // 压缩前的序列化数据大小
    bool success = false;
    std::string error;
};

class BlockProcessor {
public:
    BlockProcessor(uint32_t block_size, int compression_level = 1);
    ~BlockProcessor();
    
    // 处理单个块 (可并行) - 使用全局索引
    BlockResult process_block(
        uint32_t block_index,
        const byte* old_data, size_t old_size,
        const byte* new_data, size_t new_size,
        const BlockMatcher* global_matcher = nullptr
    );
    
    // 从块数据重建文件
    bool reconstruct_block(
        uint32_t block_index,
        const byte* old_data, size_t old_size,
        const std::vector<uint8_t>& compressed_data,
        uint32_t original_size,  // 解压后的原始大小
        byte* output, size_t output_size
    );

private:
    uint32_t block_size_;
    int compression_level_;
    std::unique_ptr<Compressor> compressor_;
};

} // namespace bindiff
