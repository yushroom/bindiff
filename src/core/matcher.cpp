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
    uint64_t sub = mul_mod(static_cast<uint64_t>(oldest), pow_base_);
    hash_ = add_mod(mul_mod(hash_, BASE) + newest + MOD - sub, 0);
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
    
    if (new_offset + min_match_ > new_size) {
        return match;
    }
    
    // 如果有索引，使用哈希表快速查找
    if (!hash_table_[0].empty()) {
        // 计算当前窗口的哈希
        hasher_.init(new_data + new_offset, min_match_);
        uint64_t target_hash = hasher_.hash();
        size_t bucket = hash_to_bucket(target_hash);
        
        // 检查哈希表中的候选
        for (size_t old_pos : hash_table_[bucket]) {
            // 验证匹配
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
        // 限制搜索范围避免太慢
        size_t max_search = std::min(old_size, static_cast<size_t>(1000000));  // 最多搜索 1MB
        
        size_t best_offset = 0;
        size_t best_length = 0;
        
        // 使用哈希快速跳过不匹配的位置
        hasher_.init(new_data + new_offset, min_match_);
        uint64_t target_hash = hasher_.hash();
        
        RollingHash old_hasher(min_match_);
        
        for (size_t i = 0; i <= old_size - min_match_ && i < max_search; ) {
            // 计算当前位置的哈希
            if (i == 0) {
                old_hasher.init(old_data, min_match_);
            } else {
                old_hasher.roll(old_data[i - 1], old_data[i + min_match_ - 1]);
            }
            
            // 哈希匹配才验证
            if (old_hasher.hash() == target_hash) {
                // 扩展匹配
                size_t len = 0;
                while (len < old_size - i && 
                       len < new_size - new_offset &&
                       old_data[i + len] == new_data[new_offset + len]) {
                    ++len;
                }
                
                if (len >= min_match_ && len > best_length) {
                    best_length = len;
                    best_offset = i;
                    
                    // 如果找到足够长的匹配，提前退出
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
    
    // 先构建索引
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
    // 清空旧索引
    for (auto& bucket : hash_table_) {
        bucket.clear();
    }
    
    if (size < chunk_size) return;
    
    // 构建新索引
    RollingHash hasher(chunk_size);
    hasher.init(data, chunk_size);
    
    // 存储第一个位置
    uint64_t hash = hasher.hash();
    size_t bucket = hash_to_bucket(hash);
    hash_table_[bucket].push_back(0);
    
    // 滑动窗口
    for (size_t i = 1; i + chunk_size <= size; ++i) {
        hasher.roll(data[i - 1], data[i + chunk_size - 1]);
        
        hash = hasher.hash();
        bucket = hash_to_bucket(hash);
        
        // 限制每个桶的大小避免冲突太多
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
