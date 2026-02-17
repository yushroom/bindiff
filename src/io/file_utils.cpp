#include "io/file_utils.hpp"
#include <sys/stat.h>

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
#else
    #include <unistd.h>
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
    return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
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
    return rename(old_path.c_str(), new_path.c_str()) == 0;
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
    char temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    char file[MAX_PATH];
    GetTempFileNameA(temp, prefix.c_str(), 0, file);
    return file;
#else
    std::string templ = "/tmp/" + prefix + "XXXXXX";
    char* path = mktemp(const_cast<char*>(templ.c_str()));
    return path ? path : "";
#endif
}

bool copy_file(const std::string& src, const std::string& dst) {
#ifdef _WIN32
    return CopyFileA(src.c_str(), dst.c_str(), FALSE) != FALSE;
#else
    // 简单实现，生产环境应使用 sendfile 或 splice
    FILE* fin = fopen(src.c_str(), "rb");
    FILE* fout = fopen(dst.c_str(), "wb");
    if (!fin || !fout) {
        if (fin) fclose(fin);
        if (fout) fclose(fout);
        return false;
    }
    
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fin)) > 0) {
        if (fwrite(buf, 1, n, fout) != n) {
            fclose(fin);
            fclose(fout);
            return false;
        }
    }
    
    fclose(fin);
    fclose(fout);
    return true;
#endif
}

std::string get_filename(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    return (pos == std::string::npos) ? path : path.substr(pos + 1);
}

std::string get_extension(const std::string& path) {
    size_t pos = path.find_last_of('.');
    if (pos == std::string::npos || pos == 0) return "";
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
    char sep = '\\';
#else
    char sep = '/';
#endif
    
    if (a.back() == sep || a.back() == '/' || a.back() == '\\') {
        return a + b;
    }
    return a + sep + b;
}

} // namespace bindiff
