#pragma once

#include "types.hpp"
#include <cstdint>
#include <vector>

namespace bindiff {

// ============== Rabin-Karp 滚动哈希 ==============

class RollingHash {
public:
    explicit RollingHash(size_t window_size = 32);
    ~RollingHash() = default;
    
    // 初始化 (传入前 window_size 个字节)
    void init(const byte* data, size_t size);
    
    // 滑动窗口: 移除 oldest，添加 newest
    void roll(byte oldest, byte newest);
    
    // 获取当前哈希值
    uint64_t hash() const { return hash_; }
    
    // 重置
    void reset();
    
    // 静态方法: 计算一段数据的哈希
    static uint64_t compute(const byte* data, size_t size);

private:
    uint64_t hash_;
    uint64_t pow_base_;  // BASE^window_size_ mod MOD
    size_t window_size_;
    
    static constexpr uint64_t BASE = 257;
    static constexpr uint64_t MOD = (1ULL << 61) - 1;  // Mersenne prime
    
    static uint64_t mul_mod(uint64_t a, uint64_t b);
    static uint64_t add_mod(uint64_t a, uint64_t b);
};

// ============== 块匹配器 ==============

struct Match {
    size_t old_offset = 0;
    size_t length = 0;
    
    bool valid() const { return length > 0; }
};

class BlockMatcher {
public:
    explicit BlockMatcher(size_t min_match = 32);
    ~BlockMatcher() = default;
    
    // 在 old 中找 new_data 的最长匹配
    Match find_longest_match(
        const byte* old_data, 
        size_t old_size,
        const byte* new_data, 
        size_t new_size,
        size_t new_offset  // 从 new_data 的哪个位置开始匹配
    );
    
    // 批量匹配: 找到所有匹配点
    std::vector<Match> find_all_matches(
        const byte* old_data,
        size_t old_size,
        const byte* new_data,
        size_t new_size
    );
    
    // 构建哈希索引 (加速匹配)
    void build_index(const byte* data, size_t size, size_t chunk_size = 32);

private:
    size_t min_match_;
    RollingHash hasher_;
    
    // 哈希表: hash -> offset
    std::vector<std::vector<size_t>> hash_table_;
    static constexpr size_t HASH_BUCKETS = 65536;
    
    uint64_t compute_chunk_hash(const byte* data, size_t size);
    size_t hash_to_bucket(uint64_t hash) const;
};

} // namespace bindiff
