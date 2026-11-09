#include "sha256_stream.h"

#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace gtasa {

namespace {

constexpr uint32_t K[64] = {
    0x428a2f98UL, 0x71374491UL, 0xb5c0fbcfUL, 0xe9b5dba5UL,
    0x3956c25bUL, 0x59f111f1UL, 0x923f82a4UL, 0xab1c5ed5UL,
    0xd807aa98UL, 0x12835b01UL, 0x243185beUL, 0x550c7dc3UL,
    0x72be5d74UL, 0x80deb1feUL, 0x9bdc06a7UL, 0xc19bf174UL,
    0xe49b69c1UL, 0xefbe4786UL, 0x0fc19dc6UL, 0x240ca1ccUL,
    0x2de92c6fUL, 0x4a7484aaUL, 0x5cb0a9dcUL, 0x76f988daUL,
    0x983e5152UL, 0xa831c66dUL, 0xb00327c8UL, 0xbf597fc7UL,
    0xc6e00bf3UL, 0xd5a79147UL, 0x06ca6351UL, 0x14292967UL,
    0x27b70a85UL, 0x2e1b2138UL, 0x4d2c6dfcUL, 0x53380d13UL,
    0x650a7354UL, 0x766a0abbUL, 0x81c2c92eUL, 0x92722c85UL,
    0xa2bfe8a1UL, 0xa81a664bUL, 0xc24b8b70UL, 0xc76c51a3UL,
    0xd192e819UL, 0xd6990624UL, 0xf40e3585UL, 0x106aa070UL,
    0x19a4c116UL, 0x1e376c08UL, 0x2748774cUL, 0x34b0bcb5UL,
    0x391c0cb3UL, 0x4ed8aa4aUL, 0x5b9cca4fUL, 0x682e6ff3UL,
    0x748f82eeUL, 0x78a5636fUL, 0x84c87814UL, 0x8cc70208UL,
    0x90befffaUL, 0xa4506cebUL, 0xbef9a3f7UL, 0xc67178f2UL
};

inline uint32_t rotr(uint32_t x, uint32_t n) {
    return (x >> n) | (x << (32U - n));
}

} // namespace

Sha256Hasher::Sha256Hasher()
    : bitCount_(0), bufferLen_(0) {
    state_[0] = 0x6a09e667UL;
    state_[1] = 0xbb67ae85UL;
    state_[2] = 0x3c6ef372UL;
    state_[3] = 0xa54ff53aUL;
    state_[4] = 0x510e527fUL;
    state_[5] = 0x9b05688cUL;
    state_[6] = 0x1f83d9abUL;
    state_[7] = 0x5be0cd19UL;
}

void Sha256Hasher::transform(const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24U) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16U) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8U) |
               (static_cast<uint32_t>(block[i * 4 + 3]));
    }
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr(w[i - 15], 7U) ^ rotr(w[i - 15], 18U) ^ (w[i - 15] >> 3U);
        uint32_t s1 = rotr(w[i - 2], 17U) ^ rotr(w[i - 2], 19U) ^ (w[i - 2] >> 10U);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state_[0];
    uint32_t b = state_[1];
    uint32_t c = state_[2];
    uint32_t d = state_[3];
    uint32_t e = state_[4];
    uint32_t f = state_[5];
    uint32_t g = state_[6];
    uint32_t h = state_[7];

    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t temp1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256Hasher::update(const uint8_t* data, size_t len) {
    size_t i = 0;
    while (i < len) {
        size_t copyLen = 64U - bufferLen_;
        if (copyLen > (len - i)) {
            copyLen = len - i;
        }
        std::memcpy(buffer_ + bufferLen_, data + i, copyLen);
        bufferLen_ += copyLen;
        i += copyLen;
        if (bufferLen_ == 64U) {
            transform(buffer_);
            bitCount_ += 512ULL;
            bufferLen_ = 0;
        }
    }
}

std::string Sha256Hasher::finalizeHex() {
    uint64_t totalBits = bitCount_ + static_cast<uint64_t>(bufferLen_) * 8ULL;
    buffer_[bufferLen_++] = 0x80U;
    if (bufferLen_ > 56U) {
        while (bufferLen_ < 64U) {
            buffer_[bufferLen_++] = 0x00U;
        }
        transform(buffer_);
        bufferLen_ = 0;
    }
    while (bufferLen_ < 56U) {
        buffer_[bufferLen_++] = 0x00U;
    }
    for (int i = 7; i >= 0; --i) {
        buffer_[bufferLen_++] = static_cast<uint8_t>((totalBits >> (i * 8)) & 0xFFU);
    }
    transform(buffer_);

    static constexpr char HEX_DIGITS[] = "0123456789abcdef";
    std::string hex;
    hex.reserve(64);
    for (int i = 0; i < 8; ++i) {
        for (int shift = 28; shift >= 0; shift -= 4) {
            hex.push_back(HEX_DIGITS[(state_[i] >> shift) & 0x0FU]);
        }
    }
    return hex;
}

bool Sha256Hasher::computeFileSha256(
    int fd,
    std::string& outHex,
    uint64_t& outBytesRead,
    std::string& outError
) {
    if (fd < 0) {
        outError = "Invalid file descriptor";
        return false;
    }
    if (lseek(fd, 0, SEEK_SET) < 0) {
        outError = std::string("lseek failed: ") + std::strerror(errno);
        return false;
    }

    Sha256Hasher hasher;
    uint8_t chunk[65536];
    outBytesRead = 0;

    while (true) {
        ssize_t n = read(fd, chunk, sizeof(chunk));
        if (n < 0) {
            if (errno == EINTR) continue;
            outError = std::string("read failed: ") + std::strerror(errno);
            return false;
        }
        if (n == 0) break;
        hasher.update(chunk, static_cast<size_t>(n));
        outBytesRead += static_cast<uint64_t>(n);
    }

    outHex = hasher.finalizeHex();
    return true;
}

bool Sha256Hasher::computePathSha256(
    const std::string& path,
    std::string& outHex,
    uint64_t& outBytesRead,
    std::string& outError
) {
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        outError = std::string("Cannot open ") + path + ": " + std::strerror(errno);
        return false;
    }
    bool ok = computeFileSha256(fd, outHex, outBytesRead, outError);
    close(fd);
    return ok;
}

} // namespace gtasa
