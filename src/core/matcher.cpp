#include "core/matcher.hpp"
#include <algorithm>

namespace bindiff {

// ============== RollingHash 实现 ==============

RollingHash::RollingHash(size_t window_size)
    : hash_(0)
    , pow_base_(1)
    , window_size_(window_size)
{
    // 计算 BASE^window_size mod MOD
    for (size_t i = 0; i < window_size; ++i) {
        pow_base_ = mul_mod(pow_base_, BASE);
    }
}

void RollingHash::init(const byte* data, size_t size) {
    hash_ = 0;
    for (size_t i = 0; i < size && i < window_size_; ++i) {
        hash_ = add_mod(mul_mod(hash_, BASE), data[i]);
    }
}

void RollingHash::roll(byte oldest, byte newest) {
    // hash = (hash * BASE - oldest * pow_base + newest) mod MOD
    uint64_t sub = mul_mod(static_cast<uint64_t>(oldest), pow_base_);
    hash_ = add_mod(hash_ * BASE - sub + newest + MOD, 0);
}

void RollingHash::reset() {
    hash_ = 0;
}

uint64_t RollingHash::compute(const byte* data, size_t size) {
    RollingHash rh(size);
    rh.init(data, size);
    return rh.hash();
}

uint64_t RollingHash::mul_mod(uint64_t a, uint64_t b) {
    // 使用 128 位乘法避免溢出
    __uint128_t res = static_cast<__uint128_t>(a) * b;
    return static_cast<uint64_t>(res % MOD);
}

uint64_t RollingHash::add_mod(uint64_t a, uint64_t b) {
    uint64_t sum = a + b;
    if (sum >= MOD) sum -= MOD;
    return sum;
}

// ============== BlockMatcher 实现 ==============

BlockMatcher::BlockMatcher(size_t min_match)
    : min_match_(min_match)
    , hasher_(min_match)
{
    hash_table_.resize(HASH_BUCKETS);
}

Match BlockMatcher::find_longest_match(
    const byte* old_data, 
    size_t old_size,
    const byte* new_data, 
    size_t new_size,
    size_t new_offset
) {
    Match match;
    
    // TODO: 使用哈希表加速
    // 简单实现: 暴力搜索
    
    if (new_offset + min_match_ > new_size) {
        return match;
    }
    
    size_t best_offset = 0;
    size_t best_length = 0;
    
    // 滑动窗口搜索
    for (size_t i = 0; i <= old_size - min_match_; ++i) {
        // 找匹配起点
        if (old_data[i] != new_data[new_offset]) continue;
        
        // 扩展匹配
        size_t len = 0;
        while (len < old_size - i && 
               len < new_size - new_offset &&
               old_data[i + len] == new_data[new_offset + len]) {
            ++len;
        }
        
        if (len > best_length) {
            best_length = len;
            best_offset = i;
        }
    }
    
    if (best_length >= min_match_) {
        match.old_offset = best_offset;
        match.length = best_length;
    }
    
    return match;
}

std::vector<Match> BlockMatcher::find_all_matches(
    const byte* old_data,
    size_t old_size,
    const byte* new_data,
    size_t new_size
) {
    std::vector<Match> matches;
    
    // TODO: 实现
    // 使用滚动哈希 + 哈希表
    
    return matches;
}

void BlockMatcher::build_index(const byte* data, size_t size, size_t chunk_size) {
    // 清空旧索引
    for (auto& bucket : hash_table_) {
        bucket.clear();
    }
    
    // 构建新索引
    for (size_t i = 0; i + chunk_size <= size; i += chunk_size / 2) {
        uint64_t hash = compute_chunk_hash(data + i, chunk_size);
        size_t bucket = hash_to_bucket(hash);
        hash_table_[bucket].push_back(i);
    }
}

uint64_t BlockMatcher::compute_chunk_hash(const byte* data, size_t size) {
    return RollingHash::compute(data, size);
}

size_t BlockMatcher::hash_to_bucket(uint64_t hash) const {
    return hash % HASH_BUCKETS;
}

} // namespace bindiff
