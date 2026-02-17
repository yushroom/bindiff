#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bindiff {

// ============== 文件工具函数 ==============

// 获取文件大小
uint64_t get_file_size(const std::string& path);

// 检查文件是否存在
bool file_exists(const std::string& path);

// 删除文件
bool delete_file(const std::string& path);

// 重命名文件
bool rename_file(const std::string& old_path, const std::string& new_path);

// 创建目录
bool create_directory(const std::string& path);

// 获取临时文件路径
std::string get_temp_file_path(const std::string& prefix = "bindiff");

// 复制文件
bool copy_file(const std::string& src, const std::string& dst);

// ============== 路径工具 ==============

// 获取文件名 (不含路径)
std::string get_filename(const std::string& path);

// 获取文件扩展名
std::string get_extension(const std::string& path);

// 获取目录路径
std::string get_directory(const std::string& path);

// 组合路径
std::string join_path(const std::string& a, const std::string& b);

} // namespace bindiff
