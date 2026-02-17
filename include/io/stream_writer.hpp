#pragma once

#include "types.hpp"
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

namespace bindiff {

// ============== 流式写入器 ==============

class StreamWriter {
public:
    explicit StreamWriter(size_t buffer_size = 4 * 1024 * 1024);  // 4MB buffer
    ~StreamWriter();
    
    // 打开文件
    bool open(const std::string& path);
    
    // 关闭
    void close();
    
    // 写入数据
    bool write(const byte* data, size_t size);
    bool write(const std::vector<byte>& data);
    
    // 写入整数 (小端序)
    bool write_u8(uint8_t value);
    bool write_u16(uint16_t value);
    bool write_u32(uint32_t value);
    bool write_u64(uint64_t value);
    
    // 刷新缓冲区
    bool flush();
    
    // 获取当前偏移
    uint64_t position() const { return position_; }
    
    // 是否打开
    bool is_open() const { return file_.is_open(); }
    
    // 错误信息
    const std::string& error() const { return error_; }

private:
    bool flush_buffer();
    
    std::ofstream file_;
    std::vector<byte> buffer_;
    size_t buffer_pos_;
    uint64_t position_;
    std::string error_;
};

// ============== 流式读取器 ==============

class StreamReader {
public:
    explicit StreamReader(size_t buffer_size = 4 * 1024 * 1024);  // 4MB buffer
    ~StreamReader();
    
    // 打开文件
    bool open(const std::string& path);
    
    // 关闭
    void close();
    
    // 读取数据
    bool read(byte* data, size_t size);
    bool read(std::vector<byte>& data, size_t size);
    
    // 读取整数 (小端序)
    bool read_u8(uint8_t& value);
    bool read_u16(uint16_t& value);
    bool read_u32(uint32_t& value);
    bool read_u64(uint64_t& value);
    
    // 获取当前偏移
    uint64_t position() const { return position_; }
    
    // 获取文件大小
    uint64_t size() const { return size_; }
    
    // 跳过字节
    bool skip(uint64_t bytes);
    
    // 是否打开
    bool is_open() const { return file_.is_open(); }
    
    // 是否到达末尾
    bool eof() const { return position_ >= size_; }
    
    // 错误信息
    const std::string& error() const { return error_; }

private:
    bool fill_buffer();
    
    std::ifstream file_;
    std::vector<byte> buffer_;
    size_t buffer_pos_;
    size_t buffer_size_;
    uint64_t position_;
    uint64_t size_;
    std::string error_;
};

} // namespace bindiff
