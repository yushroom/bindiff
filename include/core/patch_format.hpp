#pragma once

#include "types.hpp"
#include <cstdint>
#include <string>

namespace bindiff {

// ============== Patch 文件头 ==============

#pragma pack(push, 1)

struct PatchHeader {
    char     magic[4];        // 4 bytes  - "UEBD"
    uint16_t version;         // 2 bytes  - 格式版本 (1)
    uint16_t flags;           // 2 bytes  - 保留
    uint32_t block_size;      // 4 bytes  - 块大小
    uint64_t old_size;        // 8 bytes  - 原文件大小
    uint64_t new_size;        // 8 bytes  - 新文件大小
    uint32_t num_blocks;      // 4 bytes  - 块数量
    uint32_t reserved;        // 4 bytes  - 保留
    uint8_t  old_sha256[32];  // 32 bytes - 原文件 SHA256
    uint8_t  new_sha256[32];  // 32 bytes - 新文件 SHA256
    // 总计: 4+2+2+4+8+8+4+4+32+32 = 100 bytes
    
    static constexpr size_t SIZE = 100;
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
