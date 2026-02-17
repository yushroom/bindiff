#pragma once

#include "types.hpp"
#include "core/patch_format.hpp"
#include "io/mmap_file.hpp"
#include "compress/compressor.hpp"
#include <memory>

namespace bindiff {

// ============== 补丁引擎 ==============

class PatchEngine {
public:
    PatchEngine(const PatchOptions& options = {});
    ~PatchEngine();
    
    Result apply_patch(
        const std::string& old_path,
        const std::string& patch_path,
        const std::string& new_path,
        ProgressCallback* callback = nullptr
    );

private:
    bool read_patch_header(const std::string& path);
    bool reconstruct_all_blocks(
        MMapFile& old_file,
        const std::string& patch_path,
        const std::string& output_path,
        ProgressCallback* callback
    );
    bool reconstruct_block_from_data(
        MMapFile& old_file,
        const std::vector<uint8_t>& compressed_data,
        uint8_t* output,
        size_t output_size
    );
    
    PatchOptions options_;
    PatchInfo patch_info_;
    std::vector<uint64_t> block_offsets_;
};

} // namespace bindiff
