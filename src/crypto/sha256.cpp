#include "crypto/sha256.hpp"
#include <cstring>
#include <iomanip>
#include <sstream>

namespace bindiff {

// SHA-256 常量
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

// 辅助宏
#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
#define CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define EP1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define SIG0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ ((x) >> 3))
#define SIG1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ ((x) >> 10))

SHA256::SHA256() {
    reset();
}

void SHA256::reset() {
    bitlen_ = 0;
    buffer_len_ = 0;
    std::memset(buffer_, 0, sizeof(buffer_));
    
    // 初始哈希值
    state_[0] = 0x6a09e667;
    state_[1] = 0xbb67ae85;
    state_[2] = 0x3c6ef372;
    state_[3] = 0xa54ff53a;
    state_[4] = 0x510e527f;
    state_[5] = 0x9b05688c;
    state_[6] = 0x1f83d9ab;
    state_[7] = 0x5be0cd19;
}

void SHA256::update(const uint8_t* data, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        buffer_[buffer_len_++] = data[i];
        if (buffer_len_ == 64) {
            process_block(buffer_);
            bitlen_ += 512;
            buffer_len_ = 0;
        }
    }
}

void SHA256::update(const std::string& str) {
    update(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

std::array<uint8_t, 32> SHA256::finalize() {
    std::array<uint8_t, 32> hash;
    
    uint32_t i = buffer_len_;
    
    // 填充
    if (buffer_len_ < 56) {
        buffer_[i++] = 0x80;
        while (i < 56) {
            buffer_[i++] = 0x00;
        }
    } else {
        buffer_[i++] = 0x80;
        while (i < 64) {
            buffer_[i++] = 0x00;
        }
        process_block(buffer_);
        std::memset(buffer_, 0, 56);
    }
    
    // 追加长度
    bitlen_ += buffer_len_ * 8;
    buffer_[63] = bitlen_;
    buffer_[62] = bitlen_ >> 8;
    buffer_[61] = bitlen_ >> 16;
    buffer_[60] = bitlen_ >> 24;
    buffer_[59] = bitlen_ >> 32;
    buffer_[58] = bitlen_ >> 40;
    buffer_[57] = bitlen_ >> 48;
    buffer_[56] = bitlen_ >> 56;
    process_block(buffer_);
    
    // 输出
    for (i = 0; i < 8; ++i) {
        hash[i * 4] = (state_[i] >> 24) & 0xFF;
        hash[i * 4 + 1] = (state_[i] >> 16) & 0xFF;
        hash[i * 4 + 2] = (state_[i] >> 8) & 0xFF;
        hash[i * 4 + 3] = state_[i] & 0xFF;
    }
    
    return hash;
}

void SHA256::process_block(const uint8_t* block) {
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;
    
    // 准备消息调度
    for (int i = 0; i < 16; ++i) {
        W[i] = (block[i * 4] << 24) | (block[i * 4 + 1] << 16) |
               (block[i * 4 + 2] << 8) | block[i * 4 + 3];
    }
    for (int i = 16; i < 64; ++i) {
        W[i] = SIG1(W[i - 2]) + W[i - 7] + SIG0(W[i - 15]) + W[i - 16];
    }
    
    // 初始化工作变量
    a = state_[0];
    b = state_[1];
    c = state_[2];
    d = state_[3];
    e = state_[4];
    f = state_[5];
    g = state_[6];
    h = state_[7];
    
    // 主循环
    for (int i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e, f, g) + K[i] + W[i];
        t2 = EP0(a) + MAJ(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    
    // 更新状态
    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

std::array<uint8_t, 32> SHA256::compute(const uint8_t* data, size_t size) {
    SHA256 sha;
    sha.update(data, size);
    return sha.finalize();
}

std::array<uint8_t, 32> SHA256::compute(const std::string& str) {
    return compute(reinterpret_cast<const uint8_t*>(str.data()), str.size());
}

std::array<uint8_t, 32> SHA256::compute_file(const std::string& path) {
    SHA256 sha;
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return {};
    }
    
    uint8_t buffer[8192];
    size_t n;
    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) {
        sha.update(buffer, n);
    }
    
    fclose(f);
    return sha.finalize();
}

std::string SHA256::to_hex(const std::array<uint8_t, 32>& hash) {
    return to_hex(hash.data(), hash.size());
}

std::string SHA256::to_hex(const uint8_t* data, size_t size) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    return oss.str();
}

} // namespace bindiff
