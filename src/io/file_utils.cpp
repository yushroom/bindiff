#include "io/file_utils.hpp"
#include <sys/stat.h>
#include <cstring>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #define WIN32_LEAN_AND_MEAN
#else
    #include <unistd.h>
    #include <fcntl.h>
#endif

namespace bindiff {

uint64_t get_file_size(const std::string& path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA attr;
    if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attr)) {
        return (static_cast<uint64_t>(attr.nFileSizeHigh) << 32) | 
               static_cast<uint64_t>(attr.nFileSizeLow);
    }
    return 0;
#else
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return static_cast<uint64_t>(st.st_size);
    }
    return 0;
#endif
}

bool file_exists(const std::string& path) {
#ifdef _WIN32
    DWORD attrib = GetFileAttributesA(path.c_str());
    return attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY);
#else
    return access(path.c_str(), F_OK) == 0;
#endif
}

bool delete_file(const std::string& path) {
#ifdef _WIN32
    return DeleteFileA(path.c_str()) != FALSE;
#else
    return unlink(path.c_str()) == 0;
#endif
}

bool rename_file(const std::string& old_path, const std::string& new_path) {
#ifdef _WIN32
    return MoveFileA(old_path.c_str(), new_path.c_str()) != FALSE;
#else
    return ::rename(old_path.c_str(), new_path.c_str()) == 0;
#endif
}

bool create_directory(const std::string& path) {
#ifdef _WIN32
    return CreateDirectoryA(path.c_str(), nullptr) != FALSE;
#else
    return mkdir(path.c_str(), 0755) == 0;
#endif
}

std::string get_temp_file_path(const std::string& prefix) {
#ifdef _WIN32
    char temp_path[MAX_PATH];
    char temp_file[MAX_PATH];
    
    if (GetTempPathA(MAX_PATH, temp_path) == 0) {
        return "";
    }
    
    if (GetTempFileNameA(temp_path, prefix.c_str(), 0, temp_file) == 0) {
        return "";
    }
    
    return std::string(temp_file);
#else
    // 使用 mkstemp 创建安全的临时文件
    std::string templ = "/tmp/" + prefix + "_XXXXXX";
    std::vector<char> templ_buf(templ.begin(), templ.end());
    templ_buf.push_back('\0');
    
    int fd = mkstemp(templ_buf.data());
    if (fd == -1) {
        return "";
    }
    
    // 关闭文件描述符，只返回路径
    close(fd);
    
    return std::string(templ_buf.data());
#endif
}

bool copy_file(const std::string& src, const std::string& dst) {
#ifdef _WIN32
    return CopyFileA(src.c_str(), dst.c_str(), FALSE) != FALSE;
#else
    FILE* fin = fopen(src.c_str(), "rb");
    FILE* fout = fopen(dst.c_str(), "wb");
    
    if (!fin || !fout) {
        if (fin) fclose(fin);
        if (fout) fclose(fout);
        return false;
    }
    
    char buf[8192];
    size_t n;
    bool success = true;
    
    while ((n = fread(buf, 1, sizeof(buf), fin)) > 0) {
        if (fwrite(buf, 1, n, fout) != n) {
            success = false;
            break;
        }
    }
    
    fclose(fin);
    fclose(fout);
    return success;
#endif
}

std::string get_filename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

std::string get_extension(const std::string& path) {
    size_t pos = path.find_last_of('.');
    size_t sep = path.find_last_of("/\\");
    
    // 确保点号在路径分隔符之后
    if (pos == std::string::npos || pos == 0 || 
        (sep != std::string::npos && pos < sep)) {
        return "";
    }
    return path.substr(pos);
}

std::string get_directory(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? "." : path.substr(0, pos);
}

std::string join_path(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    
#ifdef _WIN32
    const char sep = '\\';
#else
    const char sep = '/';
#endif
    
    // 移除末尾的分隔符
    std::string result = a;
    while (!result.empty() && (result.back() == '/' || result.back() == '\\')) {
        result.pop_back();
    }
    
    // 移除开头的分隔符
    std::string second = b;
    while (!second.empty() && (second.front() == '/' || second.front() == '\\')) {
        second.erase(0, 1);
    }
    
    return result + sep + second;
}

} // namespace bindiff
