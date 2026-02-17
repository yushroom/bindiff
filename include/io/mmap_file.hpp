#pragma once

#include "types.hpp"
#include <cstdint>
#include <string>
#include <memory>

namespace bindiff {

// ============== 内存映射文件 ==============

class MMapFile {
public:
    MMapFile();
    ~MMapFile();
    
    // 禁止拷贝
    MMapFile(const MMapFile&) = delete;
    MMapFile& operator=(const MMapFile&) = delete;
    
    // 允许移动
    MMapFile(MMapFile&& other) noexcept;
    MMapFile& operator=(MMapFile&& other) noexcept;
    
    // 打开文件 (只读)
    bool open(const std::string& path);
    
    // 创建文件 (读写)
    bool create(const std::string& path, uint64_t size);
    
    // 关闭
    void close();
    
    // 获取数据指针
    const byte* data() const { return data_; }
    byte* data() { return data_; }
    
    // 获取大小
    uint64_t size() const { return size_; }
    
    // 是否有效
    bool is_open() const { return data_ != nullptr; }
    
    // 获取映射区域
    byte_view view(uint64_t offset, size_t length) const;
    
    // 同步到磁盘
    bool flush();
    
    // 错误信息
    const std::string& error() const { return error_; }

private:
    void* handle_;      // 平台相关句柄
    void* mapping_;     // 映射句柄 (Windows)
    byte* data_;
    uint64_t size_;
    std::string error_;
    
    bool map_file(const std::string& path, bool read_only);
    bool unmap_file();
};

} // namespace bindiff
