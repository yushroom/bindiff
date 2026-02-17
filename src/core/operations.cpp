#include "core/operations.hpp"
#include <cstring>

namespace bindiff {

// ============== OperationSerializer 实现 ==============

void OperationSerializer::serialize(const Operation& op, std::vector<byte>& output) {
    // 写入 opcode
    output.push_back(static_cast<byte>(op.opcode));
    
    if (op.opcode == OpCode::COPY) {
        // 写入 offset (小端序)
        for (int i = 0; i < 8; ++i) {
            output.push_back(static_cast<byte>(op.copy_offset >> (i * 8)));
        }
        // 写入 length
        for (int i = 0; i < 4; ++i) {
            output.push_back(static_cast<byte>(op.copy_length >> (i * 8)));
        }
    } else {
        // 写入 length
        uint32_t len = static_cast<uint32_t>(op.insert_data.size());
        for (int i = 0; i < 4; ++i) {
            output.push_back(static_cast<byte>(len >> (i * 8)));
        }
        // 写入 data
        output.insert(output.end(), op.insert_data.begin(), op.insert_data.end());
    }
}

size_t OperationSerializer::deserialize(
    const byte* data, 
    size_t size, 
    Operation& op
) {
    if (size < 1) return 0;
    
    op.opcode = static_cast<OpCode>(data[0]);
    size_t read = 1;
    
    if (op.opcode == OpCode::COPY) {
        if (size < 1 + 8 + 4) return 0;
        
        // 读取 offset
        op.copy_offset = 0;
        for (int i = 0; i < 8; ++i) {
            op.copy_offset |= static_cast<uint64_t>(data[1 + i]) << (i * 8);
        }
        
        // 读取 length
        op.copy_length = 0;
        for (int i = 0; i < 4; ++i) {
            op.copy_length |= static_cast<uint32_t>(data[9 + i]) << (i * 8);
        }
        
        read = 13;
    } else {
        if (size < 1 + 4) return 0;
        
        // 读取 length
        uint32_t len = 0;
        for (int i = 0; i < 4; ++i) {
            len |= static_cast<uint32_t>(data[1 + i]) << (i * 8);
        }
        
        if (size < 1 + 4 + len) return 0;
        
        // 读取 data
        op.insert_data.assign(data + 5, data + 5 + len);
        read = 5 + len;
    }
    
    return read;
}

void OperationSerializer::serialize_all(
    const std::vector<Operation>& ops,
    std::vector<byte>& output
) {
    output.clear();
    
    // 预分配空间
    size_t total = total_size(ops);
    output.reserve(total);
    
    // 序列化每个操作
    for (const auto& op : ops) {
        serialize(op, output);
    }
}

size_t OperationSerializer::total_size(const std::vector<Operation>& ops) {
    size_t total = 0;
    for (const auto& op : ops) {
        total += op.serialized_size();
    }
    return total;
}

} // namespace bindiff
