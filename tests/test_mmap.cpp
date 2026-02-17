#include <iostream>
#include <cstring>
#include <vector>

// 手动包含需要的头文件
#include "io/mmap_file.hpp"
#include "io/file_utils.hpp"

// 测试注册函数
void register_mmap_tests();

// 测试辅助: 创建临时测试文件
static bool create_test_file(const std::string& path, const std::vector<uint8_t>& data) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return true;
}

TEST(mmap_open_read) {
    // 创建测试文件
    std::string path = "/tmp/bindiff_test_mmap.bin";
    std::vector<uint8_t> data = {1, 2, 3, 4, 5, 6, 7, 8};
    
    ASSERT(create_test_file(path, data));
    
    // 打开并映射
    bindiff::MMapFile file;
    ASSERT(file.open(path));
    ASSERT(file.is_open());
    ASSERT(file.size() == 8);
    
    // 验证数据
    const uint8_t* ptr = file.data();
    ASSERT(ptr != nullptr);
    ASSERT(std::memcmp(ptr, data.data(), 8) == 0);
    
    file.close();
    ASSERT(!file.is_open());
    
    // 清理
    bindiff::delete_file(path);
    return true;
}

TEST(mmap_view) {
    std::string path = "/tmp/bindiff_test_view.bin";
    std::vector<uint8_t> data(1024);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i % 256);
    }
    
    ASSERT(create_test_file(path, data));
    
    bindiff::MMapFile file;
    ASSERT(file.open(path));
    
    // 获取部分视图
    auto view = file.view(100, 50);
    ASSERT(view.first != nullptr);
    ASSERT(view.second == 50);
    
    // 验证数据
    for (size_t i = 0; i < 50; ++i) {
        ASSERT(view.first[i] == static_cast<uint8_t>((100 + i) % 256));
    }
    
    file.close();
    bindiff::delete_file(path);
    return true;
}

TEST(mmap_large_file) {
    // 测试大文件 (1MB)
    std::string path = "/tmp/bindiff_test_large.bin";
    std::vector<uint8_t> data(1024 * 1024);
    for (size_t i = 0; i < data.size(); ++i) {
        data[i] = static_cast<uint8_t>(i & 0xFF);
    }
    
    ASSERT(create_test_file(path, data));
    
    bindiff::MMapFile file;
    ASSERT(file.open(path));
    ASSERT(file.size() == 1024 * 1024);
    
    // 随机访问测试
    const uint8_t* ptr = file.data();
    ASSERT(ptr[0] == 0);
    ASSERT(ptr[255] == 255);
    ASSERT(ptr[256] == 0);
    ASSERT(ptr[1024 * 1024 - 1] == 255);
    
    file.close();
    bindiff::delete_file(path);
    return true;
}

TEST(file_utils) {
    std::string path = "/tmp/bindiff_test_utils.bin";
    std::vector<uint8_t> data = {1, 2, 3};
    ASSERT(create_test_file(path, data));
    
    // file_exists
    ASSERT(bindiff::file_exists(path));
    ASSERT(!bindiff::file_exists("/tmp/nonexistent_file_xyz"));
    
    // get_file_size
    ASSERT(bindiff::get_file_size(path) == 3);
    
    // get_filename
    ASSERT(bindiff::get_filename(path) == "bindiff_test_utils.bin");
    
    // get_extension
    ASSERT(bindiff::get_extension(path) == ".bin");
    
    // delete_file
    ASSERT(bindiff::delete_file(path));
    ASSERT(!bindiff::file_exists(path));
    
    return true;
}

void register_mmap_tests() {
    // 已通过 TEST 宏自动注册
}
