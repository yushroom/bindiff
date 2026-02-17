#pragma once

#include "types.hpp"
#include <cstdint>
#include <string>

namespace bindiff {

// ============== Patch 文件头 ==============

#pragma pack(push, 1)

struct PatchHeader {
    char     magic[4];        // "UEBD"
    uint16_t version;         // 格式版本 (1)
    uint16_t flags;           // 保留
    uint32_t block_size;      // 块大小
    uint64_t old_size;        // 原文件大小
    uint64_t new_size;        // 新文件大小
    uint32_t num_blocks;      // 块数量
    uint32_t reserved;        // 保留
    uint8_t  old_sha256[32];  // 原文件 SHA256
    uint8_t  new_sha256[32];  // 新文件 SHA256
    
    static constexpr size_t SIZE = 64;
    static constexpr const char* MAGIC = "UEBD";
    static constexpr uint16_t VERSION = 1;
    
    bool is_valid() const;
    void init(uint32_t blk_size, uint64_t old_sz, uint64_t new_sz);
};

#pragma pack(pop)

// ============== 块索引 ==============

struct BlockIndex {
    std::vector<uint64_t> offsets;  // 每个块的偏移量
    
    void read(const uint8_t* data, uint32_t num_blocks);
    void write(std::vector<uint8_t>& output) const;
    size_t size() const { return offsets.size() * sizeof(uint64_t); }
};

} // namespace bindiff
