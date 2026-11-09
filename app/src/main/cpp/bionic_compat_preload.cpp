#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>
#include <vector>
#include <string>

extern char** environ;

namespace {

constexpr const char* kTermuxUsrPrefix = "/data/data/com.termux/files/usr";
constexpr size_t kTermuxUsrPrefixLen = 31;

constexpr const char* kTermuxHomePrefix = "/data/data/com.termux/files/home";
constexpr size_t kTermuxHomePrefixLen = 32;

constexpr const char* kAndroidLinker64 = "/system/bin/linker64";
constexpr const char* kAndroidSh = "/system/bin/sh";

using OpenFn = int (*)(const char*, int, ...);
using OpenAtFn = int (*)(int, const char*, int, ...);
using Open2Fn = int (*)(const char*, int);
using OpenAt2Fn = int (*)(int, const char*, int);
using FopenFn = FILE* (*)(const char*, const char*);
using StatFn = int (*)(const char*, struct stat*);
using FstatAtFn = int (*)(int, const char*, struct stat*, int);
using Stat64Fn = int (*)(const char*, struct stat64*);
using FstatAt64Fn = int (*)(int, const char*, struct stat64*, int);

using AccessFn = int (*)(const char*, int);
using FaccessAtFn = int (*)(int, const char*, int, int);
using MkdirFn = int (*)(const char*, mode_t);
using MkdirAtFn = int (*)(int, const char*, mode_t);
using RmdirFn = int (*)(const char*);
using UnlinkFn = int (*)(const char*);
using UnlinkAtFn = int (*)(int, const char*, int);
using RenameFn = int (*)(const char*, const char*);
using RenameAtFn = int (*)(int, const char*, int, const char*);
using SymlinkFn = int (*)(const char*, const char*);
using SymlinkAtFn = int (*)(const char*, int, const char*);
using ReadlinkFn = ssize_t (*)(const char*, char*, size_t);
using ReadlinkAtFn = ssize_t (*)(int, const char*, char*, size_t);
using RealpathFn = char* (*)(const char*, char*);
using OpendirFn = DIR* (*)(const char*);
using ChdirFn = int (*)(const char*);
using ChmodFn = int (*)(const char*, mode_t);
using FchmodAtFn = int (*)(int, const char*, mode_t, int);
using BindFn = int (*)(int, const struct sockaddr*, socklen_t);
using ConnectFn = int (*)(int, const struct sockaddr*, socklen_t);
using ExecveFn = int (*)(const char*, char* const[], char* const[]);
using PosixSpawnFn = int (*)(pid_t*, const char*, const posix_spawn_file_actions_t*,
                             const posix_spawnattr_t*, char* const[], char* const[]);
using MmapFn = void* (*)(void*, size_t, int, int, int, off_t);
using Mmap64Fn = void* (*)(void*, size_t, int, int, int, off64_t);
using MprotectFn = int (*)(void*, size_t, int);

template <typename Fn>
Fn resolveNext(const char* symbol) {
    return reinterpret_cast<Fn>(dlsym(RTLD_NEXT, symbol));
}

const char* getRuntimeRoot() {
    const char* root = getenv("GTASA_RUNTIME_ROOT");
    if (root != nullptr && root[0] == '/') {
        return root;
    }
    return nullptr;
}

const char* getHomeDir() {
    const char* home = getenv("HOME");
    if (home != nullptr && home[0] == '/') {
        return home;
    }
    return nullptr;
}

bool endsWith(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t lenStr = strlen(str);
    size_t lenSuf = strlen(suffix);
    if (lenSuf > lenStr) return false;
    return strcmp(str + (lenStr - lenSuf), suffix) == 0;
}

const char* remapPath(const char* path, char* buf, size_t bufSize) {
    if (path == nullptr || buf == nullptr || bufSize == 0) {
        return path;
    }

    const char* runtimeRoot = getRuntimeRoot();
    if (runtimeRoot != nullptr) {
        if (strncmp(path, kTermuxUsrPrefix, kTermuxUsrPrefixLen) == 0 &&
            (path[kTermuxUsrPrefixLen] == '/' || path[kTermuxUsrPrefixLen] == '\0')) {
            int n = snprintf(buf, bufSize, "%s%s", runtimeRoot, path + kTermuxUsrPrefixLen);
            if (n > 0 && static_cast<size_t>(n) < bufSize) {
                return buf;
            }
        }
        if (strcmp(path, "/tmp") == 0) {
            int n = snprintf(buf, bufSize, "%s/tmp", runtimeRoot);
            if (n > 0 && static_cast<size_t>(n) < bufSize) {
                return buf;
            }
        }
        if (strncmp(path, "/tmp/", 5) == 0) {
            int n = snprintf(buf, bufSize, "%s/tmp/%s", runtimeRoot, path + 5);
            if (n > 0 && static_cast<size_t>(n) < bufSize) {
                return buf;
            }
        }
        if (strncmp(path, "/usr/share/", 11) == 0) {
            int n = snprintf(buf, bufSize, "%s/share/%s", runtimeRoot, path + 11);
            if (n > 0 && static_cast<size_t>(n) < bufSize) {
                return buf;
            }
        }
    }

    const char* homeDir = getHomeDir();
    if (homeDir != nullptr) {
        if (strncmp(path, kTermuxHomePrefix, kTermuxHomePrefixLen) == 0 &&
            (path[kTermuxHomePrefixLen] == '/' || path[kTermuxHomePrefixLen] == '\0')) {
            int n = snprintf(buf, bufSize, "%s%s", homeDir, path + kTermuxHomePrefixLen);
            if (n > 0 && static_cast<size_t>(n) < bufSize) {
                return buf;
            }
        }
    }

    if (strcmp(path, "/bin/sh") == 0 || strcmp(path, "/bin/bash") == 0) {
        return kAndroidSh;
    }

    return path;
}

int rawAccess(const char* path, int mode) {
    static AccessFn fn = resolveNext<AccessFn>("access");
    if (!fn) return -1;
    return fn(path, mode);
}

int rawOpen(const char* path, int flags, mode_t mode = 0) {
    static OpenFn fn = resolveNext<OpenFn>("open");
    if (!fn) return -1;
    return fn(path, flags, mode);
}

bool isElfBinary(const char* path) {
    if (!path) return false;
    int fd = rawOpen(path, O_RDONLY | O_CLOEXEC, 0);
    if (fd < 0) return false;
    unsigned char hdr[4] = {0};
    ssize_t n = read(fd, hdr, sizeof(hdr));
    close(fd);
    return (n == 4 && hdr[0] == 0x7F && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F');
}

bool isShebangScript(const char* path) {
    if (!path) return false;
    int fd = rawOpen(path, O_RDONLY | O_CLOEXEC, 0);
    if (fd < 0) return false;
    unsigned char hdr[2] = {0};
    ssize_t n = read(fd, hdr, sizeof(hdr));
    close(fd);
    return (n == 2 && hdr[0] == '#' && hdr[1] == '!');
}

bool resolveInPath(const char* file, char* outBuf, size_t outBufSize) {
    if (!file || !outBuf || outBufSize == 0) return false;
    if (strchr(file, '/') != nullptr) {
        const char* mapped = remapPath(file, outBuf, outBufSize);
        if (mapped != outBuf) {
            snprintf(outBuf, outBufSize, "%s", mapped);
        }
        return true;
    }

    const char* pathEnv = getenv("PATH");
    const char* runtimeRoot = getRuntimeRoot();
    char defaultPath[4096];
    if (!pathEnv || pathEnv[0] == '\0') {
        if (runtimeRoot) {
            snprintf(defaultPath, sizeof(defaultPath),
                     "%s/opt/hangover-wine/bin:%s/bin:/system/bin", runtimeRoot, runtimeRoot);
            pathEnv = defaultPath;
        } else {
            pathEnv = "/system/bin";
        }
    }

    const char* p = pathEnv;
    while (*p != '\0') {
        const char* colon = strchr(p, ':');
        size_t dirLen = colon ? static_cast<size_t>(colon - p) : strlen(p);
        if (dirLen > 0 && dirLen + 1 + strlen(file) < outBufSize) {
            char candidate[4096];
            memcpy(candidate, p, dirLen);
            candidate[dirLen] = '/';
            strcpy(candidate + dirLen + 1, file);
            char mappedCandidate[4096];
            const char* checkPath = remapPath(candidate, mappedCandidate, sizeof(mappedCandidate));
            if (rawAccess(checkPath, R_OK) == 0) {
                snprintf(outBuf, outBufSize, "%s", checkPath);
                return true;
            }
        }
        if (!colon) break;
        p = colon + 1;
    }

    snprintf(outBuf, outBufSize, "%s", file);
    return false;
}

void buildPatchedEnvironment(char* const envp[],
                             std::vector<std::string>& storage,
                             std::vector<char*>& outEnvp) {
    char* const* src = (envp != nullptr) ? envp : environ;
    bool hasNoExec = false;
    bool hasRuntimeRoot = false;
    bool hasLdPreload = false;
    bool hasLdLibPath = false;

    if (src != nullptr) {
        for (size_t i = 0; src[i] != nullptr; ++i) {
            const char* entry = src[i];
            if (strncmp(entry, "WINELOADERNOEXEC=", 17) == 0) {
                storage.emplace_back("WINELOADERNOEXEC=1");
                hasNoExec = true;
            } else {
                if (strncmp(entry, "GTASA_RUNTIME_ROOT=", 19) == 0) hasRuntimeRoot = true;
                if (strncmp(entry, "LD_PRELOAD=", 11) == 0) hasLdPreload = true;
                if (strncmp(entry, "LD_LIBRARY_PATH=", 16) == 0) hasLdLibPath = true;
                storage.emplace_back(entry);
            }
        }
    }

    if (!hasNoExec) {
        storage.emplace_back("WINELOADERNOEXEC=1");
    }
    if (!hasRuntimeRoot) {
        const char* rt = getRuntimeRoot();
        if (rt) {
            storage.emplace_back(std::string("GTASA_RUNTIME_ROOT=") + rt);
        }
    }
    if (!hasLdPreload) {
        const char* preload = getenv("LD_PRELOAD");
        if (preload && preload[0] != '\0') {
            storage.emplace_back(std::string("LD_PRELOAD=") + preload);
        }
    }
    if (!hasLdLibPath) {
        const char* ldPath = getenv("LD_LIBRARY_PATH");
        if (ldPath && ldPath[0] != '\0') {
            storage.emplace_back(std::string("LD_LIBRARY_PATH=") + ldPath);
        }
    }

    outEnvp.reserve(storage.size() + 1);
    for (auto& s : storage) {
        outEnvp.push_back(s.data());
    }
    outEnvp.push_back(nullptr);
}

void prepareExecInvocation(const char* rawPath,
                           char* const rawArgv[],
                           std::string& outExecBinary,
                           std::vector<std::string>& argStorage,
                           std::vector<char*>& outArgv) {
    char mappedBuf[4096];
    const char* resolved = remapPath(rawPath, mappedBuf, sizeof(mappedBuf));
    std::string target = resolved ? resolved : "";

    char* const* effectiveArgv = rawArgv;
    if (endsWith(target.c_str(), "/wine-preloader") &&
        effectiveArgv != nullptr &&
        effectiveArgv[0] != nullptr &&
        effectiveArgv[1] != nullptr) {
        char mappedLoader[4096];
        const char* loaderResolved = remapPath(effectiveArgv[1], mappedLoader, sizeof(mappedLoader));
        target = loaderResolved ? loaderResolved : effectiveArgv[1];
        effectiveArgv = effectiveArgv + 1;
    }

    const char* runtimeRoot = getRuntimeRoot();
    if (runtimeRoot != nullptr &&
        (endsWith(target.c_str(), "/opt/hangover-wine/bin/wine") ||
         endsWith(target.c_str(), "/bin/wine"))) {
        std::string realLoader = std::string(runtimeRoot) + "/opt/hangover-wine/lib/wine/aarch64-unix/wine";
        if (rawAccess(realLoader.c_str(), R_OK) == 0) {
            target = realLoader;
        }
    }

    if ((endsWith(target.c_str(), "/bin/bash") || endsWith(target.c_str(), "/bin/sh")) &&
        rawAccess(target.c_str(), R_OK) != 0) {
        target = kAndroidSh;
    }

    bool wrapWithSh = (strncmp(target.c_str(), "/data/", 6) == 0) &&
                      isShebangScript(target.c_str());

    if (wrapWithSh) {
        outExecBinary = kAndroidSh;
        argStorage.emplace_back(kAndroidSh);
        argStorage.emplace_back(target);
        if (effectiveArgv != nullptr && effectiveArgv[0] != nullptr) {
            for (size_t i = 1; effectiveArgv[i] != nullptr; ++i) {
                argStorage.emplace_back(effectiveArgv[i]);
            }
        }
    } else {
        outExecBinary = target;
        argStorage.emplace_back(target);
        if (effectiveArgv != nullptr && effectiveArgv[0] != nullptr) {
            for (size_t i = 1; effectiveArgv[i] != nullptr; ++i) {
                argStorage.emplace_back(effectiveArgv[i]);
            }
        }
    }

    outArgv.reserve(argStorage.size() + 1);
    for (auto& s : argStorage) {
        outArgv.push_back(s.data());
    }
    outArgv.push_back(nullptr);
}

using SocketOpFn = int (*)(int, const struct sockaddr*, socklen_t);

int dispatchUnixSocket(SocketOpFn fn, int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    if (addr != nullptr && addr->sa_family == AF_UNIX &&
        static_cast<size_t>(addrlen) > sizeof(sa_family_t)) {
        const auto* un = reinterpret_cast<const struct sockaddr_un*>(addr);
        if (un->sun_path[0] == '/') {
            char buf[4096];
            const char* mapped = remapPath(un->sun_path, buf, sizeof(buf));
            if (mapped != un->sun_path) {
                if (strlen(mapped) < sizeof(un->sun_path)) {
                    struct sockaddr_un patched {};
                    patched.sun_family = AF_UNIX;
                    strncpy(patched.sun_path, mapped, sizeof(patched.sun_path) - 1);
                    return fn(sockfd, reinterpret_cast<const struct sockaddr*>(&patched), sizeof(patched));
                } else {
                    const char* lastSlash = strrchr(mapped, '/');
                    if (lastSlash != nullptr && lastSlash != mapped) {
                        std::string parentDir(mapped, lastSlash - mapped);
                        const char* baseName = lastSlash + 1;
                        if (strlen(baseName) < sizeof(un->sun_path)) {
                            int oldCwd = open(".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
                            if (oldCwd >= 0) {
                                if (chdir(parentDir.c_str()) == 0) {
                                    struct sockaddr_un relPatched {};
                                    relPatched.sun_family = AF_UNIX;
                                    strncpy(relPatched.sun_path, baseName, sizeof(relPatched.sun_path) - 1);
                                    int res = fn(sockfd, reinterpret_cast<const struct sockaddr*>(&relPatched), sizeof(relPatched));
                                    int saveErr = errno;
                                    fchdir(oldCwd);
                                    close(oldCwd);
                                    errno = saveErr;
                                    return res;
                                }
                                close(oldCwd);
                            }
                        }
                    }
                }
            }
        }
    }
    return fn(sockfd, addr, addrlen);
}

} // namespace

extern "C" {

#undef open64
#undef openat64
#undef fopen64
#undef stat64
#undef lstat64
#undef fstatat64
#undef mmap64

__attribute__((visibility("default")))
int execve(const char* pathname, char* const argv[], char* const envp[]) {
    static ExecveFn realExecve = resolveNext<ExecveFn>("execve");
    if (!realExecve) {
        errno = ENOSYS;
        return -1;
    }

    std::string execBin;
    std::vector<std::string> argStorage;
    std::vector<char*> patchedArgv;
    prepareExecInvocation(pathname, argv, execBin, argStorage, patchedArgv);

    std::vector<std::string> envStorage;
    std::vector<char*> patchedEnvp;
    buildPatchedEnvironment(envp, envStorage, patchedEnvp);

    int rc = realExecve(execBin.c_str(), patchedArgv.data(), patchedEnvp.data());
    int savedErr = errno;
    if (rc == -1 && (savedErr == EACCES || savedErr == EPERM) &&
        strncmp(execBin.c_str(), "/data/", 6) == 0 &&
        isElfBinary(execBin.c_str())) {
        std::vector<char*> linkerArgv;
        linkerArgv.reserve(patchedArgv.size() + 1);
        linkerArgv.push_back(const_cast<char*>(kAndroidLinker64));
        for (char* a : patchedArgv) {
            linkerArgv.push_back(a);
        }
        realExecve(kAndroidLinker64, linkerArgv.data(), patchedEnvp.data());
        errno = savedErr;
    }
    return rc;
}

__attribute__((visibility("default")))
int execv(const char* pathname, char* const argv[]) {
    return execve(pathname, argv, environ);
}

__attribute__((visibility("default")))
int execvpe(const char* file, char* const argv[], char* const envp[]) {
    char resolved[4096];
    resolveInPath(file, resolved, sizeof(resolved));
    return execve(resolved, argv, envp);
}

__attribute__((visibility("default")))
int execvp(const char* file, char* const argv[]) {
    return execvpe(file, argv, environ);
}

__attribute__((visibility("default")))
int posix_spawn(pid_t* pid,
                const char* path,
                const posix_spawn_file_actions_t* file_actions,
                const posix_spawnattr_t* attrp,
                char* const argv[],
                char* const envp[]) {
    static PosixSpawnFn realPosixSpawn = resolveNext<PosixSpawnFn>("posix_spawn");
    if (!realPosixSpawn) {
        errno = ENOSYS;
        return ENOSYS;
    }

    std::string execBin;
    std::vector<std::string> argStorage;
    std::vector<char*> patchedArgv;
    prepareExecInvocation(path, argv, execBin, argStorage, patchedArgv);

    std::vector<std::string> envStorage;
    std::vector<char*> patchedEnvp;
    buildPatchedEnvironment(envp, envStorage, patchedEnvp);

    int rc = realPosixSpawn(pid, execBin.c_str(), file_actions, attrp,
                            patchedArgv.data(), patchedEnvp.data());
    if ((rc == EACCES || rc == EPERM) &&
        strncmp(execBin.c_str(), "/data/", 6) == 0 &&
        isElfBinary(execBin.c_str())) {
        std::vector<char*> linkerArgv;
        linkerArgv.reserve(patchedArgv.size() + 1);
        linkerArgv.push_back(const_cast<char*>(kAndroidLinker64));
        for (char* a : patchedArgv) {
            linkerArgv.push_back(a);
        }
        int linkerRc = realPosixSpawn(pid, kAndroidLinker64, file_actions, attrp,
                                      linkerArgv.data(), patchedEnvp.data());
        if (linkerRc == 0) {
            return 0;
        }
    }
    return rc;
}

__attribute__((visibility("default")))
int posix_spawnp(pid_t* pid,
                 const char* file,
                 const posix_spawn_file_actions_t* file_actions,
                 const posix_spawnattr_t* attrp,
                 char* const argv[],
                 char* const envp[]) {
    char resolved[4096];
    resolveInPath(file, resolved, sizeof(resolved));
    return posix_spawn(pid, resolved, file_actions, attrp, argv, envp);
}

__attribute__((visibility("default")))
int open(const char* pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = static_cast<mode_t>(va_arg(ap, int));
        va_end(ap);
    }
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return rawOpen(mapped, flags, mode);
}

__attribute__((visibility("default")))
int open64(const char* pathname, int flags, ...) {
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = static_cast<mode_t>(va_arg(ap, int));
        va_end(ap);
    }
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return rawOpen(mapped, flags, mode);
}

__attribute__((visibility("default")))
int __open_2(const char* pathname, int flags) {
    static Open2Fn fn = resolveNext<Open2Fn>("__open_2");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    if (fn) return fn(mapped, flags);
    return rawOpen(mapped, flags, 0);
}

__attribute__((visibility("default")))
int openat(int dirfd, const char* pathname, int flags, ...) {
    static OpenAtFn fn = resolveNext<OpenAtFn>("openat");
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = static_cast<mode_t>(va_arg(ap, int));
        va_end(ap);
    }
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(dirfd, mapped, flags, mode);
}

__attribute__((visibility("default")))
int openat64(int dirfd, const char* pathname, int flags, ...) {
    static OpenAtFn fn = resolveNext<OpenAtFn>("openat");
    mode_t mode = 0;
    if (flags & (O_CREAT | O_TMPFILE)) {
        va_list ap;
        va_start(ap, flags);
        mode = static_cast<mode_t>(va_arg(ap, int));
        va_end(ap);
    }
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(dirfd, mapped, flags, mode);
}

__attribute__((visibility("default")))
int __openat_2(int dirfd, const char* pathname, int flags) {
    static OpenAt2Fn fn = resolveNext<OpenAt2Fn>("__openat_2");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    if (fn) return fn(dirfd, mapped, flags);
    static OpenAtFn fallback = resolveNext<OpenAtFn>("openat");
    return fallback(dirfd, mapped, flags, 0);
}

__attribute__((visibility("default")))
FILE* fopen(const char* pathname, const char* mode) {
    static FopenFn fn = resolveNext<FopenFn>("fopen");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(mapped, mode);
}

__attribute__((visibility("default")))
FILE* fopen64(const char* pathname, const char* mode) {
    static FopenFn fn = resolveNext<FopenFn>("fopen");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(mapped, mode);
}

__attribute__((visibility("default")))
int stat(const char* pathname, struct stat* statbuf) {
    static StatFn fn = resolveNext<StatFn>("stat");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(mapped, statbuf);
}

__attribute__((visibility("default")))
int stat64(const char* pathname, struct stat64* statbuf) {
    static Stat64Fn fn = resolveNext<Stat64Fn>("stat64");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    if (fn) return fn(mapped, statbuf);
    static StatFn fallback = resolveNext<StatFn>("stat");
    return fallback(mapped, reinterpret_cast<struct stat*>(statbuf));
}

__attribute__((visibility("default")))
int lstat(const char* pathname, struct stat* statbuf) {
    static StatFn fn = resolveNext<StatFn>("lstat");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(mapped, statbuf);
}

__attribute__((visibility("default")))
int lstat64(const char* pathname, struct stat64* statbuf) {
    static Stat64Fn fn = resolveNext<Stat64Fn>("lstat64");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    if (fn) return fn(mapped, statbuf);
    static StatFn fallback = resolveNext<StatFn>("lstat");
    return fallback(mapped, reinterpret_cast<struct stat*>(statbuf));
}

__attribute__((visibility("default")))
int fstatat(int dirfd, const char* pathname, struct stat* statbuf, int flags) {
    static FstatAtFn fn = resolveNext<FstatAtFn>("fstatat");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(dirfd, mapped, statbuf, flags);
}

__attribute__((visibility("default")))
int fstatat64(int dirfd, const char* pathname, struct stat64* statbuf, int flags) {
    static FstatAt64Fn fn = resolveNext<FstatAt64Fn>("fstatat64");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    if (fn) return fn(dirfd, mapped, statbuf, flags);
    static FstatAtFn fallback = resolveNext<FstatAtFn>("fstatat");
    return fallback(dirfd, mapped, reinterpret_cast<struct stat*>(statbuf), flags);
}

__attribute__((visibility("default")))
int access(const char* pathname, int mode) {
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return rawAccess(mapped, mode);
}

__attribute__((visibility("default")))
int faccessat(int dirfd, const char* pathname, int mode, int flags) {
    static FaccessAtFn fn = resolveNext<FaccessAtFn>("faccessat");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(dirfd, mapped, mode, flags);
}

__attribute__((visibility("default")))
int mkdir(const char* pathname, mode_t mode) {
    static MkdirFn fn = resolveNext<MkdirFn>("mkdir");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(mapped, mode);
}

__attribute__((visibility("default")))
int mkdirat(int dirfd, const char* pathname, mode_t mode) {
    static MkdirAtFn fn = resolveNext<MkdirAtFn>("mkdirat");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(dirfd, mapped, mode);
}

__attribute__((visibility("default")))
int rmdir(const char* pathname) {
    static RmdirFn fn = resolveNext<RmdirFn>("rmdir");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(mapped);
}

__attribute__((visibility("default")))
int unlink(const char* pathname) {
    static UnlinkFn fn = resolveNext<UnlinkFn>("unlink");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(mapped);
}

__attribute__((visibility("default")))
int unlinkat(int dirfd, const char* pathname, int flags) {
    static UnlinkAtFn fn = resolveNext<UnlinkAtFn>("unlinkat");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(dirfd, mapped, flags);
}

__attribute__((visibility("default")))
int rename(const char* oldpath, const char* newpath) {
    static RenameFn fn = resolveNext<RenameFn>("rename");
    char buf1[4096];
    char buf2[4096];
    const char* mapped1 = remapPath(oldpath, buf1, sizeof(buf1));
    const char* mapped2 = remapPath(newpath, buf2, sizeof(buf2));
    return fn(mapped1, mapped2);
}

__attribute__((visibility("default")))
int renameat(int olddirfd, const char* oldpath, int newdirfd, const char* newpath) {
    static RenameAtFn fn = resolveNext<RenameAtFn>("renameat");
    char buf1[4096];
    char buf2[4096];
    const char* mapped1 = remapPath(oldpath, buf1, sizeof(buf1));
    const char* mapped2 = remapPath(newpath, buf2, sizeof(buf2));
    return fn(olddirfd, mapped1, newdirfd, mapped2);
}

__attribute__((visibility("default")))
int symlink(const char* target, const char* linkpath) {
    static SymlinkFn fn = resolveNext<SymlinkFn>("symlink");
    char buf1[4096];
    char buf2[4096];
    const char* mapped1 = remapPath(target, buf1, sizeof(buf1));
    const char* mapped2 = remapPath(linkpath, buf2, sizeof(buf2));
    return fn(mapped1, mapped2);
}

__attribute__((visibility("default")))
int symlinkat(const char* target, int newdirfd, const char* linkpath) {
    static SymlinkAtFn fn = resolveNext<SymlinkAtFn>("symlinkat");
    char buf1[4096];
    char buf2[4096];
    const char* mapped1 = remapPath(target, buf1, sizeof(buf1));
    const char* mapped2 = remapPath(linkpath, buf2, sizeof(buf2));
    return fn(mapped1, newdirfd, mapped2);
}

__attribute__((visibility("default")))
ssize_t readlink(const char* pathname, char* buf, size_t bufsiz) {
    static ReadlinkFn fn = resolveNext<ReadlinkFn>("readlink");
    char pathBuf[4096];
    const char* mapped = remapPath(pathname, pathBuf, sizeof(pathBuf));
    return fn(mapped, buf, bufsiz);
}

__attribute__((visibility("default")))
ssize_t readlinkat(int dirfd, const char* pathname, char* buf, size_t bufsiz) {
    static ReadlinkAtFn fn = resolveNext<ReadlinkAtFn>("readlinkat");
    char pathBuf[4096];
    const char* mapped = remapPath(pathname, pathBuf, sizeof(pathBuf));
    return fn(dirfd, mapped, buf, bufsiz);
}

__attribute__((visibility("default")))
char* realpath(const char* path, char* resolved_path) {
    static RealpathFn fn = resolveNext<RealpathFn>("realpath");
    char buf[4096];
    const char* mapped = remapPath(path, buf, sizeof(buf));
    return fn(mapped, resolved_path);
}

__attribute__((visibility("default")))
DIR* opendir(const char* name) {
    static OpendirFn fn = resolveNext<OpendirFn>("opendir");
    char buf[4096];
    const char* mapped = remapPath(name, buf, sizeof(buf));
    return fn(mapped);
}

__attribute__((visibility("default")))
int chdir(const char* path) {
    static ChdirFn fn = resolveNext<ChdirFn>("chdir");
    char buf[4096];
    const char* mapped = remapPath(path, buf, sizeof(buf));
    return fn(mapped);
}

__attribute__((visibility("default")))
int chmod(const char* pathname, mode_t mode) {
    static ChmodFn fn = resolveNext<ChmodFn>("chmod");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(mapped, mode);
}

__attribute__((visibility("default")))
int fchmodat(int dirfd, const char* pathname, mode_t mode, int flags) {
    static FchmodAtFn fn = resolveNext<FchmodAtFn>("fchmodat");
    char buf[4096];
    const char* mapped = remapPath(pathname, buf, sizeof(buf));
    return fn(dirfd, mapped, mode, flags);
}

__attribute__((visibility("default")))
int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    static BindFn fn = resolveNext<BindFn>("bind");
    return dispatchUnixSocket(fn, sockfd, addr, addrlen);
}

__attribute__((visibility("default")))
int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen) {
    static ConnectFn fn = resolveNext<ConnectFn>("connect");
    return dispatchUnixSocket(fn, sockfd, addr, addrlen);
}

__attribute__((visibility("default")))
void* mmap64(void* addr, size_t length, int prot, int flags, int fd, off64_t offset) {
    static Mmap64Fn realMmap64 = resolveNext<Mmap64Fn>("mmap64");
    static MmapFn realMmap = resolveNext<MmapFn>("mmap");
    static MprotectFn realMprotect = resolveNext<MprotectFn>("mprotect");

    void* res = realMmap64
        ? realMmap64(addr, length, prot, flags, fd, offset)
        : realMmap(addr, length, prot, flags, fd, static_cast<off_t>(offset));
    if (res != MAP_FAILED) {
        return res;
    }

    int savedErr = errno;
    if ((savedErr == EACCES || savedErr == EPERM) &&
        (prot & PROT_EXEC) != 0 &&
        fd >= 0 &&
        length > 0) {
        int anonFlags = (flags & ~MAP_SHARED) | MAP_PRIVATE | MAP_ANONYMOUS;
        void* anon = realMmap64
            ? realMmap64(addr, length, PROT_READ | PROT_WRITE, anonFlags, -1, 0)
            : realMmap(addr, length, PROT_READ | PROT_WRITE, anonFlags, -1, 0);
        if (anon != MAP_FAILED) {
            size_t totalRead = 0;
            while (totalRead < length) {
                ssize_t n = pread64(fd,
                                    static_cast<uint8_t*>(anon) + totalRead,
                                    length - totalRead,
                                    offset + static_cast<off64_t>(totalRead));
                if (n < 0) {
                    if (errno == EINTR) continue;
                    break;
                }
                if (n == 0) break;
                totalRead += static_cast<size_t>(n);
            }
            __builtin___clear_cache(static_cast<char*>(anon),
                                    static_cast<char*>(anon) + length);
            if (realMprotect(anon, length, prot) == 0) {
                return anon;
            }
            munmap(anon, length);
        }
    }
    errno = savedErr;
    return MAP_FAILED;
}

__attribute__((visibility("default")))
void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return mmap64(addr, length, prot, flags, fd, static_cast<off64_t>(offset));
}

__attribute__((visibility("default")))
int mprotect(void* addr, size_t len, int prot) {
    static MprotectFn realMprotect = resolveNext<MprotectFn>("mprotect");
    static MmapFn realMmap = resolveNext<MmapFn>("mmap");

    int rc = realMprotect(addr, len, prot);
    if (rc == 0) {
        return 0;
    }

    int savedErr = errno;
    if ((savedErr == EACCES || savedErr == EPERM) &&
        (prot & PROT_EXEC) != 0 &&
        addr != nullptr &&
        len > 0) {
        long sysPage = sysconf(_SC_PAGESIZE);
        size_t pageSize = (sysPage > 0) ? static_cast<size_t>(sysPage) : 4096U;
        uintptr_t startAddr = reinterpret_cast<uintptr_t>(addr) & ~(pageSize - 1U);
        uintptr_t endAddr = (reinterpret_cast<uintptr_t>(addr) + len + pageSize - 1U) & ~(pageSize - 1U);
        size_t alignedLen = endAddr - startAddr;
        void* pagePtr = reinterpret_cast<void*>(startAddr);

        if (realMprotect(pagePtr, alignedLen, PROT_READ) == 0) {
            std::vector<uint8_t> snapshot(alignedLen);
            memcpy(snapshot.data(), pagePtr, alignedLen);

            void* anon = realMmap(pagePtr,
                                  alignedLen,
                                  PROT_READ | PROT_WRITE,
                                  MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                                  -1,
                                  0);
            if (anon != MAP_FAILED) {
                memcpy(pagePtr, snapshot.data(), alignedLen);
                __builtin___clear_cache(reinterpret_cast<char*>(startAddr),
                                        reinterpret_cast<char*>(endAddr));
                if (realMprotect(pagePtr, alignedLen, prot) == 0) {
                    return 0;
                }
            }
        }
    }

    errno = savedErr;
    return -1;
}

} // extern "C"
