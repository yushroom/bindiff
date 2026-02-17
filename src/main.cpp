#include <iostream>
#include <string>
#include <cstring>
#include <bindiff.hpp>

void print_usage() {
    std::cout << R"(
Binary Diff/Patch Tool v1.0.0
针对大文件的二进制差分工具

用法:
  bindiff <command> [options]

命令:
  diff    创建补丁: diff <old_file> <new_file> <patch_file>
  patch   应用补丁: patch <old_file> <patch_file> <new_file>
  verify  验证补丁: verify <old_file> <new_file> <patch_file>
  info    查看信息: info <patch_file>

选项:
  -t, --threads <N>      线程数 (默认: 自动)
  -b, --block-size <MB>  块大小 MB (默认: 64)
  -c, --compress <0-12>  LZ4 压缩级别 (默认: 1)
  --no-verify           跳过校验
  --progress            显示进度条
  -v, --verbose         详细输出
  -h, --help            显示帮助

示例:
  bindiff diff old.pak new.pak patch.bdp --progress
  bindiff patch old.pak patch.bdp new.pak
  bindiff info patch.bdp

)" << std::endl;
}

// 简单进度回调
class ConsoleProgress : public bindiff::ProgressCallback {
public:
    void on_progress(float percent, const char* stage) override {
        int bar_width = 40;
        int pos = static_cast<int>(bar_width * percent);
        
        std::cout << "\r  [";
        for (int i = 0; i < bar_width; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << static_cast<int>(percent * 100) << "% " << stage;
        std::cout.flush();
    }
    
    void on_complete(const bindiff::Result& result) override {
        std::cout << std::endl;
        if (result.success) {
            std::cout << "  ✓ 完成" << std::endl;
            std::cout << "    用时: " << bindiff::format_duration(result.elapsed_seconds) << std::endl;
            std::cout << "    处理: " << bindiff::format_size(result.bytes_processed) << std::endl;
        }
    }
};

int cmd_diff(int argc, char* argv[]) {
    bindiff::DiffOptions options;
    bool show_progress = false;
    
    // 解析参数
    std::string old_file, new_file, patch_file;
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) {
                options.num_threads = std::stoi(argv[++i]);
            }
        } else if (arg == "-b" || arg == "--block-size") {
            if (i + 1 < argc) {
                options.block_size = std::stoi(argv[++i]) * 1024 * 1024;
            }
        } else if (arg == "-c" || arg == "--compress") {
            if (i + 1 < argc) {
                options.compression_level = std::stoi(argv[++i]);
            }
        } else if (arg == "--no-verify") {
            options.verify = false;
        } else if (arg == "--progress") {
            show_progress = true;
        } else if (arg[0] != '-') {
            if (old_file.empty()) old_file = arg;
            else if (new_file.empty()) new_file = arg;
            else if (patch_file.empty()) patch_file = arg;
        }
    }
    
    if (old_file.empty() || new_file.empty() || patch_file.empty()) {
        std::cerr << "错误: 需要指定 old_file, new_file, patch_file" << std::endl;
        return 1;
    }
    
    std::cout << "创建补丁:" << std::endl;
    std::cout << "  原文件: " << old_file << std::endl;
    std::cout << "  新文件: " << new_file << std::endl;
    std::cout << "  补丁:   " << patch_file << std::endl;
    
    ConsoleProgress progress;
    bindiff::ProgressCallback* callback = show_progress ? &progress : nullptr;
    
    auto result = bindiff::create_diff(old_file, new_file, patch_file, options, callback);
    
    if (!result.success) {
        std::cerr << "错误: " << result.error << std::endl;
        return 1;
    }
    
    if (!show_progress) {
        std::cout << "✓ 成功" << std::endl;
        std::cout << "  用时: " << bindiff::format_duration(result.elapsed_seconds) << std::endl;
    }
    
    // 显示补丁信息
    auto info = bindiff::get_patch_info(patch_file);
    std::cout << "  补丁大小: " << bindiff::format_size(info.patch_size) << std::endl;
    
    return 0;
}

int cmd_patch(int argc, char* argv[]) {
    bindiff::PatchOptions options;
    bool show_progress = false;
    
    std::string old_file, patch_file, new_file;
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--no-verify") {
            options.verify = false;
        } else if (arg == "--progress") {
            show_progress = true;
        } else if (arg[0] != '-') {
            if (old_file.empty()) old_file = arg;
            else if (patch_file.empty()) patch_file = arg;
            else if (new_file.empty()) new_file = arg;
        }
    }
    
    if (old_file.empty() || patch_file.empty() || new_file.empty()) {
        std::cerr << "错误: 需要指定 old_file, patch_file, new_file" << std::endl;
        return 1;
    }
    
    std::cout << "应用补丁:" << std::endl;
    std::cout << "  原文件: " << old_file << std::endl;
    std::cout << "  补丁:   " << patch_file << std::endl;
    std::cout << "  输出:   " << new_file << std::endl;
    
    ConsoleProgress progress;
    bindiff::ProgressCallback* callback = show_progress ? &progress : nullptr;
    
    auto result = bindiff::apply_patch(old_file, patch_file, new_file, options, callback);
    
    if (!result.success) {
        std::cerr << "错误: " << result.error << std::endl;
        return 1;
    }
    
    if (!show_progress) {
        std::cout << "✓ 成功" << std::endl;
        std::cout << "  用时: " << bindiff::format_duration(result.elapsed_seconds) << std::endl;
    }
    
    return 0;
}

int cmd_verify(int argc, char* argv[]) {
    std::string old_file, new_file, patch_file;
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] != '-') {
            if (old_file.empty()) old_file = arg;
            else if (new_file.empty()) new_file = arg;
            else if (patch_file.empty()) patch_file = arg;
        }
    }
    
    if (old_file.empty() || new_file.empty() || patch_file.empty()) {
        std::cerr << "错误: 需要指定 old_file, new_file, patch_file" << std::endl;
        return 1;
    }
    
    auto result = bindiff::verify_patch(old_file, new_file, patch_file);
    
    if (!result.success) {
        std::cerr << "验证失败: " << result.error << std::endl;
        return 1;
    }
    
    std::cout << "✓ 验证通过" << std::endl;
    return 0;
}

int cmd_info(int argc, char* argv[]) {
    std::string patch_file;
    
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] != '-') {
            if (patch_file.empty()) patch_file = arg;
        }
    }
    
    if (patch_file.empty()) {
        std::cerr << "错误: 需要指定 patch_file" << std::endl;
        return 1;
    }
    
    auto info = bindiff::get_patch_info(patch_file);
    
    if (info.version == 0) {
        std::cerr << "错误: 无效的补丁文件" << std::endl;
        return 1;
    }
    
    std::cout << "补丁文件: " << patch_file << std::endl;
    std::cout << "版本:     " << info.version << std::endl;
    std::cout << "块大小:   " << bindiff::format_size(info.block_size) << std::endl;
    std::cout << "原大小:   " << bindiff::format_size(info.old_size) << std::endl;
    std::cout << "新大小:   " << bindiff::format_size(info.new_size) << std::endl;
    std::cout << "块数量:   " << info.num_blocks << std::endl;
    std::cout << "补丁大小: " << bindiff::format_size(info.patch_size) << std::endl;
    
    float ratio = info.new_size > 0 ? 
        static_cast<float>(info.patch_size) / info.new_size * 100.0f : 0.0f;
    std::cout << "压缩比:   " << static_cast<int>(ratio) << "%" << std::endl;
    
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "-h" || command == "--help") {
        print_usage();
        return 0;
    }
    
    if (command == "diff") {
        return cmd_diff(argc, argv);
    }
    
    if (command == "patch") {
        return cmd_patch(argc, argv);
    }
    
    if (command == "verify") {
        return cmd_verify(argc, argv);
    }
    
    if (command == "info") {
        return cmd_info(argc, argv);
    }
    
    std::cerr << "未知命令: " << command << std::endl;
    std::cerr << "使用 --help 查看帮助" << std::endl;
    return 1;
}
