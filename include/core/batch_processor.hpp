#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include "types.hpp"
#include "utils/thread_pool.hpp"

namespace bindiff {

// ============== 批处理任务定义 ==============

struct BatchTask {
    std::string old_path;
    std::string new_path;
    std::string patch_path;  // 输出路径
    int priority = 0;        // 优先级（暂未使用）
};

// ============== 批处理结果 ==============

struct BatchTaskResult {
    std::string task_id;     // 通常是文件名
    bool success = false;
    std::string error;
    uint64_t old_size = 0;
    uint64_t new_size = 0;
    uint64_t patch_size = 0;
    double elapsed_seconds = 0.0;
};

struct BatchResult {
    bool success = false;
    std::string error;
    std::vector<BatchTaskResult> task_results;
    size_t total_tasks = 0;
    size_t success_count = 0;
    size_t failed_count = 0;
    double total_elapsed = 0.0;
    uint64_t total_old_size = 0;
    uint64_t total_new_size = 0;
    uint64_t total_patch_size = 0;
};

// ============== 批处理选项 ==============

struct BatchOptions {
    int num_threads = 0;           // 0 = auto (硬件并发数)
    bool verify = true;            // 是否验证
    bool continue_on_error = true; // 单任务失败是否继续
    bool progress = true;          // 是否显示进度
};

// ============== 批处理进度回调 ==============

class BatchProgressCallback {
public:
    virtual ~BatchProgressCallback() = default;
    
    // 单任务进度
    virtual void on_task_progress(const std::string& task_id, float percent, const char* stage) {
        (void)task_id;
        (void)percent;
        (void)stage;
    }
    
    // 单任务完成
    virtual void on_task_complete(const std::string& task_id, const BatchTaskResult& result) {
        (void)task_id;
        (void)result;
    }
    
    // 整体进度
    virtual void on_batch_progress(size_t completed, size_t total) {
        (void)completed;
        (void)total;
    }
    
    // 全部完成
    virtual void on_batch_complete(const BatchResult& result) {
        (void)result;
    }
};

// ============== 批处理引擎 ==============

class BatchProcessor {
public:
    BatchProcessor();
    ~BatchProcessor();
    
    // 设置选项
    void set_options(const BatchOptions& options);
    
    // 设置进度回调
    void set_callback(BatchProgressCallback* callback);
    
    // 批量创建补丁
    BatchResult create_diffs(
        const std::vector<BatchTask>& tasks,
        const DiffOptions& diff_options = {}
    );
    
    // 批量应用补丁
    BatchResult apply_patches(
        const std::vector<BatchTask>& tasks,
        const PatchOptions& patch_options = {}
    );
    
private:
    BatchOptions options_;
    BatchProgressCallback* callback_ = nullptr;
    std::unique_ptr<ThreadPool> pool_;
    
    // 进度追踪（线程安全）
    std::mutex progress_mutex_;
    size_t completed_tasks_ = 0;
    size_t total_tasks_ = 0;
    
    // 内部处理函数
    BatchTaskResult process_diff_task(
        const BatchTask& task,
        const DiffOptions& diff_options
    );
    
    BatchTaskResult process_patch_task(
        const BatchTask& task,
        const PatchOptions& patch_options
    );
    
    void update_progress(const std::string& task_id, float percent, const char* stage);
    void notify_task_complete(const std::string& task_id, const BatchTaskResult& result);
};

// ============== 辅助函数 ==============

// 从目录对生成任务列表
std::vector<BatchTask> generate_diff_tasks(
    const std::string& old_dir,
    const std::string& new_dir,
    const std::string& output_dir,
    const std::string& extension = ".pak"
);

std::vector<BatchTask> generate_patch_tasks(
    const std::string& old_dir,
    const std::string& patch_dir,
    const std::string& output_dir,
    const std::string& extension = ".pak"
);

} // namespace bindiff
