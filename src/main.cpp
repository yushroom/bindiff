#include <iostream>
#include <string>
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
        std::cout << "diff 命令: 待实现..." << std::endl;
        // TODO: 实现参数解析和调用 create_diff
        return 0;
    }
    
    if (command == "patch") {
        std::cout << "patch 命令: 待实现..." << std::endl;
        // TODO: 实现参数解析和调用 apply_patch
        return 0;
    }
    
    if (command == "verify") {
        std::cout << "verify 命令: 待实现..." << std::endl;
        // TODO: 实现参数解析和调用 verify_patch
        return 0;
    }
    
    if (command == "info") {
        std::cout << "info 命令: 待实现..." << std::endl;
        // TODO: 实现参数解析和调用 get_patch_info
        return 0;
    }
    
    std::cerr << "未知命令: " << command << std::endl;
    std::cerr << "使用 --help 查看帮助" << std::endl;
    return 1;
}
