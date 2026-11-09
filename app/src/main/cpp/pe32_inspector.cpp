#include "pe32_inspector.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace gtasa {

namespace {

constexpr uint16_t DOS_MAGIC_MZ = 0x5A4DU;
constexpr uint32_t PE_SIGNATURE = 0x00004550UL;
constexpr uint16_t IMAGE_FILE_MACHINE_I386_VAL = 0x014CU;
constexpr uint16_t IMAGE_FILE_LARGE_ADDRESS_AWARE_BIT = 0x0020U;
constexpr uint16_t PE32_OPTIONAL_MAGIC = 0x010BU;

struct SectionRange {
    uint32_t virtualAddress;
    uint32_t virtualSize;
    uint32_t rawDataOffset;
    uint32_t rawDataSize;
};

inline uint16_t readU16LE(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8U);
}

inline uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8U) |
           (static_cast<uint32_t>(p[2]) << 16U) |
           (static_cast<uint32_t>(p[3]) << 24U);
}

bool preadExact(int fd, void* buf, size_t count, off_t offset) {
    uint8_t* out = static_cast<uint8_t*>(buf);
    size_t total = 0;
    while (total < count) {
        ssize_t n = pread(fd, out + total, count - total, offset + static_cast<off_t>(total));
        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return false;
        }
        total += static_cast<size_t>(n);
    }
    return true;
}

bool rvaToFileOffset(uint32_t rva, const std::vector<SectionRange>& sections, uint32_t& outOffset) {
    for (const auto& sec : sections) {
        uint32_t span = std::max(sec.virtualSize, sec.rawDataSize);
        if (rva >= sec.virtualAddress && rva < sec.virtualAddress + span) {
            uint32_t delta = rva - sec.virtualAddress;
            if (delta < sec.rawDataSize) {
                outOffset = sec.rawDataOffset + delta;
                return true;
            }
        }
    }
    return false;
}

std::string classifyGtaSaEntryPoint(uint32_t imageBase, uint32_t entryRva) {
    uint32_t va = imageBase + entryRva;
    switch (va) {
        case 0x00824570UL:
            return "1.0_US_HOODLUM_COMPACT";
        case 0x008245B0UL:
            return "1.0_EU_NO_CD";
        case 0x008252FCUL:
        case 0x0082533CUL:
            return "1.01_PATCHED";
        case 0x00858C60UL:
            return "3.0_STEAM_ENC";
        default:
            return "PE32_CUSTOM_ENTRY";
    }
}

} // namespace

Pe32BinaryReport Pe32Inspector::inspectPe32Fd(int fd) {
    Pe32BinaryReport report;
    if (fd < 0) {
        report.errorMessage = "Invalid file descriptor";
        return report;
    }

    struct stat st {};
    if (fstat(fd, &st) != 0 || st.st_size < 128) {
        report.errorMessage = "File too small or unreadable for PE32 format";
        return report;
    }
    report.fileSizeBytes = static_cast<uint64_t>(st.st_size);

    uint8_t dosHeader[64];
    if (!preadExact(fd, dosHeader, sizeof(dosHeader), 0)) {
        report.errorMessage = "Failed to read IMAGE_DOS_HEADER";
        return report;
    }

    uint16_t dosMagic = readU16LE(dosHeader);
    if (dosMagic != DOS_MAGIC_MZ) {
        report.errorMessage = "Missing MZ header signature (not a Windows executable)";
        return report;
    }

    uint32_t peOffset = readU32LE(dosHeader + 0x3C);
    if (peOffset < 64 || static_cast<uint64_t>(peOffset) + 24ULL > report.fileSizeBytes) {
        report.errorMessage = "Corrupt e_lfanew PE header offset";
        return report;
    }

    uint8_t ntFixed[24];
    if (!preadExact(fd, ntFixed, sizeof(ntFixed), static_cast<off_t>(peOffset))) {
        report.errorMessage = "Failed to read IMAGE_NT_HEADERS32";
        return report;
    }

    uint32_t peSig = readU32LE(ntFixed);
    if (peSig != PE_SIGNATURE) {
        report.errorMessage = "Invalid PE\\0\\0 signature";
        return report;
    }

    report.machine = readU16LE(ntFixed + 4);
    report.sectionCount = readU16LE(ntFixed + 6);
    report.timestamp = readU32LE(ntFixed + 8);
    uint16_t sizeOfOptHeader = readU16LE(ntFixed + 20);
    report.characteristics = readU16LE(ntFixed + 22);
    report.isLargeAddressAware = (report.characteristics & IMAGE_FILE_LARGE_ADDRESS_AWARE_BIT) != 0;

    if (report.machine != IMAGE_FILE_MACHINE_I386_VAL) {
        report.errorMessage = "Executable architecture is not x86 32-bit (i386)";
        return report;
    }

    if (sizeOfOptHeader < 96) {
        report.errorMessage = "Optional header too small for PE32";
        return report;
    }

    std::vector<uint8_t> optHeader(sizeOfOptHeader);
    off_t optOffset = static_cast<off_t>(peOffset) + 24;
    if (!preadExact(fd, optHeader.data(), sizeOfOptHeader, optOffset)) {
        report.errorMessage = "Failed to read IMAGE_OPTIONAL_HEADER32";
        return report;
    }

    uint16_t optMagic = readU16LE(optHeader.data());
    if (optMagic != PE32_OPTIONAL_MAGIC) {
        report.errorMessage = "Optional header magic is not PE32 (0x010B)";
        return report;
    }

    report.entryPointRva = readU32LE(optHeader.data() + 16);
    report.imageBase = readU32LE(optHeader.data() + 28);
    report.sizeOfImage = readU32LE(optHeader.data() + 56);
    report.subsystem = readU16LE(optHeader.data() + 68);
    report.detectedVersionCode = classifyGtaSaEntryPoint(report.imageBase, report.entryPointRva);

    uint32_t numRvaAndSizes = readU32LE(optHeader.data() + 92);
    uint32_t importTableRva = 0;
    if (numRvaAndSizes >= 2 && sizeOfOptHeader >= 112) {
        importTableRva = readU32LE(optHeader.data() + 96 + 8);
    }

    std::vector<SectionRange> sections;
    off_t sectionTableOffset = optOffset + static_cast<off_t>(sizeOfOptHeader);
    uint16_t maxSections = std::min<uint16_t>(report.sectionCount, 64U);
    for (uint16_t i = 0; i < maxSections; ++i) {
        uint8_t secBuf[40];
        if (!preadExact(fd, secBuf, sizeof(secBuf), sectionTableOffset + static_cast<off_t>(i * 40))) {
            break;
        }
        SectionRange sr{};
        sr.virtualSize = readU32LE(secBuf + 8);
        sr.virtualAddress = readU32LE(secBuf + 12);
        sr.rawDataSize = readU32LE(secBuf + 16);
        sr.rawDataOffset = readU32LE(secBuf + 20);
        sections.push_back(sr);
    }

    if (importTableRva != 0 && !sections.empty()) {
        uint32_t importFileOffset = 0;
        if (rvaToFileOffset(importTableRva, sections, importFileOffset)) {
            for (int idx = 0; idx < 128; ++idx) {
                uint8_t desc[20];
                if (!preadExact(fd, desc, sizeof(desc), static_cast<off_t>(importFileOffset + idx * 20))) {
                    break;
                }
                uint32_t nameRva = readU32LE(desc + 12);
                uint32_t firstThunk = readU32LE(desc + 16);
                if (nameRva == 0 && firstThunk == 0) {
                    break;
                }
                uint32_t nameOffset = 0;
                if (rvaToFileOffset(nameRva, sections, nameOffset)) {
                    char dllName[64] = {0};
                    if (preadExact(fd, dllName, sizeof(dllName) - 1, static_cast<off_t>(nameOffset))) {
                        dllName[sizeof(dllName) - 1] = '\0';
                        std::string cleanName;
                        for (size_t c = 0; c < sizeof(dllName) && dllName[c] != '\0'; ++c) {
                            unsigned char ch = static_cast<unsigned char>(dllName[c]);
                            if (std::isprint(ch)) {
                                cleanName.push_back( static_cast<char>(std::tolower(ch)) );
                            }
                        }
                        if (!cleanName.empty()) {
                            report.importedDlls.push_back(cleanName);
                        }
                    }
                }
            }
        }
    }

    report.isValidPe32 = true;
    return report;
}

Pe32BinaryReport Pe32Inspector::inspectPe32Path(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        Pe32BinaryReport report;
        report.errorMessage = std::string("Cannot open file: ") + std::strerror(errno);
        return report;
    }
    Pe32BinaryReport report = inspectPe32Fd(fd);
    close(fd);
    return report;
}

bool Pe32Inspector::patchLargeAddressAwarePath(
    const std::string& path,
    bool enable,
    bool& outVerifiedState,
    std::string& outError
) {
    int fd = open(path.c_str(), O_RDWR | O_CLOEXEC);
    if (fd < 0) {
        outError = std::string("Cannot open executable for patching: ") + std::strerror(errno);
        return false;
    }

    uint8_t dosHeader[64];
    if (!preadExact(fd, dosHeader, sizeof(dosHeader), 0) || readU16LE(dosHeader) != DOS_MAGIC_MZ) {
        close(fd);
        outError = "Invalid MZ header";
        return false;
    }

    uint32_t peOffset = readU32LE(dosHeader + 0x3C);
    uint8_t ntFixed[24];
    if (!preadExact(fd, ntFixed, sizeof(ntFixed), static_cast<off_t>(peOffset)) ||
        readU32LE(ntFixed) != PE_SIGNATURE ||
        readU16LE(ntFixed + 4) != IMAGE_FILE_MACHINE_I386_VAL) {
        close(fd);
        outError = "Invalid PE32 i386 header";
        return false;
    }

    uint16_t characteristics = readU16LE(ntFixed + 22);
    if (enable) {
        characteristics |= IMAGE_FILE_LARGE_ADDRESS_AWARE_BIT;
    } else {
        characteristics &= static_cast<uint16_t>(~IMAGE_FILE_LARGE_ADDRESS_AWARE_BIT);
    }

    uint8_t newChars[2] = {
        static_cast<uint8_t>(characteristics & 0xFFU),
        static_cast<uint8_t>((characteristics >> 8U) & 0xFFU)
    };

    off_t charsOffset = static_cast<off_t>(peOffset) + 22;
    if (pwrite(fd, newChars, sizeof(newChars), charsOffset) != sizeof(newChars)) {
        outError = std::string("Failed writing Characteristics field: ") + std::strerror(errno);
        close(fd);
        return false;
    }

    if (fsync(fd) != 0) {
        outError = std::string("fsync failed: ") + std::strerror(errno);
        close(fd);
        return false;
    }

    uint8_t verifyBuf[2] = {0, 0};
    if (!preadExact(fd, verifyBuf, sizeof(verifyBuf), charsOffset)) {
        outError = "Failed reading back patched Characteristics field";
        close(fd);
        return false;
    }
    close(fd);

    uint16_t verifiedChars = readU16LE(verifyBuf);
    outVerifiedState = (verifiedChars & IMAGE_FILE_LARGE_ADDRESS_AWARE_BIT) != 0;
    if (outVerifiedState != enable) {
        outError = "Verification mismatch after patching LARGE_ADDRESS_AWARE bit";
        return false;
    }
    return true;
}

RenderWareImgReport Pe32Inspector::inspectRenderWareImgFd(int fd) {
    RenderWareImgReport report;
    if (fd < 0) {
        report.errorMessage = "Invalid file descriptor";
        return report;
    }

    struct stat st {};
    if (fstat(fd, &st) != 0 || st.st_size < 8) {
        report.errorMessage = "IMG archive unreadable or empty";
        return report;
    }
    report.fileSizeBytes = static_cast<uint64_t>(st.st_size);

    uint8_t header[8];
    if (!preadExact(fd, header, sizeof(header), 0)) {
        report.errorMessage = "Failed reading RenderWare IMG header";
        return report;
    }

    if (std::memcmp(header, "VER2", 4) != 0) {
        report.errorMessage = "Signature is not RenderWare IMG v2 (VER2)";
        return report;
    }

    uint32_t count = readU32LE(header + 4);
    if (count == 0 || count > 250000U) {
        report.errorMessage = "Invalid entry count in VER2 IMG header";
        return report;
    }

    uint64_t dirTableBytes = 8ULL + static_cast<uint64_t>(count) * 32ULL;
    if (dirTableBytes > report.fileSizeBytes) {
        report.errorMessage = "IMG directory table exceeds file size";
        return report;
    }

    report.totalEntries = count;
    constexpr size_t BATCH_ENTRIES = 512;
    std::vector<uint8_t> batch(BATCH_ENTRIES * 32);
    uint64_t maxEndByte = dirTableBytes;

    uint32_t processed = 0;
    while (processed < count) {
        uint32_t toRead = std::min<uint32_t>(count - processed, BATCH_ENTRIES);
        off_t offset = static_cast<off_t>(8ULL + static_cast<uint64_t>(processed) * 32ULL);
        if (!preadExact(fd, batch.data(), toRead * 32U, offset)) {
            report.errorMessage = "Failed reading IMG directory entries";
            return report;
        }

        for (uint32_t i = 0; i < toRead; ++i) {
            const uint8_t* entry = batch.data() + i * 32U;
            uint32_t sectorOffset = readU32LE(entry);
            uint16_t streamingSectors = readU16LE(entry + 4);
            uint64_t endByte = (static_cast<uint64_t>(sectorOffset) + streamingSectors) * 2048ULL;
            if (endByte > maxEndByte) {
                maxEndByte = endByte;
            }

            char nameBuf[25] = {0};
            std::memcpy(nameBuf, entry + 8, 24);
            std::string name(nameBuf);
            auto dotPos = name.find_last_of('.');
            if (dotPos != std::string::npos) {
                std::string ext = name.substr(dotPos + 1);
                for (char& c : ext) {
                    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                }
                if (ext == "dff") ++report.dffModelCount;
                else if (ext == "txd") ++report.txdTextureCount;
                else if (ext == "col") ++report.colCollisionCount;
                else if (ext == "ifp") ++report.ifpAnimCount;
            }
        }
        processed += toRead;
    }

    report.sectorBoundsValid = (maxEndByte <= report.fileSizeBytes);
    report.isValidVer2 = report.sectorBoundsValid;
    if (!report.sectorBoundsValid) {
        report.errorMessage = "Archive truncated: entry sector offset exceeds IMG file size";
    }
    return report;
}

RenderWareImgReport Pe32Inspector::inspectRenderWareImgPath(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        RenderWareImgReport report;
        report.errorMessage = std::string("Cannot open IMG archive: ") + std::strerror(errno);
        return report;
    }
    RenderWareImgReport report = inspectRenderWareImgFd(fd);
    close(fd);
    return report;
}

} // namespace gtasa
