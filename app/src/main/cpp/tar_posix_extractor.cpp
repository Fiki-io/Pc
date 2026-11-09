#include "tar_posix_extractor.h"
#include "sha256_stream.h"
#include "xz.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <zlib.h>

namespace gtasa {

namespace {

class ByteStreamReader {
public:
    virtual ~ByteStreamReader() = default;
    virtual ssize_t readBytes(void* buf, size_t len) = 0;
    virtual std::string lastError() const { return ""; }
};

class FdStreamReader final : public ByteStreamReader {
public:
    explicit FdStreamReader(int fd) : fd_(fd) {}
    ssize_t readBytes(void* buf, size_t len) override {
        size_t total = 0;
        uint8_t* out = static_cast<uint8_t*>(buf);
        while (total < len) {
            ssize_t n = read(fd_, out + total, len - total);
            if (n < 0) {
                if (errno == EINTR) continue;
                lastError_ = std::string("FD read error: ") + std::strerror(errno);
                return -1;
            }
            if (n == 0) break;
            total += static_cast<size_t>(n);
        }
        return static_cast<ssize_t>(total);
    }
    std::string lastError() const override { return lastError_; }
private:
    int fd_;
    std::string lastError_;
};

class GzipStreamReader final : public ByteStreamReader {
public:
    explicit GzipStreamReader(gzFile gz) : gz_(gz) {}
    ~GzipStreamReader() override {
        if (gz_ != nullptr) {
            gzclose(gz_);
        }
    }
    ssize_t readBytes(void* buf, size_t len) override {
        size_t total = 0;
        uint8_t* out = static_cast<uint8_t*>(buf);
        while (total < len) {
            int n = gzread(gz_, out + total, static_cast<unsigned int>(len - total));
            if (n < 0) {
                int errnum = 0;
                const char* msg = gzerror(gz_, &errnum);
                lastError_ = msg ? msg : "Gzip read error";
                return -1;
            }
            if (n == 0) break;
            total += static_cast<size_t>(n);
        }
        return static_cast<ssize_t>(total);
    }
    std::string lastError() const override { return lastError_; }
private:
    gzFile gz_;
    std::string lastError_;
};

class XzStreamReader final : public ByteStreamReader {
public:
    explicit XzStreamReader(int fd) : fd_(fd), inBuf_(65536) {
        static std::once_flag crcInitFlag;
        std::call_once(crcInitFlag, []() {
            xz_crc32_init();
            xz_crc64_init();
        });
        dec_ = xz_dec_init(XZ_DYNALLOC, 1U << 27); // up to 128 MiB LZMA2 dictionary
        b_.in = inBuf_.data();
        b_.in_pos = 0;
        b_.in_size = 0;
        b_.out = nullptr;
        b_.out_pos = 0;
        b_.out_size = 0;
    }

    ~XzStreamReader() override {
        if (dec_ != nullptr) {
            xz_dec_end(dec_);
        }
        if (fd_ >= 0) {
            close(fd_);
        }
    }

    ssize_t readBytes(void* buf, size_t len) override {
        if (dec_ == nullptr) {
            lastError_ = "xz_dec_init failed to allocate LZMA2 decoder state";
            return -1;
        }
        if (streamEnded_) {
            return 0;
        }
        b_.out = static_cast<uint8_t*>(buf);
        b_.out_pos = 0;
        b_.out_size = len;

        while (b_.out_pos < b_.out_size) {
            if (b_.in_pos == b_.in_size && !inputEof_) {
                ssize_t n = read(fd_, inBuf_.data(), inBuf_.size());
                if (n < 0) {
                    if (errno == EINTR) continue;
                    lastError_ = std::string("XZ file read error: ") + std::strerror(errno);
                    return -1;
                }
                if (n == 0) {
                    inputEof_ = true;
                } else {
                    b_.in_pos = 0;
                    b_.in_size = static_cast<size_t>(n);
                }
            }

            enum xz_ret ret = xz_dec_catrun(dec_, &b_, inputEof_ ? 1 : 0);
            if (ret == XZ_STREAM_END) {
                streamEnded_ = true;
                break;
            }
            if (ret != XZ_OK && ret != XZ_UNSUPPORTED_CHECK) {
                lastError_ = std::string("XZ stream error (code ") +
                             std::to_string(static_cast<int>(ret)) + ")";
                return -1;
            }
        }
        return static_cast<ssize_t>(b_.out_pos);
    }

    std::string lastError() const override { return lastError_; }

private:
    int fd_;
    struct xz_dec* dec_ = nullptr;
    struct xz_buf b_{};
    std::vector<uint8_t> inBuf_;
    bool inputEof_ = false;
    bool streamEnded_ = false;
    std::string lastError_;
};

bool mkdirRecursive(const std::string& dirPath, mode_t mode);

bool mkdirSingleComponent(const char* path, mode_t mode) {
    if (mkdir(path, mode) == 0) {
        return true;
    }
    if (errno != EEXIST) {
        return false;
    }
    struct stat st {};
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        return true;
    }
    if (lstat(path, &st) == 0 && S_ISLNK(st.st_mode)) {
        char linkBuf[4096] = {0};
        ssize_t r = readlink(path, linkBuf, sizeof(linkBuf) - 1);
        if (r > 0) {
            linkBuf[r] = '\0';
            std::string resolvedTarget;
            if (linkBuf[0] == '/') {
                resolvedTarget = linkBuf;
            } else {
                std::string parent(path);
                auto slash = parent.find_last_of('/');
                resolvedTarget = (slash != std::string::npos)
                    ? (parent.substr(0, slash) + "/" + linkBuf)
                    : std::string(linkBuf);
            }
            return mkdirRecursive(resolvedTarget, mode);
        }
    }
    return false;
}

bool mkdirRecursive(const std::string& dirPath, mode_t mode) {
    if (dirPath.empty()) return false;
    char tmp[4096];
    std::snprintf(tmp, sizeof(tmp), "%s", dirPath.c_str());
    size_t len = std::strlen(tmp);
    if (len > 1 && tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }
    for (char* p = tmp + 1; *p != '\0'; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (!mkdirSingleComponent(tmp, mode)) {
                return false;
            }
            *p = '/';
        }
    }
    return mkdirSingleComponent(tmp, mode);
}

bool ensureParentDir(const std::string& fullPath) {
    auto slash = fullPath.find_last_of('/');
    if (slash == std::string::npos || slash == 0) return true;
    return mkdirRecursive(fullPath.substr(0, slash), 0755);
}

uint64_t parseTarOctalOrBinary(const char* field, size_t len) {
    const uint8_t* u = reinterpret_cast<const uint8_t*>(field);
    if ((u[0] & 0x80U) != 0) {
        uint64_t val = 0;
        for (size_t i = 1; i < len; ++i) {
            val = (val << 8U) | u[i];
        }
        return val;
    }
    uint64_t val = 0;
    for (size_t i = 0; i < len && field[i] != '\0'; ++i) {
        if (field[i] >= '0' && field[i] <= '7') {
            val = (val << 3U) | static_cast<uint64_t>(field[i] - '0');
        }
    }
    return val;
}

bool isZeroBlock(const uint8_t block[512]) {
    for (size_t i = 0; i < 512; ++i) {
        if (block[i] != 0) return false;
    }
    return true;
}

std::string sanitizeRelativeTarPath(const std::string& rawPath) {
    std::string p = rawPath;
    while (p.rfind("./", 0) == 0) {
        p.erase(0, 2);
    }
    while (!p.empty() && p.front() == '/') {
        p.erase(0, 1);
    }
    while (!p.empty() && p.back() == '/') {
        p.pop_back();
    }
    if (p.empty() || p == "." || p.find("..") != std::string::npos) {
        return "";
    }
    return p;
}

void parsePaxExtendedRecords(
    const std::string& payload,
    std::string& nextLongName,
    std::string& nextLongLink
) {
    size_t pos = 0;
    while (pos < payload.size()) {
        size_t spacePos = payload.find(' ', pos);
        if (spacePos == std::string::npos) break;
        size_t recLen = static_cast<size_t>(std::strtoul(payload.c_str() + pos, nullptr, 10));
        if (recLen == 0 || pos + recLen > payload.size()) break;
        size_t kvStart = spacePos + 1;
        size_t kvEnd = pos + recLen;
        if (kvEnd > kvStart && payload[kvEnd - 1] == '\n') {
            --kvEnd;
        }
        std::string kv = payload.substr(kvStart, kvEnd - kvStart);
        size_t eqPos = kv.find('=');
        if (eqPos != std::string::npos) {
            std::string key = kv.substr(0, eqPos);
            std::string val = kv.substr(eqPos + 1);
            if (key == "path") {
                nextLongName = val;
            } else if (key == "linkpath") {
                nextLongLink = val;
            }
        }
        pos += recLen;
    }
}

bool extractTarStream(
    ByteStreamReader& reader,
    const std::string& destDir,
    RuntimeExtractionResult& outResult
) {
    uint8_t header[512];
    std::string nextLongName;
    std::string nextLongLink;

    while (true) {
        ssize_t n = reader.readBytes(header, sizeof(header));
        if (n < 0) {
            outResult.errorMessage = reader.lastError().empty()
                ? "Stream read error while parsing TAR header"
                : reader.lastError();
            return false;
        }
        if (n == 0) break;
        if (n != sizeof(header)) {
            outResult.errorMessage = "Truncated TAR header block";
            return false;
        }
        if (isZeroBlock(header)) {
            break;
        }

        uint64_t fileSize = parseTarOctalOrBinary(reinterpret_cast<const char*>(header + 124), 12);
        mode_t fileMode = static_cast<mode_t>(parseTarOctalOrBinary(reinterpret_cast<const char*>(header + 100), 8));
        char typeFlag = static_cast<char>(header[156]);

        if (typeFlag == 'L' || typeFlag == 'K') {
            std::string metaBuf(fileSize, '\0');
            size_t aligned = ((fileSize + 511ULL) / 512ULL) * 512ULL;
            uint64_t remaining = aligned;
            size_t offset = 0;
            uint8_t scratch[512];
            while (remaining > 0) {
                if (reader.readBytes(scratch, sizeof(scratch)) != sizeof(scratch)) {
                    outResult.errorMessage = reader.lastError().empty()
                        ? "Unexpected EOF reading GNU long name/link payload"
                        : reader.lastError();
                    return false;
                }
                size_t copyBytes = (offset < fileSize)
                    ? std::min<size_t>(512U, static_cast<size_t>(fileSize - offset))
                    : 0U;
                if (copyBytes > 0) {
                    std::memcpy(&metaBuf[offset], scratch, copyBytes);
                    offset += copyBytes;
                }
                remaining -= 512ULL;
            }
            while (!metaBuf.empty() && metaBuf.back() == '\0') {
                metaBuf.pop_back();
            }
            if (typeFlag == 'L') {
                nextLongName = metaBuf;
            } else {
                nextLongLink = metaBuf;
            }
            continue;
        }

        if (typeFlag == 'x' || typeFlag == 'g') {
            std::string paxBuf(fileSize, '\0');
            size_t aligned = ((fileSize + 511ULL) / 512ULL) * 512ULL;
            uint64_t remaining = aligned;
            size_t offset = 0;
            uint8_t scratch[512];
            while (remaining > 0) {
                if (reader.readBytes(scratch, sizeof(scratch)) != sizeof(scratch)) {
                    outResult.errorMessage = reader.lastError().empty()
                        ? "Unexpected EOF reading PAX extended header"
                        : reader.lastError();
                    return false;
                }
                size_t copyBytes = (offset < fileSize)
                    ? std::min<size_t>(512U, static_cast<size_t>(fileSize - offset))
                    : 0U;
                if (copyBytes > 0) {
                    std::memcpy(&paxBuf[offset], scratch, copyBytes);
                    offset += copyBytes;
                }
                remaining -= 512ULL;
            }
            if (typeFlag == 'x') {
                parsePaxExtendedRecords(paxBuf, nextLongName, nextLongLink);
            }
            continue;
        }

        char nameField[101] = {0};
        std::memcpy(nameField, header, 100);
        char prefixField[156] = {0};
        std::memcpy(prefixField, header + 345, 155);

        std::string entryPath;
        if (!nextLongName.empty()) {
            entryPath = nextLongName;
            nextLongName.clear();
        } else if (prefixField[0] != '\0') {
            entryPath = std::string(prefixField) + "/" + std::string(nameField);
        } else {
            entryPath = std::string(nameField);
        }

        char linkField[101] = {0};
        std::memcpy(linkField, header + 157, 100);
        std::string linkTarget = !nextLongLink.empty() ? nextLongLink : std::string(linkField);
        nextLongLink.clear();

        std::string cleanRel = sanitizeRelativeTarPath(entryPath);
        if (cleanRel.empty()) {
            size_t aligned = ((fileSize + 511ULL) / 512ULL) * 512ULL;
            uint8_t scratch[512];
            while (aligned > 0) {
                if (reader.readBytes(scratch, sizeof(scratch)) != sizeof(scratch)) {
                    outResult.errorMessage = reader.lastError().empty()
                        ? "Unexpected EOF skipping root/unsafe entry"
                        : reader.lastError();
                    return false;
                }
                aligned -= 512ULL;
            }
            continue;
        }

        std::string fullDestPath = destDir + "/" + cleanRel;

        if (typeFlag == '5') {
            if (!mkdirRecursive(fullDestPath, 0755)) {
                outResult.errorMessage = "Failed creating directory: " + fullDestPath;
                return false;
            }
            ++outResult.extractedDirs;
        } else if (typeFlag == '2') {
            ensureParentDir(fullDestPath);
            unlink(fullDestPath.c_str());
            if (symlink(linkTarget.c_str(), fullDestPath.c_str()) != 0) {
                outResult.errorMessage = std::string("symlink failed for ") + fullDestPath + ": " + std::strerror(errno);
                return false;
            }
            ++outResult.extractedSymlinks;
        } else if (typeFlag == '1') {
            std::string cleanTarget = sanitizeRelativeTarPath(linkTarget);
            std::string fullTarget = destDir + "/" + cleanTarget;
            ensureParentDir(fullDestPath);
            unlink(fullDestPath.c_str());
            if (link(fullTarget.c_str(), fullDestPath.c_str()) != 0) {
                symlink(linkTarget.c_str(), fullDestPath.c_str());
            }
            ++outResult.extractedSymlinks;
        } else if (typeFlag == '0' || typeFlag == '\0') {
            ensureParentDir(fullDestPath);
            unlink(fullDestPath.c_str());
            mode_t openMode = (fileMode & 0111) ? 0755 : 0644;
            if (cleanRel.find("bin/") != std::string::npos || cleanRel.find(".so") != std::string::npos) {
                openMode = 0755;
            }
            int outFd = open(fullDestPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, openMode);
            if (outFd < 0) {
                outResult.errorMessage = std::string("Cannot create file ") + fullDestPath + ": " + std::strerror(errno);
                return false;
            }

            uint64_t remainingFile = fileSize;
            uint8_t buf[32768];
            while (remainingFile > 0) {
                size_t toRead = static_cast<size_t>(std::min<uint64_t>(remainingFile, sizeof(buf)));
                ssize_t got = reader.readBytes(buf, toRead);
                if (got != static_cast<ssize_t>(toRead)) {
                    close(outFd);
                    outResult.errorMessage = reader.lastError().empty()
                        ? ("Truncated payload extracting " + cleanRel)
                        : reader.lastError();
                    return false;
                }
                size_t written = 0;
                while (written < toRead) {
                    ssize_t w = write(outFd, buf + written, toRead - written);
                    if (w < 0) {
                        if (errno == EINTR) continue;
                        close(outFd);
                        outResult.errorMessage = std::string("Write error on ") + fullDestPath;
                        return false;
                    }
                    written += static_cast<size_t>(w);
                }
                remainingFile -= toRead;
                outResult.totalBytesWritten += toRead;
            }
            fchmod(outFd, openMode);
            close(outFd);

            size_t pad = (512U - static_cast<size_t>(fileSize % 512ULL)) % 512U;
            if (pad > 0) {
                uint8_t padBuf[512];
                if (reader.readBytes(padBuf, pad) != static_cast<ssize_t>(pad)) {
                    outResult.errorMessage = reader.lastError().empty()
                        ? "Truncated TAR block padding"
                        : reader.lastError();
                    return false;
                }
            }
            ++outResult.extractedFiles;
        } else {
            size_t aligned = ((fileSize + 511ULL) / 512ULL) * 512ULL;
            uint8_t scratch[512];
            while (aligned > 0) {
                if (reader.readBytes(scratch, sizeof(scratch)) != sizeof(scratch)) {
                    break;
                }
                aligned -= 512ULL;
            }
        }
    }

    return (outResult.extractedFiles > 0 || outResult.extractedSymlinks > 0);
}

bool checkValidElf64Arm64(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    uint8_t hdr[20] = {0};
    ssize_t n = read(fd, hdr, sizeof(hdr));
    close(fd);
    if (n < 20) return false;
    if (hdr[0] != 0x7FU || hdr[1] != 'E' || hdr[2] != 'L' || hdr[3] != 'F') return false;
    if (hdr[4] != 2U) return false; // ELFCLASS64
    uint16_t machine = static_cast<uint16_t>(hdr[18]) | (static_cast<uint16_t>(hdr[19]) << 8U);
    return machine == 183U; // EM_AARCH64
}

bool fileExistsNonEmpty(const std::string& path) {
    struct stat st {};
    return (stat(path.c_str(), &st) == 0 && st.st_size > 0);
}

} // namespace

RuntimeExtractionResult TarPosixExtractor::extractArchivePath(
    const std::string& archivePath,
    const std::string& destDir
) {
    RuntimeExtractionResult res;
    uint64_t bytesHashed = 0;
    if (!Sha256Hasher::computePathSha256(archivePath, res.archiveSha256, bytesHashed, res.errorMessage)) {
        return res;
    }

    if (!mkdirRecursive(destDir, 0755)) {
        res.errorMessage = "Cannot create destination runtime directory: " + destDir;
        return res;
    }

    int fd = open(archivePath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        res.errorMessage = std::string("Cannot open archive: ") + std::strerror(errno);
        return res;
    }

    uint8_t magic[6] = {0};
    ssize_t mLen = pread(fd, magic, sizeof(magic), 0);
    if (mLen < 6) {
        close(fd);
        res.errorMessage = "Archive file too small";
        return res;
    }

    bool isXz = (magic[0] == 0xFDU && magic[1] == '7' && magic[2] == 'z' &&
                 magic[3] == 'X' && magic[4] == 'Z' && magic[5] == 0x00U);
    bool isGz = (magic[0] == 0x1FU && magic[1] == 0x8BU);

    if (isXz) {
        XzStreamReader xzReader(fd);
        bool ok = extractTarStream(xzReader, destDir, res);
        if (!ok) {
            if (res.errorMessage.empty()) {
                res.errorMessage = xzReader.lastError().empty()
                    ? "XZ stream decompression failed"
                    : xzReader.lastError();
            }
            res.success = false;
            return res;
        }
        res.success = true;
        return res;
    } else if (isGz) {
        gzFile gz = gzdopen(fd, "rb");
        if (gz == nullptr) {
            close(fd);
            res.errorMessage = "gzdopen failed on gzip archive";
            return res;
        }
        GzipStreamReader gzReader(gz);
        res.success = extractTarStream(gzReader, destDir, res);
        return res;
    } else {
        FdStreamReader rawReader(fd);
        res.success = extractTarStream(rawReader, destDir, res);
        close(fd);
        return res;
    }
}

InstalledRuntimeVerification TarPosixExtractor::verifyInstalledRuntime(
    const std::string& runtimeRootDir
) {
    InstalledRuntimeVerification v;
    std::string wineOptPath = runtimeRootDir + "/opt/hangover-wine";
    struct stat st {};
    if (stat(wineOptPath.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
        wineOptPath = runtimeRootDir;
    }

    v.wineBinaryPath = wineOptPath + "/bin/wine";
    chmod(v.wineBinaryPath.c_str(), 0755);
    v.wineBinaryValidElf64 = checkValidElf64Arm64(v.wineBinaryPath);

    std::string aarch64Win = wineOptPath + "/lib/wine/aarch64-windows";
    std::string i386Win = wineOptPath + "/lib/wine/i386-windows";

    auto checkMod = [&](const std::string& name) -> bool {
        return fileExistsNonEmpty(aarch64Win + "/" + name) ||
               fileExistsNonEmpty(i386Win + "/" + name);
    };

    v.wowBox64Present = fileExistsNonEmpty(aarch64Win + "/wowbox64.dll");
    v.wow64FexPresent = fileExistsNonEmpty(aarch64Win + "/libwow64fex.dll");
    v.d3d9Present = checkMod("d3d9.dll") && checkMod("wined3d.dll");
    v.dsoundPresent = checkMod("dsound.dll") && checkMod("dinput8.dll");
    v.winsockPresent = checkMod("ws2_32.dll") && checkMod("iphlpapi.dll");

    std::string virglBin = runtimeRootDir + "/bin/virgl_test_server_android";
    chmod(virglBin.c_str(), 0755);
    v.maliVirglPresent = fileExistsNonEmpty(virglBin) ||
                         fileExistsNonEmpty(runtimeRootDir + "/lib/libvirglrenderer.so");
    v.maliZinkPresent = fileExistsNonEmpty(runtimeRootDir + "/lib/dri/zink_dri.so") ||
                        fileExistsNonEmpty(runtimeRootDir + "/lib/libGL.so.1") ||
                        fileExistsNonEmpty(runtimeRootDir + "/lib/libGL.so");

    const char* coreMods[] = {
        "ntdll.dll", "kernel32.dll", "kernelbase.dll", "user32.dll",
        "gdi32.dll", "advapi32.dll", "winmm.dll", "dsound.dll",
        "dinput8.dll", "d3d9.dll", "wined3d.dll", "ws2_32.dll",
        "winex11.drv", "wowbox64.dll", "libwow64fex.dll"
    };
    for (const char* m : coreMods) {
        if (checkMod(m)) {
            ++v.verifiedModuleCount;
        }
    }

    v.isVerified = v.wineBinaryValidElf64 &&
                   (v.wowBox64Present || v.wow64FexPresent) &&
                   v.d3d9Present &&
                   v.dsoundPresent &&
                   v.winsockPresent;

    if (!v.isVerified) {
        if (!v.wineBinaryValidElf64) {
            v.errorMessage = "Missing or invalid ARM64 ELF binary: opt/hangover-wine/bin/wine";
        } else if (!v.wowBox64Present && !v.wow64FexPresent) {
            v.errorMessage = "Missing WOW64 PE32 translator DLLs (wowbox64.dll / libwow64fex.dll)";
        } else {
            v.errorMessage = "Incomplete Wine DirectX 9 / DirectSound / Winsock DLL tree";
        }
    }
    return v;
}

} // namespace gtasa
