#pragma once

#include <cstdint>
#include <array>
#include <string>

namespace bindiff {

// ============== SHA-256 实现 ==============

class SHA256 {
public:
    SHA256();
    ~SHA256() = default;
    
    // 更新数据
    void update(const uint8_t* data, size_t size);
    void update(const std::string& str);
    
    // 完成计算，获取哈希值
    std::array<uint8_t, 32> finalize();
    
    // 重置，可重新计算
    void reset();
    
    // 静态方法: 一次性计算
    static std::array<uint8_t, 32> compute(const uint8_t* data, size_t size);
    static std::array<uint8_t, 32> compute(const std::string& str);
    static std::array<uint8_t, 32> compute_file(const std::string& path);
    
    // 转换为十六进制字符串
    static std::string to_hex(const std::array<uint8_t, 32>& hash);
    static std::string to_hex(const uint8_t* data, size_t size);

private:
    void process_block(const uint8_t* block);
    
    uint64_t bitlen_;
    uint32_t state_[8];
    uint8_t buffer_[64];
    size_t buffer_len_;
};

} // namespace bindiff
