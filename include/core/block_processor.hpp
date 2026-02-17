#pragma once

#include "core/operations.hpp"
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

class Compressor;  // 前向声明

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

} // namespace bindiff
