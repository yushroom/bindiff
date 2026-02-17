#pragma once

#include "types.hpp"
#include <cstdint>
#include <vector>

namespace bindiff {

// ============== 压缩接口 ==============

class Compressor {
public:
    virtual ~Compressor() = default;
    
    // 压缩数据
    // 返回: 压缩后的数据
    virtual std::vector<uint8_t> compress(
        const uint8_t* data, 
        size_t size
    ) = 0;
    
    // 解压数据
    // 返回: 解压后的数据，失败返回空
    virtual std::vector<uint8_t> decompress(
        const uint8_t* data, 
        size_t size,
        size_t original_size
    ) = 0;
    
    // 设置压缩级别
    virtual void set_level(int level) { level_ = level; }
    int level() const { return level_; }
    
    // 获取名称
    virtual const char* name() const = 0;
    
protected:
    int level_ = 1;
};

// ============== LZ4 压缩器 ==============

class LZ4Compressor : public Compressor {
public:
    LZ4Compressor(int level = 1);
    ~LZ4Compressor() override = default;
    
    std::vector<uint8_t> compress(
        const uint8_t* data, 
        size_t size
    ) override;
    
    std::vector<uint8_t> decompress(
        const uint8_t* data, 
        size_t size,
        size_t original_size
    ) override;
    
    const char* name() const override { return "LZ4"; }
    
    // LZ4 特有: 获取压缩后最大大小
    static size_t compress_bound(size_t size);
};

// ============== 压缩器工厂 ==============

enum class CompressionType {
    None,
    LZ4,
};

std::unique_ptr<Compressor> create_compressor(
    CompressionType type, 
    int level = 1
);

} // namespace bindiff
