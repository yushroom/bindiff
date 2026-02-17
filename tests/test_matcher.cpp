#include <iostream>
#include <cstring>
#include <vector>

#include "core/matcher.hpp"

TEST(matcher_rolling_hash) {
    bindiff::RollingHash hash(32);
    
    // 测试数据
    std::vector<uint8_t> data(64);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i);
    }
    
    // 初始化
    hash.init(data.data(), 32);
    uint64_t h1 = hash.hash();
    ASSERT(h1 != 0);
    
    // 滚动
    hash.roll(data[0], data[32]);
    uint64_t h2 = hash.hash();
    
    // 手动计算第二个窗口的哈希
    bindiff::RollingHash hash2(32);
    hash2.init(data.data() + 1, 32);
    uint64_t h3 = hash2.hash();
    
    ASSERT(h2 == h3);
    
    return true;
}

TEST(matcher_find_match) {
    bindiff::BlockMatcher matcher(8);  // 最小匹配 8 字节
    
    // old: "ABCDEFGHxxxxABCDEFGH"
    // new: "ABCDEFGH"
    std::vector<uint8_t> old_data = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
        'x', 'x', 'x', 'x',
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'
    };
    std::vector<uint8_t> new_data = {
        'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'
    };
    
    auto match = matcher.find_longest_match(
        old_data.data(), old_data.size(),
        new_data.data(), new_data.size(),
        0
    );
    
    ASSERT(match.valid());
    ASSERT(match.length == 8);
    ASSERT(match.old_offset == 0);
    
    return true;
}

TEST(matcher_no_match) {
    bindiff::BlockMatcher matcher(8);
    
    std::vector<uint8_t> old_data = {'A', 'B', 'C', 'D'};
    std::vector<uint8_t> new_data = {'X', 'Y', 'Z', 'W'};
    
    auto match = matcher.find_longest_match(
        old_data.data(), old_data.size(),
        new_data.data(), new_data.size(),
        0
    );
    
    // 没有足够长的匹配
    ASSERT(!match.valid() || match.length < 8);
    
    return true;
}

TEST(matcher_partial_match) {
    bindiff::BlockMatcher matcher(4);
    
    // old: "ABCDxxxx"
    // new: "ABCD"
    std::vector<uint8_t> old_data = {
        'A', 'B', 'C', 'D', 'x', 'x', 'x', 'x'
    };
    std::vector<uint8_t> new_data = {
        'A', 'B', 'C', 'D'
    };
    
    auto match = matcher.find_longest_match(
        old_data.data(), old_data.size(),
        new_data.data(), new_data.size(),
        0
    );
    
    ASSERT(match.valid());
    ASSERT(match.length >= 4);
    
    return true;
}

void register_matcher_tests() {
    // 已通过 TEST 宏自动注册
}
