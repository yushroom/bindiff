#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace bindiff {

// ============== 类型别名 ==============

using byte = uint8_t;
using byte_view = std::pair<const byte*, size_t>;
using bytes = std::vector<byte>;

// ============== 结果类型 ==============

struct Result {
    bool success = false;
    std::string error;
    size_t bytes_processed = 0;
    double elapsed_seconds = 0.0;
    
    operator bool() const { return success; }
};

// ============== 选项 ==============

struct DiffOptions {
    uint32_t block_size = 64 * 1024 * 1024;  // 64MB
    int compression_level = 1;                // LZ4: 1-12
    int num_threads = 0;                      // 0 = auto (hardware concurrency)
    bool verify = true;
};

struct PatchOptions {
    bool verify = true;
};

// ============== 进度回调 ==============

class ProgressCallback {
public:
    virtual ~ProgressCallback() = default;
    
    virtual void on_progress(float percent, const char* stage) {
        (void)percent;
        (void)stage;
    }
    
    virtual void on_complete(const Result& result) {
        (void)result;
    }
};

// ============== Patch 信息 ==============

struct PatchInfo {
    uint16_t version = 0;
    uint32_t block_size = 0;
    uint64_t old_size = 0;
    uint64_t new_size = 0;
    uint32_t num_blocks = 0;
    uint64_t patch_size = 0;
    std::string old_sha256;
    std::string new_sha256;
};

// ============== 核心接口 ==============

Result create_diff(
    const std::string& old_path,
    const std::string& new_path,
    const std::string& patch_path,
    const DiffOptions& options = {},
    ProgressCallback* callback = nullptr
);

Result apply_patch(
    const std::string& old_path,
    const std::string& patch_path,
    const std::string& new_path,
    const PatchOptions& options = {},
    ProgressCallback* callback = nullptr
);

Result verify_patch(
    const std::string& old_path,
    const std::string& new_path,
    const std::string& patch_path
);

PatchInfo get_patch_info(const std::string& patch_path);

// ============== 工具函数 ==============

std::string format_size(uint64_t bytes);
std::string format_duration(double seconds);

} // namespace bindiff
