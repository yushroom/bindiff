// 示例: 使用 bindiff 库创建和应用补丁

#include <bindiff.hpp>
#include <iostream>

// 自定义进度回调
class MyProgress : public bindiff::ProgressCallback {
public:
    void on_progress(float percent, const char* stage) override {
        std::cout << "\r进度: " << static_cast<int>(percent * 100) << "% - " << stage;
        std::cout.flush();
    }
    
    void on_complete(const bindiff::Result& result) override {
        std::cout << std::endl;
        if (result.success) {
            std::cout << "完成! 用时: " << result.elapsed_seconds << "s" << std::endl;
        }
    }
};

int main() {
    std::cout << "=== Binary Diff 示例 ===" << std::endl;
    
    // 配置选项
    bindiff::DiffOptions diff_options;
    diff_options.block_size = 64 * 1024 * 1024;  // 64MB
    diff_options.compression_level = 1;          // LZ4 快速压缩
    diff_options.num_threads = 0;                // 自动检测线程数
    diff_options.verify = true;                  // 启用 SHA256 校验
    
    // 创建补丁
    std::cout << "\n1. 创建补丁..." << std::endl;
    
    MyProgress progress;
    auto diff_result = bindiff::create_diff(
        "old.pak",
        "new.pak", 
        "patch.bdp",
        diff_options,
        &progress
    );
    
    if (!diff_result.success) {
        std::cerr << "创建补丁失败: " << diff_result.error << std::endl;
        return 1;
    }
    
    // 查看补丁信息
    std::cout << "\n2. 补丁信息:" << std::endl;
    
    auto info = bindiff::get_patch_info("patch.bdp");
    std::cout << "  原文件大小: " << bindiff::format_size(info.old_size) << std::endl;
    std::cout << "  新文件大小: " << bindiff::format_size(info.new_size) << std::endl;
    std::cout << "  补丁大小: " << bindiff::format_size(info.patch_size) << std::endl;
    std::cout << "  块数量: " << info.num_blocks << std::endl;
    
    // 应用补丁
    std::cout << "\n3. 应用补丁..." << std::endl;
    
    bindiff::PatchOptions patch_options;
    patch_options.verify = true;
    
    auto patch_result = bindiff::apply_patch(
        "old.pak",
        "patch.bdp",
        "output.pak",
        patch_options,
        &progress
    );
    
    if (!patch_result.success) {
        std::cerr << "应用补丁失败: " << patch_result.error << std::endl;
        return 1;
    }
    
    // 验证
    std::cout << "\n4. 验证补丁..." << std::endl;
    
    auto verify_result = bindiff::verify_patch("old.pak", "new.pak", "patch.bdp");
    if (verify_result.success) {
        std::cout << "验证通过!" << std::endl;
    }
    
    std::cout << "\n完成!" << std::endl;
    return 0;
}
