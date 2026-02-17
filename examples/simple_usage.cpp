#include <iostream>
#include <vector>
#include <bindiff.hpp>

// 创建测试文件
void create_test_file(const std::string& path, const std::vector<uint8_t>& data) {
    FILE* f = fopen(path.c_str(), "wb");
    fwrite(data.data(), 1, data.size(), f);
    fclose(f);
}

int main() {
    std::cout << "=== Binary Diff 示例 ===" << std::endl;
    
    // 创建测试文件
    std::cout << "1. 创建测试文件..." << std::endl;
    
    // old.bin: "Hello World!"
    std::vector<uint8_t> old_data = {
        'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!'
    };
    
    // new.bin: "Hello OpenClaw!"
    std::vector<uint8_t> new_data = {
        'H', 'e', 'l', 'l', 'o', ' ', 'O', 'p', 'e', 'n', 'C', 'l', 'a', 'w', '!'
    };
    
    create_test_file("/tmp/old.bin", old_data);
    create_test_file("/tmp/new.bin", new_data);
    
    std::cout << "   old.bin: " << old_data.size() << " bytes" << std::endl;
    std::cout << "   new.bin: " << new_data.size() << " bytes" << std::endl;
    
    // 创建 diff
    std::cout << "\n2. 创建 diff..." << std::endl;
    
    bindiff::DiffOptions options;
    options.block_size = 1024;
    options.num_threads = 1;
    
    auto result = bindiff::create_diff(
        "/tmp/old.bin",
        "/tmp/new.bin",
        "/tmp/patch.bdp",
        options
    );
    
    if (result.success) {
        std::cout << "   ✓ 成功" << std::endl;
        std::cout << "   用时: " << result.elapsed_seconds << "s" << std::endl;
    } else {
        std::cout << "   ✗ 失败: " << result.error << std::endl;
        return 1;
    }
    
    // 应用 patch
    std::cout << "\n3. 应用 patch..." << std::endl;
    
    auto patch_result = bindiff::apply_patch(
        "/tmp/old.bin",
        "/tmp/patch.bdp",
        "/tmp/output.bin"
    );
    
    if (patch_result.success) {
        std::cout << "   ✓ 成功" << std::endl;
    } else {
        std::cout << "   ✗ 失败: " << patch_result.error << std::endl;
        return 1;
    }
    
    // 验证
    std::cout << "\n4. 验证结果..." << std::endl;
    
    FILE* f = fopen("/tmp/output.bin", "rb");
    if (f) {
        std::vector<uint8_t> output(new_data.size());
        fread(output.data(), 1, output.size(), f);
        fclose(f);
        
        if (output == new_data) {
            std::cout << "   ✓ 输出与预期一致" << std::endl;
        } else {
            std::cout << "   ✗ 输出不匹配" << std::endl;
            return 1;
        }
    }
    
    // 查看 patch 信息
    std::cout << "\n5. Patch 信息:" << std::endl;
    
    auto info = bindiff::get_patch_info("/tmp/patch.bdp");
    std::cout << "   Old size: " << bindiff::format_size(info.old_size) << std::endl;
    std::cout << "   New size: " << bindiff::format_size(info.new_size) << std::endl;
    std::cout << "   Patch size: " << bindiff::format_size(info.patch_size) << std::endl;
    
    std::cout << "\n完成!" << std::endl;
    return 0;
}
