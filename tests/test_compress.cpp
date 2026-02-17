#include <iostream>
#include <vector>
#include <random>

#include "compress/compressor.hpp"

TEST(compress_lz4_small) {
    bindiff::LZ4Compressor compressor(1);
    
    std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
    auto compressed = compressor.compress(data.data(), data.size());
    
    ASSERT(!compressed.empty());
    
    auto decompressed = compressor.decompress(
        compressed.data(), compressed.size(), data.size()
    );
    
    ASSERT(!decompressed.empty());
    ASSERT(decompressed.size() == data.size());
    
    for (size_t i = 0; i < data.size(); ++i) {
        ASSERT(decompressed[i] == data[i]);
    }
    
    return true;
}

TEST(compress_lz4_repeated) {
    bindiff::LZ4Compressor compressor(1);
    
    // 重复数据，压缩效果好
    std::vector<uint8_t> data(1024, 0xAB);
    auto compressed = compressor.compress(data.data(), data.size());
    
    ASSERT(!compressed.empty());
    // 压缩后应该远小于原数据
    ASSERT(compressed.size() < data.size() / 2);
    
    auto decompressed = compressor.decompress(
        compressed.data(), compressed.size(), data.size()
    );
    
    ASSERT(decompressed.size() == data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        ASSERT(decompressed[i] == 0xAB);
    }
    
    return true;
}

TEST(compress_lz4_random) {
    bindiff::LZ4Compressor compressor(1);
    
    // 随机数据，压缩效果差
    std::vector<uint8_t> data(1024);
    std::mt19937 rng(42);
    for (auto& b : data) {
        b = rng() & 0xFF;
    }
    
    auto compressed = compressor.compress(data.data(), data.size());
    
    ASSERT(!compressed.empty());
    
    auto decompressed = compressor.decompress(
        compressed.data(), compressed.size(), data.size()
    );
    
    ASSERT(decompressed.size() == data.size());
    for (size_t i = 0; i < data.size(); ++i) {
        ASSERT(decompressed[i] == data[i]);
    }
    
    return true;
}

TEST(compress_lz4_levels) {
    std::vector<uint8_t> data(4096);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }
    
    // 测试不同压缩级别
    for (int level : {1, 3, 6, 9}) {
        bindiff::LZ4Compressor compressor(level);
        
        auto compressed = compressor.compress(data.data(), data.size());
        ASSERT(!compressed.empty());
        
        auto decompressed = compressor.decompress(
            compressed.data(), compressed.size(), data.size()
        );
        
        ASSERT(decompressed.size() == data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            ASSERT(decompressed[i] == data[i]);
        }
    }
    
    return true;
}

TEST(compress_empty) {
    bindiff::LZ4Compressor compressor(1);
    
    std::vector<uint8_t> empty;
    auto compressed = compressor.compress(empty.data(), 0);
    
    // 空数据应该返回空
    ASSERT(compressed.empty());
    
    return true;
}

TEST(compress_bound) {
    size_t size = 1024;
    size_t bound = bindiff::LZ4Compressor::compress_bound(size);
    
    // 压缩后的最大大小应该 > 原大小
    ASSERT(bound >= size);
    
    // 实际测试
    bindiff::LZ4Compressor compressor(1);
    std::vector<uint8_t> data(size, 0xAB);
    auto compressed = compressor.compress(data.data(), size);
    
    ASSERT(compressed.size() <= bound);
    
    return true;
}

void register_compress_tests() {
    // 已通过 TEST 宏自动注册
}
