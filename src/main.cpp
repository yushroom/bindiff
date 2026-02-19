#include <iostream>
#include <string>
#include <cstring>
#include <bindiff.hpp>
#include <core/batch_processor.hpp>

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
  batch   批量处理: batch diff <old_dir> <new_dir> <output_dir>
                    batch patch <old_dir> <patch_dir> <output_dir>

选项:
  -t, --threads <N>      线程数 (默认: 自动)
  -b, --block-size <MB>  块大小 MB (默认: 64)
  -c, --compress <0-12>  LZ4 压缩级别 (默认: 1)
  -e, --extension <ext>  文件扩展名 (batch: 默认 .pak)
  --no-verify           跳过校验
  --progress            显示进度条
  -v, --verbose         详细输出
  -h, --help            显示帮助

示例:
  bindiff diff old.pak new.pak patch.bdp --progress
  bindiff patch old.pak patch.bdp new.pak
  bindiff info patch.bdp
  bindiff batch diff old_paks/ new_paks/ patches/ -t 8
  bindiff batch patch old_paks/ patches/ output_paks/ -t 8

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

// ============== Batch 命令实现 ==============

class BatchProgress : public bindiff::BatchProgressCallback {
public:
    void on_task_progress(const std::string& task_id, float percent, const char* stage) override {
        std::lock_guard<std::mutex> lock(mutex_);
        int bar_width = 30;
        int pos = static_cast<int>(bar_width * percent);
        
        std::cout << "\r  [" << task_id << "] [";
        for (int i = 0; i < bar_width; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << static_cast<int>(percent * 100) << "% " << stage;
        std::cout.flush();
    }
    
    void on_task_complete(const std::string& task_id, const bindiff::BatchTaskResult& result) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << std::endl;
        if (result.success) {
            std::cout << "  ✓ [" << task_id << "] "
                      << bindiff::format_size(result.patch_size) << " ("
                      << bindiff::format_duration(result.elapsed_seconds) << ")"
                      << std::endl;
        } else {
            std::cout << "  ✗ [" << task_id << "] " << result.error << std::endl;
        }
    }
    
    void on_batch_progress(size_t completed, size_t total) override {
        // 可选：显示整体进度
    }
    
    void on_batch_complete(const bindiff::BatchResult& result) override {
        std::cout << std::endl;
        std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
        std::cout << "批量处理完成" << std::endl;
        std::cout << "  成功: " << result.success_count << "/" << result.total_tasks << std::endl;
        if (result.failed_count > 0) {
            std::cout << "  失败: " << result.failed_count << std::endl;
        }
        std::cout << "  总用时: " << bindiff::format_duration(result.total_elapsed) << std::endl;
        std::cout << "  原文件总大小: " << bindiff::format_size(result.total_old_size) << std::endl;
        std::cout << "  新文件总大小: " << bindiff::format_size(result.total_new_size) << std::endl;
        std::cout << "  补丁总大小: " << bindiff::format_size(result.total_patch_size) << std::endl;
        
        if (result.total_new_size > 0) {
            float ratio = static_cast<float>(result.total_patch_size) / 
                         result.total_new_size * 100.0f;
            std::cout << "  压缩比: " << static_cast<int>(ratio) << "%" << std::endl;
        }
    }
    
private:
    std::mutex mutex_;
};

int cmd_batch_diff(int argc, char* argv[]) {
    bindiff::BatchOptions batch_options;
    bindiff::DiffOptions diff_options;
    std::string old_dir, new_dir, output_dir;
    std::string extension = ".pak";
    
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) {
                batch_options.num_threads = std::stoi(argv[++i]);
            }
        } else if (arg == "-b" || arg == "--block-size") {
            if (i + 1 < argc) {
                diff_options.block_size = std::stoi(argv[++i]) * 1024 * 1024;
            }
        } else if (arg == "-c" || arg == "--compress") {
            if (i + 1 < argc) {
                diff_options.compression_level = std::stoi(argv[++i]);
            }
        } else if (arg == "-e" || arg == "--extension") {
            if (i + 1 < argc) {
                extension = argv[++i];
                if (extension[0] != '.') extension = "." + extension;
            }
        } else if (arg == "--no-verify") {
            diff_options.verify = false;
        } else if (arg[0] != '-') {
            if (old_dir.empty()) old_dir = arg;
            else if (new_dir.empty()) new_dir = arg;
            else if (output_dir.empty()) output_dir = arg;
        }
    }
    
    if (old_dir.empty() || new_dir.empty() || output_dir.empty()) {
        std::cerr << "错误: 需要指定 old_dir, new_dir, output_dir" << std::endl;
        return 1;
    }
    
    std::cout << "批量创建补丁:" << std::endl;
    std::cout << "  原目录: " << old_dir << std::endl;
    std::cout << "  新目录: " << new_dir << std::endl;
    std::cout << "  输出目录: " << output_dir << std::endl;
    std::cout << "  扩展名: " << extension << std::endl;
    if (batch_options.num_threads > 0) {
        std::cout << "  并发数: " << batch_options.num_threads << std::endl;
    }
    std::cout << std::endl;
    
    // 生成任务列表
    auto tasks = bindiff::generate_diff_tasks(old_dir, new_dir, output_dir, extension);
    
    if (tasks.empty()) {
        std::cerr << "警告: 未找到匹配的文件对" << std::endl;
        return 0;
    }
    
    std::cout << "找到 " << tasks.size() << " 个文件对" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    
    // 执行批处理
    bindiff::BatchProcessor processor;
    processor.set_options(batch_options);
    
    BatchProgress progress;
    processor.set_callback(&progress);
    
    auto result = processor.create_diffs(tasks, diff_options);
    
    return result.success ? 0 : 1;
}

int cmd_batch_patch(int argc, char* argv[]) {
    bindiff::BatchOptions batch_options;
    bindiff::PatchOptions patch_options;
    std::string old_dir, patch_dir, output_dir;
    std::string extension = ".pak";
    
    for (int i = 3; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) {
                batch_options.num_threads = std::stoi(argv[++i]);
            }
        } else if (arg == "-e" || arg == "--extension") {
            if (i + 1 < argc) {
                extension = argv[++i];
                if (extension[0] != '.') extension = "." + extension;
            }
        } else if (arg == "--no-verify") {
            patch_options.verify = false;
        } else if (arg[0] != '-') {
            if (old_dir.empty()) old_dir = arg;
            else if (patch_dir.empty()) patch_dir = arg;
            else if (output_dir.empty()) output_dir = arg;
        }
    }
    
    if (old_dir.empty() || patch_dir.empty() || output_dir.empty()) {
        std::cerr << "错误: 需要指定 old_dir, patch_dir, output_dir" << std::endl;
        return 1;
    }
    
    std::cout << "批量应用补丁:" << std::endl;
    std::cout << "  原目录: " << old_dir << std::endl;
    std::cout << "  补丁目录: " << patch_dir << std::endl;
    std::cout << "  输出目录: " << output_dir << std::endl;
    std::cout << "  扩展名: " << extension << std::endl;
    if (batch_options.num_threads > 0) {
        std::cout << "  并发数: " << batch_options.num_threads << std::endl;
    }
    std::cout << std::endl;
    
    auto tasks = bindiff::generate_patch_tasks(old_dir, patch_dir, output_dir, extension);
    
    if (tasks.empty()) {
        std::cerr << "警告: 未找到匹配的文件对" << std::endl;
        return 0;
    }
    
    std::cout << "找到 " << tasks.size() << " 个补丁文件" << std::endl;
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" << std::endl;
    
    bindiff::BatchProcessor processor;
    processor.set_options(batch_options);
    
    BatchProgress progress;
    processor.set_callback(&progress);
    
    auto result = processor.apply_patches(tasks, patch_options);
    
    return result.success ? 0 : 1;
}

int cmd_batch(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "错误: 需要指定 batch 子命令 (diff/patch)" << std::endl;
        return 1;
    }
    
    std::string subcommand = argv[2];
    
    if (subcommand == "diff") {
        return cmd_batch_diff(argc, argv);
    }
    
    if (subcommand == "patch") {
        return cmd_batch_patch(argc, argv);
    }
    
    std::cerr << "未知 batch 子命令: " << subcommand << std::endl;
    return 1;
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
    
    if (command == "batch") {
        return cmd_batch(argc, argv);
    }
    
    std::cerr << "未知命令: " << command << std::endl;
    std::cerr << "使用 --help 查看帮助" << std::endl;
    return 1;
}
