#include "core/matcher.hpp"
#include <algorithm>
#include <cstring>
#include <thread>

#ifdef __SSE2__
#include <emmintrin.h>
#endif

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
        
        // 优化：优先检查靠近 new_offset 的位置
        // 并在找到足够长的匹配后提前退出
        for (size_t old_pos : hash_table_[bucket]) {
            // 快速检查前几个字节是否匹配
            if (old_data[old_pos] != new_data[new_offset] ||
                old_data[old_pos + 1] != new_data[new_offset + 1]) {
                continue;
            }
            
            // SIMD 优化：批量比较字节
            size_t len = 0;
            const size_t simd_width = 16;
            
            // 先用 SIMD 比较前 16 字节
            #ifdef __SSE2__
            if (new_size - new_offset >= simd_width && old_size - old_pos >= simd_width) {
                __m128i v_old = _mm_loadu_si128(reinterpret_cast<const __m128i*>(old_data + old_pos));
                __m128i v_new = _mm_loadu_si128(reinterpret_cast<const __m128i*>(new_data + new_offset));
                __m128i v_cmp = _mm_cmpeq_epi8(v_old, v_new);
                int mask = _mm_movemask_epi8(v_cmp);
                
                if (mask != 0xFFFF) {
                    // 前 16 字节有不匹配，跳过
                    continue;
                }
                len = simd_width;
            }
            #endif
            
            // 继续逐字节比较剩余部分
            while (len < old_size - old_pos && 
                   len < new_size - new_offset &&
                   old_data[old_pos + len] == new_data[new_offset + len]) {
                ++len;
            }
            
            if (len >= min_match_ && len > match.length) {
                match.old_offset = old_pos;
                match.length = len;
                
                // 优化：如果找到长匹配，提前退出
                if (len >= 4096) {
                    break;  // 4KB 以上匹配足够好
                }
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
    
    // 优化：每隔一定步长采样，而不是每个位置都建索引
    // 对于大文件，减少索引大小，降低内存占用和冲突
    size_t step = 1;
    if (size > 100 * 1024 * 1024) {  // > 100MB
        step = 4;  // 每 4 字节采样一次
    }
    if (size > 1024 * 1024 * 1024) {  // > 1GB
        step = 8;  // 每 8 字节采样一次
    }
    
    for (size_t i = 1; i + chunk_size <= size; i += step) {
        // 滚动计算哈希（根据步长调整）
        for (size_t j = 1; j <= step && i + chunk_size <= size; ++j, ++i) {
            hasher.roll(data[i - 1], data[i + chunk_size - 1]);
        }
        
        hash = hasher.hash();
        bucket = hash_to_bucket(hash);
        
        if (hash_table_[bucket].size() < 200) {  // 增加桶容量
            hash_table_[bucket].push_back(i);
        }
    }
}

void BlockMatcher::build_index_parallel(const byte* data, size_t size, size_t chunk_size, int num_threads) {
    // 单线程版本先清空
    for (auto& bucket : hash_table_) {
        bucket.clear();
    }
    
    if (size < chunk_size) return;
    
    // 确定线程数
    if (num_threads <= 0) {
        num_threads = static_cast<int>(std::thread::hardware_concurrency());
        if (num_threads <= 0) num_threads = 4;
    }
    
    // 确定采样步长
    size_t step = 1;
    if (size > 100 * 1024 * 1024) step = 4;
    if (size > 1024 * 1024 * 1024) step = 8;
    
    // 分块处理
    size_t chunk_size_bytes = size / num_threads;
    std::vector<std::vector<std::pair<uint64_t, size_t>>> local_results(num_threads);
    
    // 并行计算哈希
    std::vector<std::thread> threads;
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&, t]() {
            size_t start = t * chunk_size_bytes;
            size_t end = (t == num_threads - 1) ? size : (t + 1) * chunk_size_bytes;
            if (end + chunk_size > size) end = size - chunk_size;
            
            auto& results = local_results[t];
            results.reserve((end - start) / step / num_threads);
            
            RollingHash hasher(chunk_size);
            
            for (size_t i = start; i < end; i += step) {
                if (i + chunk_size > size) break;
                
                if (i == start) {
                    hasher.init(data + i, chunk_size);
                } else {
                    // 滚动计算
                    for (size_t j = 0; j < step && i + j + chunk_size <= size; ++j) {
                        hasher.roll(data[i + j - 1], data[i + j + chunk_size - 1]);
                    }
                }
                
                results.emplace_back(hasher.hash(), i);
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 合并结果到哈希表
    for (const auto& local : local_results) {
        for (const auto& [hash, offset] : local) {
            size_t bucket = hash_to_bucket(hash);
            if (hash_table_[bucket].size() < 200) {
                hash_table_[bucket].push_back(offset);
            }
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
