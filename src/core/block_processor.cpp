#include "core/block_processor.hpp"
#include "core/matcher.hpp"
#include "compress/compressor.hpp"
#include <algorithm>
#include <cstring>

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
    
    if (new_size == 0) {
        result.success = true;
        result.original_size = 0;
        return result;
    }
    
    // 使用块匹配器生成操作
    std::vector<Operation> operations;
    BlockMatcher matcher(32);  // 最小匹配 32 字节
    
    size_t pos = 0;
    while (pos < new_size) {
        // 尝试找到匹配
        auto match = matcher.find_longest_match(
            old_data, old_size,
            new_data, new_size,
            pos
        );
        
        if (match.valid() && match.length >= 32) {
            // 找到匹配，生成 COPY 操作
            operations.push_back(Operation::copy(match.old_offset, static_cast<uint32_t>(match.length)));
            pos += match.length;
        } else {
            // 没有匹配，收集未匹配字节生成 INSERT
            size_t insert_start = pos;
            size_t insert_end = pos + 1;
            
            // 继续查找下一个匹配点
            while (insert_end < new_size) {
                auto next_match = matcher.find_longest_match(
                    old_data, old_size,
                    new_data, new_size,
                    insert_end
                );
                
                if (next_match.valid() && next_match.length >= 32) {
                    break;  // 找到下一个匹配，停止收集
                }
                ++insert_end;
                
                // 限制 INSERT 大小 (避免过大的单个操作)
                if (insert_end - insert_start >= 65536) {
                    break;
                }
            }
            
            // 生成 INSERT 操作
            size_t insert_size = insert_end - insert_start;
            operations.push_back(Operation::insert(new_data + insert_start, insert_size));
            pos = insert_end;
        }
    }
    
    // 序列化操作
    std::vector<byte> serialized;
    OperationSerializer::serialize_all(operations, serialized);
    
    // 保存原始大小
    result.original_size = static_cast<uint32_t>(serialized.size());
    
    // 压缩
    if (serialized.size() > 0) {
        result.data = compressor_->compress(serialized.data(), serialized.size());
    }
    
    result.success = true;
    return result;
}

bool BlockProcessor::reconstruct_block(
    uint32_t /*block_index*/,
    const byte* old_data, size_t old_size,
    const std::vector<uint8_t>& compressed_data,
    uint32_t original_size,
    byte* output, size_t output_size
) {
    if (compressed_data.empty()) {
        return output_size == 0;
    }
    
    // 解压
    auto decompressed = compressor_->decompress(
        compressed_data.data(),
        compressed_data.size(),
        original_size
    );
    
    if (decompressed.empty() && original_size > 0) {
        return false;
    }
    
    // 解析并执行操作
    size_t pos = 0;
    size_t output_pos = 0;
    
    while (pos < decompressed.size() && output_pos < output_size) {
        Operation op;
        size_t read = OperationSerializer::deserialize(
            decompressed.data() + pos,
            decompressed.size() - pos,
            op
        );
        
        if (read == 0) break;
        pos += read;
        
        if (op.opcode == OpCode::COPY) {
            // 从原文件复制
            if (op.copy_offset >= old_size || 
                op.copy_offset + op.copy_length > old_size) {
                return false;
            }
            std::memcpy(output + output_pos, old_data + op.copy_offset, op.copy_length);
            output_pos += op.copy_length;
        } else {
            // 插入新数据
            if (output_pos + op.insert_data.size() > output_size) {
                return false;
            }
            std::memcpy(output + output_pos, op.insert_data.data(), op.insert_data.size());
            output_pos += op.insert_data.size();
        }
    }
    
    return output_pos == output_size;
}

} // namespace bindiff
