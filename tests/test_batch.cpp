#include <cstdio>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <cassert>
#include "core/batch_processor.hpp"
#include "bindiff.hpp"

namespace fs = std::filesystem;

// 测试辅助函数
void create_test_file(const std::string& path, size_t size, uint8_t pattern = 0xAA) {
    std::ofstream ofs(path, std::ios::binary);
    std::vector<uint8_t> data(size, pattern);
    ofs.write(reinterpret_cast<const char*>(data.data()), size);
}

void modify_test_file(const std::string& path, size_t offset, size_t size) {
    std::fstream fs(path, std::ios::in | std::ios::out | std::ios::binary);
    fs.seekp(offset);
    std::vector<uint8_t> data(size, 0xBB);
    fs.write(reinterpret_cast<const char*>(data.data()), size);
}

// 测试：生成 diff 任务列表
void test_generate_diff_tasks() {
    printf("测试: generate_diff_tasks... ");

    // 创建测试目录
    fs::create_directories("test_batch_tmp/old");
    fs::create_directories("test_batch_tmp/new");

    // 创建测试文件
    create_test_file("test_batch_tmp/old/file1.pak", 1024);
    create_test_file("test_batch_tmp/old/file2.pak", 2048);
    create_test_file("test_batch_tmp/old/file3.dat", 512);  // 不同扩展名
    create_test_file("test_batch_tmp/new/file1.pak", 1024);
    create_test_file("test_batch_tmp/new/file2.pak", 2048);
    // file3.dat 在 new 目录中不存在

    // 生成任务（默认扩展名 .pak）
    auto tasks = bindiff::generate_diff_tasks(
        "test_batch_tmp/old",
        "test_batch_tmp/new",
        "test_batch_tmp/patches",
        ".pak"
    );

    // 验证
    assert(tasks.size() == 2);  // 只匹配 .pak 文件
    assert(tasks[0].old_path.find("file1.pak") != std::string::npos);
    assert(tasks[1].old_path.find("file2.pak") != std::string::npos);
    assert(tasks[0].patch_path.find(".bdp") != std::string::npos);

    // 清理
    fs::remove_all("test_batch_tmp");

    printf("✓\n");
}

// 测试：生成 patch 任务列表
void test_generate_patch_tasks() {
    printf("测试: generate_patch_tasks... ");

    // 创建测试目录
    fs::create_directories("test_batch_tmp/old");
    fs::create_directories("test_batch_tmp/patches");

    // 创建测试文件
    create_test_file("test_batch_tmp/old/file1.pak", 1024);
    create_test_file("test_batch_tmp/old/file2.pak", 2048);
    create_test_file("test_batch_tmp/patches/file1.pak.bdp", 512);
    create_test_file("test_batch_tmp/patches/file2.pak.bdp", 256);

    // 生成任务
    auto tasks = bindiff::generate_patch_tasks(
        "test_batch_tmp/old",
        "test_batch_tmp/patches",
        "test_batch_tmp/output",
        ".pak"
    );

    // 验证
    assert(tasks.size() == 2);
    assert(tasks[0].old_path.find("file1.pak") != std::string::npos);
    assert(tasks[0].patch_path.find("file1.pak.bdp") != std::string::npos);

    // 清理
    fs::remove_all("test_batch_tmp");

    printf("✓\n");
}

// 测试：批量 diff
void test_batch_diff() {
    printf("测试: batch diff... ");

    // 创建测试目录
    fs::create_directories("test_batch_tmp/old");
    fs::create_directories("test_batch_tmp/new");
    fs::create_directories("test_batch_tmp/patches");

    // 创建测试文件（小文件以加快测试）
    create_test_file("test_batch_tmp/old/a.pak", 4096, 0xAA);
    create_test_file("test_batch_tmp/old/b.pak", 4096, 0xBB);
    create_test_file("test_batch_tmp/new/a.pak", 4096, 0xAA);
    create_test_file("test_batch_tmp/new/b.pak", 4096, 0xBB);

    // 修改新文件
    modify_test_file("test_batch_tmp/new/a.pak", 100, 50);
    modify_test_file("test_batch_tmp/new/b.pak", 200, 100);

    // 生成任务
    auto tasks = bindiff::generate_diff_tasks(
        "test_batch_tmp/old",
        "test_batch_tmp/new",
        "test_batch_tmp/patches",
        ".pak"
    );

    assert(tasks.size() == 2);

    // 执行批量 diff
    bindiff::BatchProcessor processor;
    bindiff::BatchOptions options;
    options.num_threads = 2;
    options.progress = false;
    processor.set_options(options);

    bindiff::DiffOptions diff_options;
    diff_options.verify = false;  // 加快测试

    auto result = processor.create_diffs(tasks, diff_options);

    // 验证结果
    assert(result.success);
    assert(result.success_count == 2);
    assert(result.failed_count == 0);
    assert(fs::exists("test_batch_tmp/patches/a.pak.bdp"));
    assert(fs::exists("test_batch_tmp/patches/b.pak.bdp"));
    assert(result.total_old_size == 8192);
    assert(result.total_new_size == 8192);

    // 清理
    fs::remove_all("test_batch_tmp");

    printf("✓\n");
}

// 测试：批量 patch
void test_batch_patch() {
    printf("测试: batch patch... ");

    // 创建测试目录
    fs::create_directories("test_batch_tmp/old");
    fs::create_directories("test_batch_tmp/new");
    fs::create_directories("test_batch_tmp/patches");
    fs::create_directories("test_batch_tmp/output");

    // 创建并修改文件
    create_test_file("test_batch_tmp/old/a.pak", 4096, 0xAA);
    create_test_file("test_batch_tmp/new/a.pak", 4096, 0xAA);
    modify_test_file("test_batch_tmp/new/a.pak", 100, 50);

    // 先创建补丁
    bindiff::create_diff(
        "test_batch_tmp/old/a.pak",
        "test_batch_tmp/new/a.pak",
        "test_batch_tmp/patches/a.pak.bdp",
        bindiff::DiffOptions{}
    );

    // 生成 patch 任务
    auto tasks = bindiff::generate_patch_tasks(
        "test_batch_tmp/old",
        "test_batch_tmp/patches",
        "test_batch_tmp/output",
        ".pak"
    );

    assert(tasks.size() == 1);

    // 执行批量 patch
    bindiff::BatchProcessor processor;
    bindiff::BatchOptions options;
    options.num_threads = 2;
    options.progress = false;
    processor.set_options(options);

    auto result = processor.apply_patches(tasks, bindiff::PatchOptions{});

    // 验证结果
    assert(result.success);
    assert(result.success_count == 1);
    assert(fs::exists("test_batch_tmp/output/a.pak"));

    // 验证文件内容（应该与 new 相同）
    std::ifstream ifs1("test_batch_tmp/new/a.pak", std::ios::binary);
    std::ifstream ifs2("test_batch_tmp/output/a.pak", std::ios::binary);
    std::vector<uint8_t> data1((std::istreambuf_iterator<char>(ifs1)),
                                std::istreambuf_iterator<char>());
    std::vector<uint8_t> data2((std::istreambuf_iterator<char>(ifs2)),
                                std::istreambuf_iterator<char>());
    assert(data1 == data2);

    // 清理
    fs::remove_all("test_batch_tmp");

    printf("✓\n");
}

// 测试：进度回调
void test_batch_progress_callback() {
    printf("测试: batch progress callback... ");

    // 创建测试环境
    fs::create_directories("test_batch_tmp/old");
    fs::create_directories("test_batch_tmp/new");

    create_test_file("test_batch_tmp/old/test.pak", 1024);
    create_test_file("test_batch_tmp/new/test.pak", 1024);
    modify_test_file("test_batch_tmp/new/test.pak", 100, 50);

    // 进度追踪
    bool progress_called = false;
    bool task_complete_called = false;
    bool batch_complete_called = false;

    class TestCallback : public bindiff::BatchProgressCallback {
    public:
        bool& progress_called;
        bool& task_complete_called;
        bool& batch_complete_called;

        TestCallback(bool& p, bool& t, bool& b)
            : progress_called(p), task_complete_called(t), batch_complete_called(b) {}

        void on_task_progress(const std::string&, float, const char*) override {
            progress_called = true;
        }

        void on_task_complete(const std::string&, const bindiff::BatchTaskResult&) override {
            task_complete_called = true;
        }

        void on_batch_complete(const bindiff::BatchResult&) override {
            batch_complete_called = true;
        }
    };

    TestCallback callback(progress_called, task_complete_called, batch_complete_called);

    auto tasks = bindiff::generate_diff_tasks(
        "test_batch_tmp/old", "test_batch_tmp/new", "test_batch_tmp/patches", ".pak"
    );

    bindiff::BatchProcessor processor;
    bindiff::BatchOptions options;
    options.progress = true;
    processor.set_options(options);
    processor.set_callback(&callback);

    auto result = processor.create_diffs(tasks, bindiff::DiffOptions{});

    assert(progress_called);
    assert(task_complete_called);
    assert(batch_complete_called);

    // 清理
    fs::remove_all("test_batch_tmp");

    printf("✓\n");
}

// 主测试入口
int main() {
    printf("\n=== Batch Processor 单元测试 ===\n\n");

    test_generate_diff_tasks();
    test_generate_patch_tasks();
    test_batch_diff();
    test_batch_patch();
    test_batch_progress_callback();

    printf("\n所有测试通过 ✅\n\n");
    return 0;
}
