#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gtasa {

struct Pe32BinaryReport {
    bool isValidPe32 = false;
    uint16_t machine = 0;
    uint16_t sectionCount = 0;
    uint32_t timestamp = 0;
    uint16_t characteristics = 0;
    bool isLargeAddressAware = false;
    uint32_t entryPointRva = 0;
    uint32_t imageBase = 0;
    uint32_t sizeOfImage = 0;
    uint16_t subsystem = 0;
    uint64_t fileSizeBytes = 0;
    std::string detectedVersionCode;
    std::vector<std::string> importedDlls;
    std::string errorMessage;
};

struct RenderWareImgReport {
    bool isValidVer2 = false;
    uint32_t totalEntries = 0;
    uint32_t dffModelCount = 0;
    uint32_t txdTextureCount = 0;
    uint32_t colCollisionCount = 0;
    uint32_t ifpAnimCount = 0;
    uint64_t fileSizeBytes = 0;
    bool sectorBoundsValid = false;
    std::string errorMessage;
};

class Pe32Inspector {
public:
    static Pe32BinaryReport inspectPe32Fd(int fd);
    static Pe32BinaryReport inspectPe32Path(const std::string& path);

    static bool patchLargeAddressAwarePath(
        const std::string& path,
        bool enable,
        bool& outVerifiedState,
        std::string& outError
    );

    static RenderWareImgReport inspectRenderWareImgFd(int fd);
    static RenderWareImgReport inspectRenderWareImgPath(const std::string& path);
};

} // namespace gtasa
