#include "io/mmap_file.hpp"
#include <system_error>
#include <cstring>

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
#endif

namespace bindiff {

// ============== MMapFile 实现 ==============

MMapFile::MMapFile()
    : handle_(nullptr)
    , mapping_(nullptr)
    , data_(nullptr)
    , size_(0)
{
}

MMapFile::~MMapFile() {
    close();
}

MMapFile::MMapFile(MMapFile&& other) noexcept
    : handle_(other.handle_)
    , mapping_(other.mapping_)
    , data_(other.data_)
    , size_(other.size_)
    , error_(std::move(other.error_))
{
    other.handle_ = nullptr;
    other.mapping_ = nullptr;
    other.data_ = nullptr;
    other.size_ = 0;
}

MMapFile& MMapFile::operator=(MMapFile&& other) noexcept {
    if (this != &other) {
        close();
        handle_ = other.handle_;
        mapping_ = other.mapping_;
        data_ = other.data_;
        size_ = other.size_;
        error_ = std::move(other.error_);
        
        other.handle_ = nullptr;
        other.mapping_ = nullptr;
        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

bool MMapFile::open(const std::string& path) {
    return map_file(path, true);
}

bool MMapFile::create(const std::string& path, uint64_t size) {
    // TODO: 实现可写映射
    error_ = "可写映射尚未实现";
    return false;
}

void MMapFile::close() {
    unmap_file();
}

byte_view MMapFile::view(uint64_t offset, size_t length) const {
    if (!data_ || offset >= size_) {
        return {nullptr, 0};
    }
    size_t avail = static_cast<size_t>(size_ - offset);
    return {data_ + offset, std::min(length, avail)};
}

bool MMapFile::flush() {
#ifdef _WIN32
    if (data_ && mapping_) {
        return FlushViewOfFile(data_, 0) != FALSE;
    }
#else
    if (data_) {
        return msync(data_, size_, MS_SYNC) == 0;
    }
#endif
    return false;
}

// ============== 平台相关实现 ==============

#ifdef _WIN32

bool MMapFile::map_file(const std::string& path, bool read_only) {
    close();
    
    // 打开文件
    HANDLE hFile = CreateFileA(
        path.c_str(),
        read_only ? GENERIC_READ : (GENERIC_READ | GENERIC_WRITE),
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        error_ = "无法打开文件: " + path;
        return false;
    }
    
    // 获取大小
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile, &fileSize)) {
        CloseHandle(hFile);
        error_ = "无法获取文件大小";
        return false;
    }
    size_ = static_cast<uint64_t>(fileSize.QuadPart);
    
    // 创建映射
    HANDLE hMap = CreateFileMappingA(
        hFile,
        nullptr,
        read_only ? PAGE_READONLY : PAGE_READWRITE,
        0, 0,
        nullptr
    );
    
    if (!hMap) {
        CloseHandle(hFile);
        error_ = "无法创建文件映射";
        return false;
    }
    
    // 映射视图
    data_ = static_cast<byte*>(MapViewOfFile(
        hMap,
        read_only ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS,
        0, 0, 0
    ));
    
    if (!data_) {
        CloseHandle(hMap);
        CloseHandle(hFile);
        error_ = "无法映射文件视图";
        return false;
    }
    
    handle_ = hFile;
    mapping_ = hMap;
    return true;
}

bool MMapFile::unmap_file() {
    bool success = true;
    
    if (data_) {
        if (!UnmapViewOfFile(data_)) {
            success = false;
        }
        data_ = nullptr;
    }
    
    if (mapping_) {
        CloseHandle(mapping_);
        mapping_ = nullptr;
    }
    
    if (handle_) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }
    
    size_ = 0;
    return success;
}

#else  // Linux/macOS

bool MMapFile::map_file(const std::string& path, bool read_only) {
    close();
    
    // 打开文件
    int fd = ::open(path.c_str(), read_only ? O_RDONLY : O_RDWR);
    if (fd < 0) {
        error_ = "无法打开文件: " + path + " (" + std::strerror(errno) + ")";
        return false;
    }
    
    // 获取大小
    struct stat st;
    if (fstat(fd, &st) < 0) {
        ::close(fd);
        error_ = "无法获取文件大小: " + std::string(std::strerror(errno));
        return false;
    }
    size_ = static_cast<uint64_t>(st.st_size);
    
    // 空文件特殊处理
    if (size_ == 0) {
        handle_ = reinterpret_cast<void*>(static_cast<intptr_t>(fd));
        data_ = nullptr;
        return true;
    }
    
    // 映射
    void* addr = mmap(
        nullptr,
        size_,
        read_only ? PROT_READ : (PROT_READ | PROT_WRITE),
        MAP_PRIVATE,
        fd,
        0
    );
    
    if (addr == MAP_FAILED) {
        ::close(fd);
        error_ = "无法映射文件: " + std::string(std::strerror(errno));
        return false;
    }
    
    // 使用 madvise 优化大文件读取
    if (size_ > 64 * 1024 * 1024) {  // > 64MB
        madvise(addr, size_, MADV_SEQUENTIAL);
    }
    
    handle_ = reinterpret_cast<void*>(static_cast<intptr_t>(fd));
    data_ = static_cast<byte*>(addr);
    return true;
}

bool MMapFile::unmap_file() {
    bool success = true;
    
    if (data_) {
        if (munmap(data_, size_) < 0) {
            success = false;
        }
        data_ = nullptr;
    }
    
    if (handle_) {
        int fd = static_cast<int>(reinterpret_cast<intptr_t>(handle_));
        ::close(fd);
        handle_ = nullptr;
    }
    
    size_ = 0;
    return success;
}

#endif

} // namespace bindiff
