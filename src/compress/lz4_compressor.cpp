#include "compress/compressor.hpp"
#include <cstring>

#if BINDIFF_HAS_LZ4
#include <lz4.h>
#include <lz4hc.h>
#endif

namespace bindiff {

// ============== LZ4Compressor 实现 ==============

LZ4Compressor::LZ4Compressor(int level)
    : Compressor()
{
    level_ = level;
}

size_t LZ4Compressor::compress_bound(size_t size) {
#if BINDIFF_HAS_LZ4
    return static_cast<size_t>(LZ4_compressBound(static_cast<int>(size)));
#else
    // 无 LZ4 时，不添加额外字节
    return size;
#endif
}

std::vector<uint8_t> LZ4Compressor::compress(
    const uint8_t* data, 
    size_t size
) {
    if (size == 0) {
        return {};
    }
    
#if BINDIFF_HAS_LZ4
    // 分配输出缓冲区
    size_t bound = compress_bound(size);
    std::vector<uint8_t> output(bound);
    
    // 压缩
    int compressed_size;
    if (level_ <= 3) {
        // 快速模式
        compressed_size = LZ4_compress_default(
            reinterpret_cast<const char*>(data),
            reinterpret_cast<char*>(output.data()),
            static_cast<int>(size),
            static_cast<int>(bound)
        );
    } else {
        // 高压缩模式
        compressed_size = LZ4_compress_HC(
            reinterpret_cast<const char*>(data),
            reinterpret_cast<char*>(output.data()),
            static_cast<int>(size),
            static_cast<int>(bound),
            level_
        );
    }
    
    if (compressed_size <= 0) {
        return {};  // 压缩失败
    }
    
    output.resize(compressed_size);
    return output;
#else
    // 无 LZ4 时，直接返回原数据 (不添加长度前缀)
    return std::vector<uint8_t>(data, data + size);
#endif
}

std::vector<uint8_t> LZ4Compressor::decompress(
    const uint8_t* data, 
    size_t size,
    size_t original_size
) {
    if (size == 0 || original_size == 0) {
        return {};
    }
    
#if BINDIFF_HAS_LZ4
    std::vector<uint8_t> output(original_size);
    
    int decompressed_size = LZ4_decompress_safe(
        reinterpret_cast<const char*>(data),
        reinterpret_cast<char*>(output.data()),
        static_cast<int>(size),
        static_cast<int>(original_size)
    );
    
    if (decompressed_size < 0 || 
        static_cast<size_t>(decompressed_size) != original_size) {
        return {};  // 解压失败
    }
    
    return output;
#else
    // 无 LZ4 时，直接返回原数据
    // original_size 只是估计值，返回实际可用的数据
    return std::vector<uint8_t>(data, data + size);
#endif
}

// ============== 工厂函数 ==============

std::unique_ptr<Compressor> create_compressor(
    CompressionType type, 
    int level
) {
    switch (type) {
        case CompressionType::LZ4:
            return std::make_unique<LZ4Compressor>(level);
        case CompressionType::None:
        default:
            return nullptr;
    }
}

} // namespace bindiff
