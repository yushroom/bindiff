#include <iostream>
#include <vector>
#include <functional>
#include <string>

// 简单测试框架
struct Test {
    std::string name;
    std::function<bool()> fn;
};

static std::vector<Test>& get_tests() {
    static std::vector<Test> tests;
    return tests;
}

struct TestRegistrar {
    TestRegistrar(const char* name, std::function<bool()> fn) {
        get_tests().push_back({name, fn});
    }
};

#define TEST(name) \
    static TestRegistrar test_##name(#name, []() -> bool)

#define ASSERT(cond) \
    do { \
        if (!(cond)) { \
            std::cerr << "  失败: " << #cond << std::endl; \
            return false; \
        } \
    } while(0)

#define ASSERT_EQ(a, b) \
    do { \
        if ((a) != (b)) { \
            std::cerr << "  失败: " << #a << " != " << #b << std::endl; \
            return false; \
        } \
    } while(0)

int main() {
    std::cout << "=== Binary Diff 测试套件 ===" << std::endl;
    
    auto& tests = get_tests();
    int passed = 0;
    int failed = 0;
    
    for (const auto& test : tests) {
        std::cout << "测试: " << test.name << "... ";
        try {
            if (test.fn()) {
                std::cout << "✓ 通过" << std::endl;
                passed++;
            } else {
                std::cout << "✗ 失败" << std::endl;
                failed++;
            }
        } catch (const std::exception& e) {
            std::cout << "✗ 异常: " << e.what() << std::endl;
            failed++;
        }
    }
    
    std::cout << std::endl;
    std::cout << "总计: " << tests.size() << " 测试" << std::endl;
    std::cout << "通过: " << passed << std::endl;
    std::cout << "失败: " << failed << std::endl;
    
    return failed > 0 ? 1 : 0;
}

// 声明测试函数
extern void register_mmap_tests();
extern void register_matcher_tests();
extern void register_operations_tests();
extern void register_compress_tests();

// 注册所有测试
static struct TestInit {
    TestInit() {
        register_mmap_tests();
        register_matcher_tests();
        register_operations_tests();
        register_compress_tests();
    }
} test_init;
