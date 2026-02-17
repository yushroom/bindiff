#include "core/patch_format.hpp"
#include <cstring>

namespace bindiff {

bool PatchHeader::is_valid() const {
    return std::memcmp(magic, MAGIC, 4) == 0 && version == VERSION;
}

void PatchHeader::init(uint32_t blk_size, uint64_t old_sz, uint64_t new_sz) {
    std::memcpy(magic, MAGIC, 4);
    version = VERSION;
    flags = 0;
    block_size = blk_size;
    old_size = old_sz;
    new_size = new_sz;
    num_blocks = 0;
    reserved = 0;
    std::memset(old_sha256, 0, 32);
    std::memset(new_sha256, 0, 32);
}

void BlockIndex::read(const uint8_t* data, uint32_t num_blocks) {
    offsets.resize(num_blocks);
    std::memcpy(offsets.data(), data, num_blocks * sizeof(uint64_t));
}

void BlockIndex::write(std::vector<uint8_t>& output) const {
    size_t size = offsets.size() * sizeof(uint64_t);
    output.resize(size);
    std::memcpy(output.data(), offsets.data(), size);
}

} // namespace bindiff
