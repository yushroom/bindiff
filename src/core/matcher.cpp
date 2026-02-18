#include "core/matcher.hpp"
#include <algorithm>
#include <cstring>

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
    size_t len = std::min(size, window_size_);
    for (size_t i = 0; i < len; ++i) {
        hash_ = add_mod(mul_mod(hash_, BASE), data[i]);
    }
}

void RollingHash::roll(byte oldest, byte newest) {
    // hash = (hash * BASE - oldest * pow_base + newest) mod MOD
    hash_ = mul_mod(hash_, BASE);
    hash_ = add_mod(hash_, newest);
    
    uint64_t sub = mul_mod(static_cast<uint64_t>(oldest), pow_base_);
    if (hash_ >= sub) {
        hash_ -= sub;
    } else {
        hash_ = hash_ + MOD - sub;
    }
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
    // 跨平台 128 位乘法
#ifdef _MSC_VER
    // MSVC: 使用 __umulh 和内置乘法
    uint64_t a_lo = a & 0xFFFFFFFF;
    uint64_t a_hi = a >> 32;
    uint64_t b_lo = b & 0xFFFFFFFF;
    uint64_t b_hi = b >> 32;
    
    uint64_t p0 = a_lo * b_lo;
    uint64_t p1 = a_lo * b_hi;
    uint64_t p2 = a_hi * b_lo;
    uint64_t p3 = a_hi * b_hi;
    
    uint64_t cy = ((p0 >> 32) + (p1 & 0xFFFFFFFF) + (p2 & 0xFFFFFFFF)) >> 32;
    
    uint64_t lo = p0;
    uint64_t hi = p3 + (p1 >> 32) + (p2 >> 32) + cy;
    
    // 对于 MOD = 2^61 - 1，使用 Montgomery reduction
    // 简化：直接使用 64 位模
    if (hi == 0) {
        return lo % MOD;
    }
    
    // 分解: (hi * 2^64 + lo) mod (2^61 - 1)
    // = (hi mod MOD * (2^64 mod MOD) + lo mod MOD) mod MOD
    uint64_t hi_mod = hi % MOD;
    // 2^64 mod (2^61 - 1) = 2^3 = 8 (因为 2^61 ≡ 1)
    uint64_t result = add_mod(mul_mod(hi_mod, 8), lo % MOD);
    return result;
#else
    // GCC/Clang: 使用 __uint128_t
    __uint128_t res = static_cast<__uint128_t>(a) * b;
    return static_cast<uint64_t>(res % MOD);
#endif
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
    
    if (new_offset + min_match_ > new_size) {
        return match;
    }
    
    // 如果有索引，使用哈希表快速查找
    if (!hash_table_[0].empty()) {
        hasher_.init(new_data + new_offset, min_match_);
        uint64_t target_hash = hasher_.hash();
        size_t bucket = hash_to_bucket(target_hash);
        
        for (size_t old_pos : hash_table_[bucket]) {
            size_t len = 0;
            while (len < old_size - old_pos && 
                   len < new_size - new_offset &&
                   old_data[old_pos + len] == new_data[new_offset + len]) {
                ++len;
            }
            
            if (len >= min_match_ && len > match.length) {
                match.old_offset = old_pos;
                match.length = len;
            }
        }
    } else {
        // 无索引，使用快速滑动窗口搜索
        // 动态调整搜索范围：对于大文件，搜索前 10MB 或 10% 的数据
        size_t max_search = std::min(
            old_size, 
            std::max(
                static_cast<size_t>(1000000),  // 至少 1MB
                std::min(
                    old_size / 10,              // 最多搜索 10% 的数据
                    static_cast<size_t>(10000000)  // 但不超过 10MB
                )
            )
        );
        
        size_t best_offset = 0;
        size_t best_length = 0;
        
        hasher_.init(new_data + new_offset, min_match_);
        uint64_t target_hash = hasher_.hash();
        
        RollingHash old_hasher(min_match_);
        
        for (size_t i = 0; i <= old_size - min_match_ && i < max_search; ) {
            if (i == 0) {
                old_hasher.init(old_data, min_match_);
            } else {
                old_hasher.roll(old_data[i - 1], old_data[i + min_match_ - 1]);
            }
            
            if (old_hasher.hash() == target_hash) {
                size_t len = 0;
                while (len < old_size - i && 
                       len < new_size - new_offset &&
                       old_data[i + len] == new_data[new_offset + len]) {
                    ++len;
                }
                
                if (len >= min_match_ && len > best_length) {
                    best_length = len;
                    best_offset = i;
                    
                    if (best_length >= 1024) break;
                }
            }
            
            ++i;
        }
        
        if (best_length >= min_match_) {
            match.old_offset = best_offset;
            match.length = best_length;
        }
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
    
    build_index(old_data, old_size, min_match_);
    
    size_t pos = 0;
    while (pos + min_match_ <= new_size) {
        auto match = find_longest_match(old_data, old_size, new_data, new_size, pos);
        
        if (match.valid()) {
            matches.push_back(match);
            pos += match.length;
        } else {
            ++pos;
        }
    }
    
    return matches;
}

void BlockMatcher::build_index(const byte* data, size_t size, size_t chunk_size) {
    for (auto& bucket : hash_table_) {
        bucket.clear();
    }
    
    if (size < chunk_size) return;
    
    RollingHash hasher(chunk_size);
    hasher.init(data, chunk_size);
    
    uint64_t hash = hasher.hash();
    size_t bucket = hash_to_bucket(hash);
    hash_table_[bucket].push_back(0);
    
    for (size_t i = 1; i + chunk_size <= size; ++i) {
        hasher.roll(data[i - 1], data[i + chunk_size - 1]);
        
        hash = hasher.hash();
        bucket = hash_to_bucket(hash);
        
        if (hash_table_[bucket].size() < 100) {
            hash_table_[bucket].push_back(i);
        }
    }
}

uint64_t BlockMatcher::compute_chunk_hash(const byte* data, size_t size) {
    return RollingHash::compute(data, size);
}

size_t BlockMatcher::hash_to_bucket(uint64_t hash) const {
    return static_cast<size_t>(hash % HASH_BUCKETS);
}

} // namespace bindiff
