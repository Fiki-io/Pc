#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace gtasa {

class Sha256Hasher {
public:
    Sha256Hasher();
    void update(const uint8_t* data, size_t len);
    std::string finalizeHex();

    static bool computeFileSha256(
        int fd,
        std::string& outHex,
        uint64_t& outBytesRead,
        std::string& outError
    );

    static bool computePathSha256(
        const std::string& path,
        std::string& outHex,
        uint64_t& outBytesRead,
        std::string& outError
    );

private:
    void transform(const uint8_t block[64]);

    uint32_t state_[8];
    uint64_t bitCount_;
    uint8_t buffer_[64];
    size_t bufferLen_;
};

} // namespace gtasa
