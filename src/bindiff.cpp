#include "types.hpp"
#include "core/diff_engine.hpp"
#include "core/patch_engine.hpp"
#include "core/patch_format.hpp"
#include "io/file_utils.hpp"
#include "crypto/sha256.hpp"
#include <fstream>

namespace bindiff {

Result create_diff(
    const std::string& old_path,
    const std::string& new_path,
    const std::string& patch_path,
    const DiffOptions& options,
    ProgressCallback* callback
) {
    DiffEngine engine(options);
    return engine.create_diff(old_path, new_path, patch_path, callback);
}

Result apply_patch(
    const std::string& old_path,
    const std::string& patch_path,
    const std::string& new_path,
    const PatchOptions& options,
    ProgressCallback* callback
) {
    PatchEngine engine(options);
    return engine.apply_patch(old_path, patch_path, new_path, callback);
}

Result verify_patch(
    const std::string& old_path,
    const std::string& new_path,
    const std::string& patch_path
) {
    Result result;
    
    // 读取 patch 信息
    auto info = get_patch_info(patch_path);
    if (info.version == 0) {
        result.error = "无效的补丁文件";
        return result;
    }
    
    // 验证原文件
    if (!file_exists(old_path)) {
        result.error = "原文件不存在";
        return result;
    }
    if (get_file_size(old_path) != info.old_size) {
        result.error = "原文件大小不匹配";
        return result;
    }
    
    // 验证新文件
    if (!file_exists(new_path)) {
        result.error = "新文件不存在";
        return result;
    }
    if (get_file_size(new_path) != info.new_size) {
        result.error = "新文件大小不匹配";
        return result;
    }
    
    // 应用 patch 到临时文件并比对
    // TODO: 实现完整验证
    
    result.success = true;
    return result;
}

PatchInfo get_patch_info(const std::string& patch_path) {
    PatchInfo info;
    
    std::ifstream file(patch_path, std::ios::binary);
    if (!file) {
        return info;
    }
    
    PatchHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (!file || !header.is_valid()) {
        return info;
    }
    
    info.version = header.version;
    info.block_size = header.block_size;
    info.old_size = header.old_size;
    info.new_size = header.new_size;
    info.num_blocks = header.num_blocks;
    
    // 获取文件大小
    file.seekg(0, std::ios::end);
    info.patch_size = static_cast<uint64_t>(file.tellg());
    
    info.old_sha256 = SHA256::to_hex(header.old_sha256, 32);
    info.new_sha256 = SHA256::to_hex(header.new_sha256, 32);
    
    return info;
}

} // namespace bindiff
