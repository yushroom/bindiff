#include "core/block_processor.hpp"
#include "core/matcher.hpp"
#include "compress/compressor.hpp"
#include <algorithm>

namespace bindiff {

// ============== BlockProcessor 实现 ==============

BlockProcessor::BlockProcessor(uint32_t block_size, int compression_level)
    : block_size_(block_size)
    , compression_level_(compression_level)
{
    compressor_ = std::make_unique<LZ4Compressor>(compression_level);
}

BlockProcessor::~BlockProcessor() = default;

BlockResult BlockProcessor::process_block(
    uint32_t block_index,
    const byte* old_data, size_t old_size,
    const byte* new_data, size_t new_size
) {
    BlockResult result;
    result.block_index = block_index;
    
    // TODO: 实现完整的 diff 算法
    // 1. 使用 BlockMatcher 找匹配
    // 2. 生成 COPY/INSERT 操作
    // 3. 序列化 + 压缩
    
    // 简单实现: 全部作为 INSERT
    std::vector<Operation> ops;
    if (new_size > 0) {
        ops.push_back(Operation::insert(new_data, new_size));
    }
    
    // 序列化
    std::vector<byte> serialized;
    OperationSerializer::serialize_all(ops, serialized);
    
    // 压缩
    result.data = compressor_->compress(serialized.data(), serialized.size());
    result.success = true;
    
    return result;
}

bool BlockProcessor::reconstruct_block(
    uint32_t block_index,
    const byte* old_data, size_t old_size,
    const std::vector<uint8_t>& block_data,
    byte* output, size_t output_size
) {
    // TODO: 实现
    // 1. 解压
    // 2. 反序列化操作
    // 3. 执行 COPY/INSERT
    
    return false;
}

} // namespace bindiff
