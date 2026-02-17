#pragma once

#include "types.hpp"
#include <cstdint>
#include <vector>

namespace bindiff {

// ============== 操作码 ==============

enum class OpCode : uint8_t {
    COPY = 0x00,    // 从旧文件复制
    INSERT = 0x01,  // 插入新数据
};

// ============== 操作指令 ==============

struct Operation {
    OpCode opcode;
    
    // COPY 操作的数据
    uint64_t copy_offset = 0;
    uint32_t copy_length = 0;
    
    // INSERT 操作的数据
    std::vector<byte> insert_data;
    
    // 构造函数
    static Operation copy(uint64_t offset, uint32_t length) {
        Operation op;
        op.opcode = OpCode::COPY;
        op.copy_offset = offset;
        op.copy_length = length;
        return op;
    }
    
    static Operation insert(const byte* data, size_t size) {
        Operation op;
        op.opcode = OpCode::INSERT;
        op.insert_data.assign(data, data + size);
        return op;
    }
    
    static Operation insert(const std::vector<byte>& data) {
        Operation op;
        op.opcode = OpCode::INSERT;
        op.insert_data = data;
        return op;
    }
    
    // 序列化
    size_t serialized_size() const {
        if (opcode == OpCode::COPY) {
            return 1 + 8 + 4;  // opcode + offset + length
        } else {
            return 1 + 4 + insert_data.size();  // opcode + length + data
        }
    }
};

// ============== 序列化 ==============

class OperationSerializer {
public:
    // 序列化操作到字节流
    static void serialize(const Operation& op, std::vector<byte>& output);
    
    // 从字节流反序列化
    // 返回: 读取的字节数，0 表示失败
    static size_t deserialize(
        const byte* data, 
        size_t size, 
        Operation& op
    );
    
    // 批量序列化
    static void serialize_all(
        const std::vector<Operation>& ops,
        std::vector<byte>& output
    );
    
    // 计算总大小
    static size_t total_size(const std::vector<Operation>& ops);
};

} // namespace bindiff
