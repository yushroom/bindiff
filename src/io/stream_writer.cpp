#include "io/stream_writer.hpp"
#include <cstring>

namespace bindiff {

// ============== StreamWriter 实现 ==============

StreamWriter::StreamWriter(size_t buffer_size)
    : buffer_(buffer_size)
    , buffer_pos_(0)
    , position_(0)
{
}

StreamWriter::~StreamWriter() {
    close();
}

bool StreamWriter::open(const std::string& path) {
    file_.open(path, std::ios::binary | std::ios::out);
    if (!file_) {
        error_ = "无法打开文件: " + path;
        return false;
    }
    position_ = 0;
    buffer_pos_ = 0;
    return true;
}

void StreamWriter::close() {
    if (file_.is_open()) {
        flush_buffer();
        file_.close();
    }
}

bool StreamWriter::write(const byte* data, size_t size) {
    // 如果数据比缓冲区大，直接写
    if (size >= buffer_.size() - buffer_pos_) {
        if (!flush_buffer()) return false;
        
        file_.write(reinterpret_cast<const char*>(data), size);
        if (!file_) {
            error_ = "写入失败";
            return false;
        }
        position_ += size;
        return true;
    }
    
    // 添加到缓冲区
    std::memcpy(buffer_.data() + buffer_pos_, data, size);
    buffer_pos_ += size;
    position_ += size;
    
    // 缓冲区满则刷新
    if (buffer_pos_ >= buffer_.size()) {
        return flush_buffer();
    }
    
    return true;
}

bool StreamWriter::write(const std::vector<byte>& data) {
    return write(data.data(), data.size());
}

bool StreamWriter::write_u8(uint8_t value) {
    return write(&value, 1);
}

bool StreamWriter::write_u16(uint16_t value) {
    byte data[2];
    data[0] = static_cast<byte>(value & 0xFF);
    data[1] = static_cast<byte>((value >> 8) & 0xFF);
    return write(data, 2);
}

bool StreamWriter::write_u32(uint32_t value) {
    byte data[4];
    for (int i = 0; i < 4; ++i) {
        data[i] = static_cast<byte>((value >> (i * 8)) & 0xFF);
    }
    return write(data, 4);
}

bool StreamWriter::write_u64(uint64_t value) {
    byte data[8];
    for (int i = 0; i < 8; ++i) {
        data[i] = static_cast<byte>((value >> (i * 8)) & 0xFF);
    }
    return write(data, 8);
}

bool StreamWriter::flush() {
    return flush_buffer();
}

bool StreamWriter::flush_buffer() {
    if (buffer_pos_ > 0 && file_.is_open()) {
        file_.write(reinterpret_cast<const char*>(buffer_.data()), buffer_pos_);
        if (!file_) {
            error_ = "刷新缓冲区失败";
            return false;
        }
        buffer_pos_ = 0;
    }
    return true;
}

// ============== StreamReader 实现 ==============

StreamReader::StreamReader(size_t buffer_size)
    : buffer_(buffer_size)
    , buffer_pos_(0)
    , buffer_size_(0)
    , position_(0)
    , size_(0)
{
}

StreamReader::~StreamReader() {
    close();
}

bool StreamReader::open(const std::string& path) {
    file_.open(path, std::ios::binary | std::ios::in);
    if (!file_) {
        error_ = "无法打开文件: " + path;
        return false;
    }
    
    // 获取文件大小
    file_.seekg(0, std::ios::end);
    size_ = static_cast<uint64_t>(file_.tellg());
    file_.seekg(0, std::ios::beg);
    
    position_ = 0;
    buffer_pos_ = 0;
    buffer_size_ = 0;
    return true;
}

void StreamReader::close() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool StreamReader::read(byte* data, size_t size) {
    while (size > 0) {
        // 缓冲区空了，需要填充
        if (buffer_pos_ >= buffer_size_) {
            if (!fill_buffer()) {
                return false;
            }
        }
        
        // 从缓冲区复制
        size_t avail = buffer_size_ - buffer_pos_;
        size_t copy = std::min(size, avail);
        std::memcpy(data, buffer_.data() + buffer_pos_, copy);
        
        data += copy;
        size -= copy;
        buffer_pos_ += copy;
        position_ += copy;
    }
    
    return true;
}

bool StreamReader::read(std::vector<byte>& data, size_t size) {
    data.resize(size);
    return read(data.data(), size);
}

bool StreamReader::read_u8(uint8_t& value) {
    return read(&value, 1);
}

bool StreamReader::read_u16(uint16_t& value) {
    byte data[2];
    if (!read(data, 2)) return false;
    value = static_cast<uint16_t>(data[0]) | 
            (static_cast<uint16_t>(data[1]) << 8);
    return true;
}

bool StreamReader::read_u32(uint32_t& value) {
    byte data[4];
    if (!read(data, 4)) return false;
    value = 0;
    for (int i = 0; i < 4; ++i) {
        value |= static_cast<uint32_t>(data[i]) << (i * 8);
    }
    return true;
}

bool StreamReader::read_u64(uint64_t& value) {
    byte data[8];
    if (!read(data, 8)) return false;
    value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= static_cast<uint64_t>(data[i]) << (i * 8);
    }
    return true;
}

bool StreamReader::skip(uint64_t bytes) {
    // 优化: 如果跳过的在缓冲区内
    if (bytes <= buffer_size_ - buffer_pos_) {
        buffer_pos_ += static_cast<size_t>(bytes);
        position_ += bytes;
        return true;
    }
    
    // 否则直接 seek
    file_.seekg(bytes, std::ios::cur);
    if (!file_) {
        error_ = "Seek 失败";
        return false;
    }
    position_ += bytes;
    buffer_pos_ = 0;
    buffer_size_ = 0;
    return true;
}

bool StreamReader::fill_buffer() {
    if (!file_.is_open() || eof()) {
        error_ = "文件已结束";
        return false;
    }
    
    file_.read(reinterpret_cast<char*>(buffer_.data()), buffer_.size());
    buffer_size_ = static_cast<size_t>(file_.gcount());
    buffer_pos_ = 0;
    
    if (buffer_size_ == 0) {
        error_ = "读取失败";
        return false;
    }
    
    return true;
}

} // namespace bindiff
