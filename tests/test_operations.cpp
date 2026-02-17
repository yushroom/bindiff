#include <iostream>
#include <cstring>
#include <vector>

#include "core/operations.hpp"

TEST(operations_copy) {
    auto op = bindiff::Operation::copy(0x123456789ABCDEF0ULL, 0x12345678);
    
    ASSERT(op.opcode == bindiff::OpCode::COPY);
    ASSERT(op.copy_offset == 0x123456789ABCDEF0ULL);
    ASSERT(op.copy_length == 0x12345678);
    
    return true;
}

TEST(operations_insert) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto op = bindiff::Operation::insert(data);
    
    ASSERT(op.opcode == bindiff::OpCode::INSERT);
    ASSERT(op.insert_data.size() == 5);
    ASSERT(std::memcmp(op.insert_data.data(), data.data(), 5) == 0);
    
    return true;
}

TEST(operations_serialize_copy) {
    auto op = bindiff::Operation::copy(0x100, 0x50);
    
    std::vector<uint8_t> buffer;
    bindiff::OperationSerializer::serialize(op, buffer);
    
    // COPY: opcode(1) + offset(8) + length(4) = 13 bytes
    ASSERT(buffer.size() == 13);
    ASSERT(buffer[0] == 0x00);  // COPY opcode
    
    // offset (小端序)
    ASSERT(buffer[1] == 0x00);
    ASSERT(buffer[2] == 0x01);
    ASSERT(buffer[3] == 0x00);
    // ...
    
    return true;
}

TEST(operations_serialize_insert) {
    std::vector<uint8_t> data = {10, 20, 30};
    auto op = bindiff::Operation::insert(data);
    
    std::vector<uint8_t> buffer;
    bindiff::OperationSerializer::serialize(op, buffer);
    
    // INSERT: opcode(1) + length(4) + data(3) = 8 bytes
    ASSERT(buffer.size() == 8);
    ASSERT(buffer[0] == 0x01);  // INSERT opcode
    ASSERT(buffer[1] == 3);     // length
    ASSERT(buffer[5] == 10);
    ASSERT(buffer[6] == 20);
    ASSERT(buffer[7] == 30);
    
    return true;
}

TEST(operations_deserialize_copy) {
    auto orig = bindiff::Operation::copy(0x123456789ABCDEF0ULL, 0x12345678);
    
    std::vector<uint8_t> buffer;
    bindiff::OperationSerializer::serialize(orig, buffer);
    
    bindiff::Operation parsed;
    size_t read = bindiff::OperationSerializer::deserialize(
        buffer.data(), buffer.size(), parsed
    );
    
    ASSERT(read == 13);
    ASSERT(parsed.opcode == bindiff::OpCode::COPY);
    ASSERT(parsed.copy_offset == 0x123456789ABCDEF0ULL);
    ASSERT(parsed.copy_length == 0x12345678);
    
    return true;
}

TEST(operations_deserialize_insert) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto orig = bindiff::Operation::insert(data);
    
    std::vector<uint8_t> buffer;
    bindiff::OperationSerializer::serialize(orig, buffer);
    
    bindiff::Operation parsed;
    size_t read = bindiff::OperationSerializer::deserialize(
        buffer.data(), buffer.size(), parsed
    );
    
    ASSERT(read == 9);  // 1 + 4 + 4 (不，应该是 1+4+5=10)
    // 修正: length=5, 所以总大小是 1+4+5=10
    ASSERT(read == 10);
    ASSERT(parsed.opcode == bindiff::OpCode::INSERT);
    ASSERT(parsed.insert_data.size() == 5);
    ASSERT(std::memcmp(parsed.insert_data.data(), data.data(), 5) == 0);
    
    return true;
}

TEST(operations_serialize_all) {
    std::vector<bindiff::Operation> ops;
    ops.push_back(bindiff::Operation::copy(0x100, 0x50));
    ops.push_back(bindiff::Operation::insert({1, 2, 3}));
    ops.push_back(bindiff::Operation::copy(0x200, 0x30));
    
    std::vector<uint8_t> buffer;
    bindiff::OperationSerializer::serialize_all(ops, buffer);
    
    // 13 + (1+4+3) + 13 = 34 bytes
    ASSERT(buffer.size() == 34);
    
    return true;
}

void register_operations_tests() {
    // 已通过 TEST 宏自动注册
}
