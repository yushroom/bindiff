#include "core/batch_processor.hpp"
#include "bindiff.hpp"
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace bindiff {

namespace fs = std::filesystem;

// ============== BatchProcessor 实现 ==============

BatchProcessor::BatchProcessor() {
    options_.num_threads = std::thread::hardware_concurrency();
    if (options_.num_threads == 0) options_.num_threads = 4;
}

BatchProcessor::~BatchProcessor() = default;

void BatchProcessor::set_options(const BatchOptions& options) {
    options_ = options;
    if (options_.num_threads == 0) {
        options_.num_threads = std::thread::hardware_concurrency();
        if (options_.num_threads == 0) options_.num_threads = 4;
    }
}

void BatchProcessor::set_callback(BatchProgressCallback* callback) {
    callback_ = callback;
}

BatchResult BatchProcessor::create_diffs(
    const std::vector<BatchTask>& tasks,
    const DiffOptions& diff_options
) {
    BatchResult result;
    result.total_tasks = tasks.size();
    total_tasks_ = tasks.size();
    completed_tasks_ = 0;
    
    if (tasks.empty()) {
        result.success = true;
        return result;
    }
    
    // 创建线程池
    pool_ = std::make_unique<ThreadPool>(options_.num_threads);
    
    std::vector<std::future<BatchTaskResult>> futures;
    futures.reserve(tasks.size());
    
    auto start_time = std::chrono::steady_clock::now();
    
    // 提交所有任务
    for (const auto& task : tasks) {
        futures.push_back(
            pool_->submit(&BatchProcessor::process_diff_task, this, 
                         std::cref(task), std::cref(diff_options))
        );
    }
    
    // 等待所有任务完成
    for (auto& future : futures) {
        try {
            auto task_result = future.get();
            result.task_results.push_back(task_result);
            
            if (task_result.success) {
                result.success_count++;
                result.total_old_size += task_result.old_size;
                result.total_new_size += task_result.new_size;
                result.total_patch_size += task_result.patch_size;
            } else {
                result.failed_count++;
                if (!options_.continue_on_error) {
                    // 停止处理（但已提交的任务会继续）
                    result.error = "任务失败: " + task_result.task_id;
                    break;
                }
            }
        } catch (const std::exception& e) {
            result.failed_count++;
            BatchTaskResult err_result;
            err_result.success = false;
            err_result.error = e.what();
            result.task_results.push_back(err_result);
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.total_elapsed = std::chrono::duration<double>(end_time - start_time).count();
    
    result.success = (result.failed_count == 0);
    
    if (callback_) {
        callback_->on_batch_complete(result);
    }
    
    pool_.reset();  // 释放线程池
    
    return result;
}

BatchResult BatchProcessor::apply_patches(
    const std::vector<BatchTask>& tasks,
    const PatchOptions& patch_options
) {
    BatchResult result;
    result.total_tasks = tasks.size();
    total_tasks_ = tasks.size();
    completed_tasks_ = 0;
    
    if (tasks.empty()) {
        result.success = true;
        return result;
    }
    
    pool_ = std::make_unique<ThreadPool>(options_.num_threads);
    
    std::vector<std::future<BatchTaskResult>> futures;
    futures.reserve(tasks.size());
    
    auto start_time = std::chrono::steady_clock::now();
    
    for (const auto& task : tasks) {
        futures.push_back(
            pool_->submit(&BatchProcessor::process_patch_task, this,
                         std::cref(task), std::cref(patch_options))
        );
    }
    
    for (auto& future : futures) {
        try {
            auto task_result = future.get();
            result.task_results.push_back(task_result);
            
            if (task_result.success) {
                result.success_count++;
                result.total_old_size += task_result.old_size;
                result.total_new_size += task_result.new_size;
                result.total_patch_size += task_result.patch_size;
            } else {
                result.failed_count++;
                if (!options_.continue_on_error) {
                    result.error = "任务失败: " + task_result.task_id;
                    break;
                }
            }
        } catch (const std::exception& e) {
            result.failed_count++;
            BatchTaskResult err_result;
            err_result.success = false;
            err_result.error = e.what();
            result.task_results.push_back(err_result);
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    result.total_elapsed = std::chrono::duration<double>(end_time - start_time).count();
    
    result.success = (result.failed_count == 0);
    
    if (callback_) {
        callback_->on_batch_complete(result);
    }
    
    pool_.reset();
    
    return result;
}

BatchTaskResult BatchProcessor::process_diff_task(
    const BatchTask& task,
    const DiffOptions& diff_options
) {
    BatchTaskResult result;
    result.task_id = fs::path(task.old_path).filename().string();
    
    // 获取文件大小
    if (file_exists(task.old_path)) {
        result.old_size = get_file_size(task.old_path);
    }
    if (file_exists(task.new_path)) {
        result.new_size = get_file_size(task.new_path);
    }
    
    // 进度回调适配器
    class TaskProgressAdapter : public ProgressCallback {
    public:
        TaskProgressAdapter(BatchProcessor* processor, const std::string& task_id)
            : processor_(processor), task_id_(task_id) {}
        
        void on_progress(float percent, const char* stage) override {
            processor_->update_progress(task_id_, percent, stage);
        }
        
    private:
        BatchProcessor* processor_;
        std::string task_id_;
    };
    
    TaskProgressAdapter progress_adapter(this, result.task_id);
    ProgressCallback* callback = options_.progress ? &progress_adapter : nullptr;
    
    // 执行 diff
    auto diff_result = create_diff(
        task.old_path, task.new_path, task.patch_path,
        diff_options, callback
    );
    
    result.success = diff_result.success;
    result.error = diff_result.error;
    result.elapsed_seconds = diff_result.elapsed_seconds;
    
    if (result.success && file_exists(task.patch_path)) {
        result.patch_size = get_file_size(task.patch_path);
    }
    
    // 通知完成
    notify_task_complete(result.task_id, result);
    
    return result;
}

BatchTaskResult BatchProcessor::process_patch_task(
    const BatchTask& task,
    const PatchOptions& patch_options
) {
    BatchTaskResult result;
    result.task_id = fs::path(task.old_path).filename().string();
    
    if (file_exists(task.old_path)) {
        result.old_size = get_file_size(task.old_path);
    }
    if (file_exists(task.patch_path)) {
        result.patch_size = get_file_size(task.patch_path);
    }
    
    class TaskProgressAdapter : public ProgressCallback {
    public:
        TaskProgressAdapter(BatchProcessor* processor, const std::string& task_id)
            : processor_(processor), task_id_(task_id) {}
        
        void on_progress(float percent, const char* stage) override {
            processor_->update_progress(task_id_, percent, stage);
        }
        
    private:
        BatchProcessor* processor_;
        std::string task_id_;
    };
    
    TaskProgressAdapter progress_adapter(this, result.task_id);
    ProgressCallback* callback = options_.progress ? &progress_adapter : nullptr;
    
    auto patch_result = apply_patch(
        task.old_path, task.patch_path, task.new_path,
        patch_options, callback
    );
    
    result.success = patch_result.success;
    result.error = patch_result.error;
    result.elapsed_seconds = patch_result.elapsed_seconds;
    
    if (result.success && file_exists(task.new_path)) {
        result.new_size = get_file_size(task.new_path);
    }
    
    notify_task_complete(result.task_id, result);
    
    return result;
}

void BatchProcessor::update_progress(const std::string& task_id, float percent, const char* stage) {
    if (callback_) {
        callback_->on_task_progress(task_id, percent, stage);
    }
}

void BatchProcessor::notify_task_complete(const std::string& task_id, const BatchTaskResult& result) {
    {
        std::lock_guard<std::mutex> lock(progress_mutex_);
        completed_tasks_++;
    }
    
    if (callback_) {
        callback_->on_task_complete(task_id, result);
        callback_->on_batch_progress(completed_tasks_, total_tasks_);
    }
}

// ============== 辅助函数实现 ==============

std::vector<BatchTask> generate_diff_tasks(
    const std::string& old_dir,
    const std::string& new_dir,
    const std::string& output_dir,
    const std::string& extension
) {
    std::vector<BatchTask> tasks;
    
    if (!fs::exists(old_dir) || !fs::exists(new_dir)) {
        return tasks;
    }
    
    // 确保输出目录存在
    if (!output_dir.empty() && !fs::exists(output_dir)) {
        fs::create_directories(output_dir);
    }
    
    // 遍历 old 目录，寻找匹配的 new 文件
    for (const auto& entry : fs::directory_iterator(old_dir)) {
        if (!entry.is_regular_file()) continue;
        
        std::string filename = entry.path().filename().string();
        
        // 检查扩展名
        if (!extension.empty() && entry.path().extension() != extension) {
            continue;
        }
        
        fs::path new_file = fs::path(new_dir) / filename;
        if (!fs::exists(new_file)) {
            continue;  // 新文件不存在，跳过
        }
        
        BatchTask task;
        task.old_path = entry.path().string();
        task.new_path = new_file.string();
        
        if (!output_dir.empty()) {
            task.patch_path = (fs::path(output_dir) / (filename + ".bdp")).string();
        } else {
            task.patch_path = (entry.path().string() + ".bdp");
        }
        
        tasks.push_back(task);
    }
    
    // 按文件名排序（保证顺序一致性）
    std::sort(tasks.begin(), tasks.end(), [](const BatchTask& a, const BatchTask& b) {
        return a.old_path < b.old_path;
    });
    
    return tasks;
}

std::vector<BatchTask> generate_patch_tasks(
    const std::string& old_dir,
    const std::string& patch_dir,
    const std::string& output_dir,
    const std::string& extension
) {
    std::vector<BatchTask> tasks;
    
    if (!fs::exists(old_dir) || !fs::exists(patch_dir)) {
        return tasks;
    }
    
    if (!output_dir.empty() && !fs::exists(output_dir)) {
        fs::create_directories(output_dir);
    }
    
    // 遍历 patch 目录，寻找匹配的 old 文件
    for (const auto& entry : fs::directory_iterator(patch_dir)) {
        if (!entry.is_regular_file()) continue;
        
        std::string patch_filename = entry.path().filename().string();
        
        // 补丁文件应该是 xxx.pak.bdp，对应的原文件是 xxx.pak
        if (patch_filename.size() <= 4 || 
            patch_filename.substr(patch_filename.size() - 4) != ".bdp") {
            continue;
        }
        
        std::string base_name = patch_filename.substr(0, patch_filename.size() - 4);
        
        // 检查扩展名
        if (!extension.empty()) {
            if (base_name.size() <= extension.size() ||
                base_name.substr(base_name.size() - extension.size()) != extension) {
                continue;
            }
        }
        
        fs::path old_file = fs::path(old_dir) / base_name;
        if (!fs::exists(old_file)) {
            continue;
        }
        
        BatchTask task;
        task.old_path = old_file.string();
        task.patch_path = entry.path().string();
        
        if (!output_dir.empty()) {
            task.new_path = (fs::path(output_dir) / base_name).string();
        } else {
            task.new_path = (fs::path(old_dir) / (base_name + ".new")).string();
        }
        
        tasks.push_back(task);
    }
    
    std::sort(tasks.begin(), tasks.end(), [](const BatchTask& a, const BatchTask& b) {
        return a.old_path < b.old_path;
    });
    
    return tasks;
}

} // namespace bindiff
