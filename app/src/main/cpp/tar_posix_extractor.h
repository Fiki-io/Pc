#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace gtasa {

struct RuntimeExtractionResult {
    bool success = false;
    uint32_t extractedFiles = 0;
    uint32_t extractedSymlinks = 0;
    uint32_t extractedDirs = 0;
    uint64_t totalBytesWritten = 0;
    std::string archiveSha256;
    std::string errorMessage;
};

struct InstalledRuntimeVerification {
    bool isVerified = false;
    bool wineBinaryValidElf64 = false;
    bool wowBox64Present = false;
    bool wow64FexPresent = false;
    bool d3d9Present = false;
    bool dsoundPresent = false;
    bool winsockPresent = false;
    bool maliVirglPresent = false;
    bool maliZinkPresent = false;
    uint32_t verifiedModuleCount = 0;
    std::string wineBinaryPath;
    std::string errorMessage;
};

class TarPosixExtractor {
public:
    static RuntimeExtractionResult extractArchivePath(
        const std::string& archivePath,
        const std::string& destDir
    );

    static InstalledRuntimeVerification verifyInstalledRuntime(
        const std::string& runtimeRootDir
    );
};

} // namespace gtasa
