#pragma once

#include <cstdint>
#include <functional>

namespace bindiff {

// ============== 进度报告 ==============

struct Progress {
    float percent;       // 0.0 - 1.0
    const char* stage;   // 当前阶段描述
    uint64_t current;    // 当前处理量
    uint64_t total;      // 总量
    
    Progress(float p = 0.0f, const char* s = "", uint64_t c = 0, uint64_t t = 0)
        : percent(p), stage(s), current(c), total(t) {}
};

// ============== 进度追踪器 ==============

class ProgressTracker {
public:
    using Callback = std::function<void(const Progress&)>;
    
    explicit ProgressTracker(Callback callback = nullptr);
    ~ProgressTracker() = default;
    
    // 设置总量
    void set_total(uint64_t total) { total_ = total; }
    
    // 设置阶段
    void set_stage(const char* stage) { stage_ = stage; }
    
    // 更新进度
    void update(uint64_t current);
    void add(uint64_t delta);
    
    // 完成
    void complete();
    
    // 获取当前进度
    Progress get() const;
    
    // 重置
    void reset();

private:
    void report();
    
    Callback callback_;
    uint64_t total_ = 0;
    uint64_t current_ = 0;
    const char* stage_ = "";
    float last_percent_ = -1.0f;  // 避免频繁回调
    
    // 最小回调间隔
    static constexpr float MIN_UPDATE_DELTA = 0.01f;  // 1%
};

// ============== 简单进度条 (CLI) ==============

class ConsoleProgressBar {
public:
    explicit ConsoleProgressBar(int width = 50);
    ~ConsoleProgressBar() = default;
    
    void update(float percent, const char* stage = "");
    void complete(const char* message = "完成");
    
private:
    int width_;
    float last_percent_ = -1.0f;
};

} // namespace bindiff
