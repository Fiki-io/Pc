#include "bionic_session_engine.h"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <zlib.h>

namespace gtasa {

namespace {

uint16_t readLe16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8U);
}

uint32_t readLe32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8U) |
           (static_cast<uint32_t>(p[2]) << 16U) |
           (static_cast<uint32_t>(p[3]) << 24U);
}

bool ensureDirectory(const std::string& path) {
    if (path.empty()) return false;
    char tmp[4096];
    std::snprintf(tmp, sizeof(tmp), "%s", path.c_str());
    for (char* p = tmp + 1; *p != '\0'; ++p) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    return (mkdir(tmp, 0755) == 0 || errno == EEXIST);
}

bool extractZipEntryToFile(const std::string& zipPath,
                           const std::string& entryName,
                           const std::string& destPath) {
    int fd = open(zipPath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;

    struct stat st {};
    if (fstat(fd, &st) != 0 || st.st_size < 22) {
        close(fd);
        return false;
    }

    size_t fileSize = static_cast<size_t>(st.st_size);
    size_t tailSize = std::min<size_t>(fileSize, 65557U);
    std::vector<uint8_t> tail(tailSize);
    if (pread(fd, tail.data(), tailSize, static_cast<off_t>(fileSize - tailSize)) !=
        static_cast<ssize_t>(tailSize)) {
        close(fd);
        return false;
    }

    ssize_t eocdPos = -1;
    for (ssize_t i = static_cast<ssize_t>(tailSize) - 22; i >= 0; --i) {
        if (tail[i] == 0x50 && tail[i + 1] == 0x4B &&
            tail[i + 2] == 0x05 && tail[i + 3] == 0x06) {
            eocdPos = i;
            break;
        }
    }
    if (eocdPos < 0) {
        close(fd);
        return false;
    }

    uint32_t cdSize = readLe32(tail.data() + eocdPos + 12);
    uint32_t cdOffset = readLe32(tail.data() + eocdPos + 16);
    if (static_cast<size_t>(cdOffset) + static_cast<size_t>(cdSize) > fileSize) {
        close(fd);
        return false;
    }

    std::vector<uint8_t> cd(cdSize);
    if (pread(fd, cd.data(), cdSize, static_cast<off_t>(cdOffset)) !=
        static_cast<ssize_t>(cdSize)) {
        close(fd);
        return false;
    }

    size_t pos = 0;
    while (pos + 46 <= cdSize) {
        const uint8_t* p = cd.data() + pos;
        if (readLe32(p) != 0x02014B50U) break;
        uint16_t method = readLe16(p + 10);
        uint32_t compSize = readLe32(p + 20);
        uint32_t uncompSize = readLe32(p + 24);
        uint16_t nameLen = readLe16(p + 28);
        uint16_t extraLen = readLe16(p + 30);
        uint16_t commentLen = readLe16(p + 32);
        uint32_t lfhOffset = readLe32(p + 42);

        if (pos + 46 + nameLen + extraLen + commentLen > cdSize) break;
        std::string currentName(reinterpret_cast<const char*>(p + 46), nameLen);
        if (currentName == entryName) {
            uint8_t lfh[30];
            if (pread(fd, lfh, sizeof(lfh), static_cast<off_t>(lfhOffset)) != sizeof(lfh) ||
                readLe32(lfh) != 0x04034B50U) {
                close(fd);
                return false;
            }
            uint16_t lfhNameLen = readLe16(lfh + 26);
            uint16_t lfhExtraLen = readLe16(lfh + 28);
            size_t dataOffset = static_cast<size_t>(lfhOffset) + 30U + lfhNameLen + lfhExtraLen;
            if (dataOffset + compSize > fileSize) {
                close(fd);
                return false;
            }

            std::vector<uint8_t> compBuf(compSize);
            if (pread(fd, compBuf.data(), compSize, static_cast<off_t>(dataOffset)) !=
                static_cast<ssize_t>(compSize)) {
                close(fd);
                return false;
            }
            close(fd);

            std::vector<uint8_t> outBuf;
            if (method == 0) {
                outBuf = std::move(compBuf);
            } else if (method == 8) {
                outBuf.resize(uncompSize);
                z_stream zs {};
                if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
                    return false;
                }
                zs.next_in = compBuf.data();
                zs.avail_in = compSize;
                zs.next_out = outBuf.data();
                zs.avail_out = uncompSize;
                int zrc = inflate(&zs, Z_FINISH);
                inflateEnd(&zs);
                if (zrc != Z_STREAM_END) {
                    return false;
                }
            } else {
                return false;
            }

            if (outBuf.size() < 4 || outBuf[0] != 0x7F || outBuf[1] != 'E' ||
                outBuf[2] != 'L' || outBuf[3] != 'F') {
                return false;
            }

            int outFd = open(destPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0755);
            if (outFd < 0) return false;
            size_t written = 0;
            while (written < outBuf.size()) {
                ssize_t w = write(outFd, outBuf.data() + written, outBuf.size() - written);
                if (w <= 0) {
                    close(outFd);
                    return false;
                }
                written += static_cast<size_t>(w);
            }
            close(outFd);
            chmod(destPath.c_str(), 0755);
            return true;
        }
        pos += 46U + nameLen + extraLen + commentLen;
    }

    close(fd);
    return false;
}

std::string resolveBionicCompatLibraryPath(const std::string& runtimeRootDir) {
    Dl_info info {};
    if (dladdr(reinterpret_cast<const void*>(&BionicSessionEngine::instance), &info) != 0 &&
        info.dli_fname != nullptr) {
        std::string selfPath(info.dli_fname);
        size_t bangPos = selfPath.find("!/");
        if (bangPos == std::string::npos) {
            size_t slash = selfPath.find_last_of('/');
            if (slash != std::string::npos) {
                std::string candidate = selfPath.substr(0, slash + 1) + "libbionic_compat.so";
                if (access(candidate.c_str(), R_OK) == 0) {
                    return candidate;
                }
            }
        } else {
            std::string apkPath = selfPath.substr(0, bangPos);
            std::string innerPath = selfPath.substr(bangPos + 2);
            size_t slash = innerPath.find_last_of('/');
            std::string entryName = (slash != std::string::npos)
                ? (innerPath.substr(0, slash + 1) + "libbionic_compat.so")
                : "lib/arm64-v8a/libbionic_compat.so";
            std::string destDir = runtimeRootDir + "/lib";
            ensureDirectory(destDir);
            std::string destPath = destDir + "/libbionic_compat.so";
            if (extractZipEntryToFile(apkPath, entryName, destPath)) {
                return destPath;
            }
        }
    }
    std::string fallbackPath = runtimeRootDir + "/lib/libbionic_compat.so";
    if (access(fallbackPath.c_str(), R_OK) == 0) {
        return fallbackPath;
    }
    return "";
}


GLuint compileShader(GLenum type, const char* src) {
    GLuint sh = glCreateShader(type);
    glShaderSource(sh, 1, &src, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glDeleteShader(sh);
        return 0;
    }
    return sh;
}

GLuint createQuadProgram() {
    static const char* VS = R"(#version 300 es
        layout(location = 0) in vec2 aPos;
        layout(location = 1) in vec2 aUv;
        out vec2 vUv;
        void main() {
            vUv = aUv;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";
    static const char* FS = R"(#version 300 es
        precision mediump float;
        in vec2 vUv;
        uniform sampler2D uTex;
        uniform vec2 uCursor;
        uniform float uHasProcess;
        out vec4 fragColor;
        void main() {
            vec4 base = texture(uTex, vUv);
            float dist = length((vUv - uCursor) * vec2(16.0, 9.0));
            if (dist < 0.08 && dist > 0.045) {
                base.rgb = mix(base.rgb, vec3(0.04, 0.47, 0.82), 0.85);
            } else if (dist < 0.018) {
                base.rgb = vec3(0.95, 0.95, 0.95);
            }
            if (uHasProcess < 0.5) {
                base.rgb *= 0.6;
            }
            fragColor = vec4(base.rgb, 1.0);
        }
    )";

    GLuint vs = compileShader(GL_VERTEX_SHADER, VS);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, FS);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(prog);
        return 0;
    }
    return prog;
}

} // namespace

BionicSessionEngine& BionicSessionEngine::instance() {
    static BionicSessionEngine inst;
    return inst;
}

BionicSessionEngine::BionicSessionEngine()
    : statusText_("IDLE") {}

BionicSessionEngine::~BionicSessionEngine() {
    detachSurface();
    stopSession();
}

GpuHardwareInfo BionicSessionEngine::probeDeviceGpuHardware() {
    GpuHardwareInfo info;
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY) {
        return info;
    }
    EGLint major = 0, minor = 0;
    if (!eglInitialize(dpy, &major, &minor)) {
        return info;
    }

    const EGLint configAttribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg = nullptr;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(dpy, configAttribs, &cfg, 1, &numConfigs) || numConfigs < 1) {
        eglTerminate(dpy);
        return info;
    }

    const EGLint pbufAttribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pbufAttribs);
    const EGLint ctxAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttribs);
    if (ctx != EGL_NO_CONTEXT && surf != EGL_NO_SURFACE && eglMakeCurrent(dpy, surf, surf, ctx)) {
        const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
        const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
        const char* slVersion = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
        GLint maxTex = 0;
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTex);

        info.vendor = vendor ? vendor : "Unknown";
        info.renderer = renderer ? renderer : "Unknown";
        info.glVersion = version ? version : "Unknown";
        info.shadingLanguageVersion = slVersion ? slVersion : "Unknown";
        info.maxTextureSize = static_cast<int32_t>(maxTex);
        info.querySucceeded = true;

        std::string rUpper = info.renderer + " " + info.vendor;
        for (char& c : rUpper) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        info.isMaliFamily = (rUpper.find("MALI") != std::string::npos ||
                             rUpper.find("IMMORTALIS") != std::string::npos ||
                             rUpper.find("ARM") != std::string::npos);

        if (rUpper.find("IMMORTALIS") != std::string::npos ||
            rUpper.find("G710") != std::string::npos ||
            rUpper.find("G715") != std::string::npos ||
            rUpper.find("G720") != std::string::npos ||
            rUpper.find("G610") != std::string::npos) {
            info.recommendedBackend = "zink-vulkan-mali";
        } else {
            info.recommendedBackend = "virgl-gles32-mali";
        }
        eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }

    if (ctx != EGL_NO_CONTEXT) eglDestroyContext(dpy, ctx);
    if (surf != EGL_NO_SURFACE) eglDestroySurface(dpy, surf);
    eglTerminate(dpy);
    return info;
}

void BionicSessionEngine::appendLog(const std::string& line) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (logLines_.size() >= 250) {
        logLines_.pop_front();
    }
    logLines_.push_back(line);
}

void BionicSessionEngine::logReaderLoop(int pipeFd) {
    char buf[1024];
    std::string partial;
    while (true) {
        ssize_t n = read(pipeFd, buf, sizeof(buf) - 1);
        if (n < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (n == 0) break;
        buf[n] = '\0';
        partial.append(buf, static_cast<size_t>(n));
        size_t pos = 0;
        while ((pos = partial.find('\n')) != std::string::npos) {
            std::string line = partial.substr(0, pos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                appendLog(line);
            }
            partial.erase(0, pos + 1);
        }
    }
    if (!partial.empty()) {
        appendLog(partial);
    }
    close(pipeFd);
}

bool BionicSessionEngine::startSession(const SessionLaunchConfig& config, std::string& outError) {
    stopSession();

    std::string exeFullPath = config.gameDirectoryPath + "/" + config.executableName;
    struct stat stExe {};
    if (stat(exeFullPath.c_str(), &stExe) != 0 || !S_ISREG(stExe.st_mode)) {
        outError = "Executable not found at: " + exeFullPath;
        return false;
    }

    std::string wineOptRoot = config.runtimeRootDir + "/opt/hangover-wine";
    struct stat stWine {};
    if (stat(wineOptRoot.c_str(), &stWine) != 0 || !S_ISDIR(stWine.st_mode)) {
        wineOptRoot = config.runtimeRootDir;
    }
    std::string wineBin = wineOptRoot + "/lib/wine/aarch64-unix/wine";
    if (stat(wineBin.c_str(), &stWine) != 0) {
        wineBin = wineOptRoot + "/bin/wine";
    }
    if (stat(wineBin.c_str(), &stWine) != 0) {
        outError = "Bionic Wine binary not found at: " + wineBin;
        return false;
    }
    std::string wineServerBin = wineOptRoot + "/bin/wineserver";
    chmod(wineBin.c_str(), 0755);
    chmod((wineOptRoot + "/bin/wine").c_str(), 0755);
    chmod(wineServerBin.c_str(), 0755);

    std::string prefixDir = config.runtimeRootDir + "/wineprefix";
    std::string tmpDir = config.runtimeRootDir + "/tmp";
    std::string x11SocketDir = tmpDir + "/.X11-unix";
    ensureDirectory(prefixDir + "/dosdevices");
    ensureDirectory(prefixDir + "/drive_c/windows/system32");
    ensureDirectory(x11SocketDir);

    std::string driveCLink = prefixDir + "/dosdevices/c:";
    std::string driveDLink = prefixDir + "/dosdevices/d:";
    unlink(driveCLink.c_str());
    unlink(driveDLink.c_str());
    symlink("../drive_c", driveCLink.c_str());
    symlink(config.gameDirectoryPath.c_str(), driveDLink.c_str());

    std::string virglLibDir = config.runtimeRootDir + "/opt/virglrenderer-android/lib";
    if (access((virglLibDir + "/libvirglrenderer.so").c_str(), R_OK) == 0) {
        symlink("../opt/virglrenderer-android/lib/libvirglrenderer.so",
                (config.runtimeRootDir + "/lib/libvirglrenderer.so").c_str());
        symlink("../opt/virglrenderer-android/lib/libvirglrenderer.so.1",
                (config.runtimeRootDir + "/lib/libvirglrenderer.so.1").c_str());
    }
    if (access((virglLibDir + "/libepoxy.so").c_str(), R_OK) == 0) {
        symlink("../opt/virglrenderer-android/lib/libepoxy.so",
                (config.runtimeRootDir + "/lib/libepoxy.so").c_str());
        symlink("../opt/virglrenderer-android/lib/libepoxy.so.0",
                (config.runtimeRootDir + "/lib/libepoxy.so.0").c_str());
    }

    fbWidth_ = std::clamp(config.renderWidth, 640, 2560);
    fbHeight_ = std::clamp(config.renderHeight, 360, 1440);
    fbSharedPath_ = tmpDir + "/gtasa_fb0.raw";

    int fbFd = open(fbSharedPath_.c_str(), O_RDWR | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fbFd >= 0) {
        size_t fbBytes = static_cast<size_t>(fbWidth_) * static_cast<size_t>(fbHeight_) * 4U;
        if (ftruncate(fbFd, static_cast<off_t>(fbBytes)) == 0) {
            void* mapped = mmap(nullptr, fbBytes, PROT_READ | PROT_WRITE, MAP_SHARED, fbFd, 0);
            if (mapped != MAP_FAILED) {
                uint32_t* px = static_cast<uint32_t*>(mapped);
                for (int32_t y = 0; y < fbHeight_; ++y) {
                    uint8_t shade = static_cast<uint8_t>(18 + (y * 16) / std::max(1, fbHeight_));
                    uint32_t rgba = 0xFF000000U |
                                    (static_cast<uint32_t>(shade + 4) << 16U) |
                                    (static_cast<uint32_t>(shade + 2) << 8U) |
                                    static_cast<uint32_t>(shade);
                    for (int32_t x = 0; x < fbWidth_; ++x) {
                        px[y * fbWidth_ + x] = rgba;
                    }
                }
                munmap(mapped, fbBytes);
            }
        }
        close(fbFd);
    }

    std::string xvfbBin = config.runtimeRootDir + "/bin/Xvfb";
    std::string termuxX11Bin = config.runtimeRootDir + "/bin/termux-x11";
    bool hasXServer = (access(xvfbBin.c_str(), R_OK) == 0) || (access(termuxX11Bin.c_str(), R_OK) == 0);

    std::string userRegPath = prefixDir + "/user.reg";
    FILE* regFp = std::fopen(userRegPath.c_str(), "w");
    if (regFp != nullptr) {
        std::fprintf(
            regFp,
            "WINE REGISTRY Version 2\n\n"
            "[Software\\\\Wine\\\\Direct3D] 1700000000\n"
            "\"DirectDrawRenderer\"=\"opengl\"\n"
            "\"csmt\"=dword:00000003\n"
            "\"VideoMemorySize\"=\"512\"\n"
            "\"OffscreenRenderingMode\"=\"fbo\"\n\n"
            "[Software\\\\Wine\\\\Drivers] 1700000000\n"
            "\"Graphics\"=\"%s\"\n\n"
            "[Software\\\\Wine\\\\X11 Driver] 1700000000\n"
            "\"MouseWarpOverride\"=\"enable\"\n"
            "\"Decorated\"=\"N\"\n"
            "\"Managed\"=\"N\"\n"
            "\"UseXVidMode\"=\"N\"\n"
            "\"UseXRandR\"=\"N\"\n\n"
            "[Software\\\\Wine\\\\DllOverrides] 1700000000\n"
            "\"d3d9\"=\"%s\"\n",
            hasXServer ? "x11" : "null",
            config.useDxvkD3d9 ? "native,builtin" : "builtin"
        );
        std::fclose(regFp);
    }

    int logPipe[2];
    int inPipe[2];
    if (pipe(logPipe) != 0 || pipe(inPipe) != 0) {
        outError = std::string("pipe() failed: ") + std::strerror(errno);
        return false;
    }

    std::string virglBin = config.runtimeRootDir + "/bin/virgl_test_server_android";
    chmod(virglBin.c_str(), 0755);
    std::string virglSocket = tmpDir + "/.virgl_test";
    std::string ldLibPath = config.runtimeRootDir + "/lib:" +
                            virglLibDir + ":" +
                            config.runtimeRootDir + "/opt/angle-android/gl:" +
                            config.runtimeRootDir + "/opt/angle-android/vulkan:" +
                            wineOptRoot + "/lib:" +
                            wineOptRoot + "/lib/wine/aarch64-unix:" +
                            config.runtimeRootDir + "/lib/dri:" +
                            config.runtimeRootDir + "/lib/gallium-pipe:/system/lib64";
    std::string runtimePathEnv = wineOptRoot + "/bin:" + config.runtimeRootDir + "/bin:/system/bin";

    std::string compatLib = resolveBionicCompatLibraryPath(config.runtimeRootDir);
    std::string shmemLib = config.runtimeRootDir + "/lib/libandroid-shmem.so";
    std::string spawnLib = config.runtimeRootDir + "/lib/libandroid-spawn.so";
    std::string ldPreload;
    auto appendPreload = [&ldPreload](const std::string& libPath) {
        if (!libPath.empty() && access(libPath.c_str(), R_OK) == 0) {
            if (!ldPreload.empty()) ldPreload += ":";
            ldPreload += libPath;
        }
    };
    appendPreload(compatLib);
    appendPreload(shmemLib);
    appendPreload(spawnLib);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        logLines_.clear();
        statusText_ = "RUNNING";
    }

    if (config.maliGraphicsBackend == "virgl-gles32-mali" && access(virglBin.c_str(), R_OK) == 0) {
        unlink(virglSocket.c_str());
        pid_t vpid = fork();
        if (vpid == 0) {
            close(logPipe[0]);
            dup2(logPipe[1], STDOUT_FILENO);
            dup2(logPipe[1], STDERR_FILENO);
            close(logPipe[1]);
            close(inPipe[0]);
            close(inPipe[1]);

            setenv("GTASA_RUNTIME_ROOT", config.runtimeRootDir.c_str(), 1);
            setenv("HOME", prefixDir.c_str(), 1);
            setenv("TMPDIR", tmpDir.c_str(), 1);
            setenv("PATH", runtimePathEnv.c_str(), 1);
            setenv("LD_LIBRARY_PATH", ldLibPath.c_str(), 1);
            if (!ldPreload.empty()) {
                setenv("LD_PRELOAD", ldPreload.c_str(), 1);
            }
            setenv("VTEST_SOCKET_NAME", virglSocket.c_str(), 1);
            setenv("VTEST_RENDERER_SOCKET_NAME", virglSocket.c_str(), 1);
            setenv("VTEST_USE_EGL_SURFACELESS", "1", 1);
            setenv("VTEST_USE_GLES", "1", 1);
            execl(
                virglBin.c_str(),
                virglBin.c_str(),
                "--socket-path",
                virglSocket.c_str(),
                static_cast<char*>(nullptr)
            );
            int directErr = errno;
            execl(
                "/system/bin/linker64",
                "/system/bin/linker64",
                virglBin.c_str(),
                "--socket-path",
                virglSocket.c_str(),
                static_cast<char*>(nullptr)
            );
            std::fprintf(stderr, "[VIRGL-EXEC-ERR] direct=%s linker=%s\n",
                         std::strerror(directErr), std::strerror(errno));
            std::fflush(stderr);
            _exit(127);
        } else if (vpid > 0) {
            virglPid_.store(vpid);
            appendLog("[MALI-VIRGL] Started virgl_test_server_android PID=" + std::to_string(vpid));
        }
    }

    if (access(xvfbBin.c_str(), R_OK) == 0) {
        chmod(xvfbBin.c_str(), 0755);
        pid_t xpid = fork();
        if (xpid == 0) {
            close(logPipe[0]);
            dup2(logPipe[1], STDOUT_FILENO);
            dup2(logPipe[1], STDERR_FILENO);
            close(logPipe[1]);
            close(inPipe[0]);
            close(inPipe[1]);

            setenv("GTASA_RUNTIME_ROOT", config.runtimeRootDir.c_str(), 1);
            setenv("HOME", prefixDir.c_str(), 1);
            setenv("TMPDIR", tmpDir.c_str(), 1);
            setenv("PATH", runtimePathEnv.c_str(), 1);
            setenv("LD_LIBRARY_PATH", ldLibPath.c_str(), 1);
            if (!ldPreload.empty()) {
                setenv("LD_PRELOAD", ldPreload.c_str(), 1);
            }

            std::string screenArg = std::to_string(fbWidth_) + "x" + std::to_string(fbHeight_) + "x24";
            execl(
                xvfbBin.c_str(),
                xvfbBin.c_str(),
                ":0",
                "-screen", "0", screenArg.c_str(),
                "-fbdir", tmpDir.c_str(),
                "-nolisten", "tcp",
                "-ac",
                static_cast<char*>(nullptr)
            );
            int directErr = errno;
            execl(
                "/system/bin/linker64",
                "/system/bin/linker64",
                xvfbBin.c_str(),
                ":0",
                "-screen", "0", screenArg.c_str(),
                "-fbdir", tmpDir.c_str(),
                "-nolisten", "tcp",
                "-ac",
                static_cast<char*>(nullptr)
            );
            std::fprintf(stderr, "[XVFB-EXEC-ERR] direct=%s linker=%s\n",
                         std::strerror(directErr), std::strerror(errno));
            std::fflush(stderr);
            _exit(127);
        } else if (xpid > 0) {
            x11Pid_.store(xpid);
            appendLog("[X11] Started Xvfb Virtual Framebuffer Server PID=" + std::to_string(xpid) +
                      " (:0 @ " + std::to_string(fbWidth_) + "x" + std::to_string(fbHeight_) + ")");
            for (int i = 0; i < 20; ++i) {
                struct stat stSock {};
                if (stat((x11SocketDir + "/X0").c_str(), &stSock) == 0 ||
                    stat((tmpDir + "/.X11-unix/X0").c_str(), &stSock) == 0) {
                    break;
                }
                usleep(15000);
            }
            std::string xvfbScreen = tmpDir + "/Xvfb_screen0";
            if (access(xvfbScreen.c_str(), R_OK) == 0) {
                fbSharedPath_ = xvfbScreen;
            }
        }
    } else if (access(termuxX11Bin.c_str(), R_OK) == 0) {
        chmod(termuxX11Bin.c_str(), 0755);
        pid_t xpid = fork();
        if (xpid == 0) {
            close(logPipe[0]);
            dup2(logPipe[1], STDOUT_FILENO);
            dup2(logPipe[1], STDERR_FILENO);
            close(logPipe[1]);
            close(inPipe[0]);
            close(inPipe[1]);

            setenv("GTASA_RUNTIME_ROOT", config.runtimeRootDir.c_str(), 1);
            setenv("HOME", prefixDir.c_str(), 1);
            setenv("TMPDIR", tmpDir.c_str(), 1);
            setenv("PATH", runtimePathEnv.c_str(), 1);
            setenv("LD_LIBRARY_PATH", ldLibPath.c_str(), 1);
            if (!ldPreload.empty()) {
                setenv("LD_PRELOAD", ldPreload.c_str(), 1);
            }

            execl(
                termuxX11Bin.c_str(),
                termuxX11Bin.c_str(),
                ":0",
                "-ac",
                static_cast<char*>(nullptr)
            );
            _exit(127);
        } else if (xpid > 0) {
            x11Pid_.store(xpid);
            appendLog("[X11] Started termux-x11 Display Server PID=" + std::to_string(xpid));
            for (int i = 0; i < 20; ++i) {
                struct stat stSock {};
                if (stat((x11SocketDir + "/X0").c_str(), &stSock) == 0 ||
                    stat((tmpDir + "/.X11-unix/X0").c_str(), &stSock) == 0) {
                    break;
                }
                usleep(15000);
            }
        }
    } else {
        appendLog("[X11] No external X-server detected; running Wine with headless null graphics driver");
    }

    std::string desktopArg = "/desktop=GTASA," + std::to_string(fbWidth_) + "x" + std::to_string(fbHeight_);
    std::string dosExePath = "D:\\" + config.executableName;

    pid_t wpid = fork();
    if (wpid < 0) {
        close(logPipe[0]);
        close(logPipe[1]);
        close(inPipe[0]);
        close(inPipe[1]);
        outError = std::string("fork() failed: ") + std::strerror(errno);
        return false;
    }

    if (wpid == 0) {
        close(logPipe[0]);
        dup2(logPipe[1], STDOUT_FILENO);
        dup2(logPipe[1], STDERR_FILENO);
        close(logPipe[1]);

        close(inPipe[1]);
        dup2(inPipe[0], STDIN_FILENO);
        close(inPipe[0]);

        setenv("GTASA_RUNTIME_ROOT", config.runtimeRootDir.c_str(), 1);
        setenv("HOME", prefixDir.c_str(), 1);
        setenv("TMPDIR", tmpDir.c_str(), 1);
        setenv("PATH", runtimePathEnv.c_str(), 1);
        setenv("WINEPREFIX", prefixDir.c_str(), 1);
        setenv("WINEDLLPATH", (wineOptRoot + "/lib/wine").c_str(), 1);
        setenv("WINELOADER", wineBin.c_str(), 1);
        setenv("WINESERVER", wineServerBin.c_str(), 1);
        setenv("WINELOADERNOEXEC", "1", 1);
        setenv("LD_LIBRARY_PATH", ldLibPath.c_str(), 1);
        if (!ldPreload.empty()) {
            setenv("LD_PRELOAD", ldPreload.c_str(), 1);
        }

        setenv("DISPLAY", ":0", 1);
        setenv("HODLL", config.cpuTranslatorDll.c_str(), 1);
        setenv("WINEDEBUG", "err+all,fixme-all", 1);
        setenv("WINEESYNC", config.enableEsync ? "1" : "0", 1);
        setenv("WINEESYNC_TERMUX", config.enableEsync ? "1" : "0", 1);

        setenv("BOX64_DYNAREC", "1", 1);
        setenv("BOX64_DYNAREC_SAFEFLAGS", std::to_string(config.box64SafeFlags).c_str(), 1);
        setenv("BOX64_DYNAREC_STRONGMEM", std::to_string(config.box64StrongMem).c_str(), 1);
        setenv("BOX64_DYNAREC_BIGBLOCK", std::to_string(config.box64BigBlock).c_str(), 1);
        setenv("BOX64_DYNAREC_CALLRET", config.box64CallRet ? "1" : "0", 1);
        setenv("BOX64_DYNAREC_FASTNAN", config.box64FastNan ? "1" : "0", 1);
        setenv("BOX64_DYNAREC_FASTROUND", "1", 1);
        setenv("BOX64_AVX", "0", 1);

        if (config.maliGraphicsBackend == "zink-vulkan-mali") {
            setenv("GALLIUM_DRIVER", "zink", 1);
            setenv("MESA_LOADER_DRIVER_OVERRIDE", "zink", 1);
        } else {
            setenv("GALLIUM_DRIVER", "virpipe", 1);
            setenv("VTEST_SOCKET_NAME", virglSocket.c_str(), 1);
            setenv("VTEST_RENDERER_SOCKET_NAME", virglSocket.c_str(), 1);
        }
        setenv("MESA_GL_VERSION_OVERRIDE", "3.3COMPAT", 1);
        setenv("MESA_GLES_VERSION_OVERRIDE", "3.2", 1);
        setenv("MESA_NO_ERROR", "1", 1);

        if (chdir(config.gameDirectoryPath.c_str()) != 0) {
            std::fprintf(stderr, "[WINE-CHDIR-ERR] path=%s err=%s\n",
                         config.gameDirectoryPath.c_str(), std::strerror(errno));
            std::fflush(stderr);
            _exit(126);
        }

        execl(
            wineBin.c_str(),
            wineBin.c_str(),
            "explorer",
            desktopArg.c_str(),
            dosExePath.c_str(),
            static_cast<char*>(nullptr)
        );
        int directErr = errno;
        execl(
            "/system/bin/linker64",
            "/system/bin/linker64",
            wineBin.c_str(),
            "explorer",
            desktopArg.c_str(),
            dosExePath.c_str(),
            static_cast<char*>(nullptr)
        );
        std::fprintf(stderr, "[WINE-EXEC-ERR] bin=%s direct=%s linker=%s\n",
                     wineBin.c_str(), std::strerror(directErr), std::strerror(errno));
        std::fflush(stderr);
        _exit(127);
    }

    close(logPipe[1]);
    close(inPipe[0]);
    logPipeReadFd_ = logPipe[0];
    inputPipeWriteFd_ = inPipe[1];
    fcntl(inputPipeWriteFd_, F_SETFL, O_NONBLOCK);

    winePid_.store(wpid);
    lastExitCode_.store(0);
    inputCounter_.store(0);
    sessionRunning_.store(true);

    appendLog("[ENGINE] Spawned Bionic Hangover Wine PID=" + std::to_string(wpid) +
              " (" + config.cpuTranslatorDll + " | " + config.maliGraphicsBackend + ")");
    if (!compatLib.empty()) {
        appendLog("[ENGINE] Bionic Preload Hook active: " + compatLib);
    }

    logThread_ = std::thread(&BionicSessionEngine::logReaderLoop, this, logPipeReadFd_);
    return true;
}

void BionicSessionEngine::stopSession() {
    sessionRunning_.store(false);

    if (inputPipeWriteFd_ >= 0) {
        close(inputPipeWriteFd_);
        inputPipeWriteFd_ = -1;
    }

    pid_t wpid = winePid_.exchange(-1);
    if (wpid > 0) {
        kill(wpid, SIGTERM);
        int status = 0;
        for (int i = 0; i < 10; ++i) {
            if (waitpid(wpid, &status, WNOHANG) == wpid) {
                wpid = -1;
                break;
            }
            usleep(20000);
        }
        if (wpid > 0) {
            kill(wpid, SIGKILL);
            waitpid(wpid, &status, 0);
        }
    }

    pid_t vpid = virglPid_.exchange(-1);
    if (vpid > 0) {
        kill(vpid, SIGKILL);
        int status = 0;
        waitpid(vpid, &status, 0);
    }

    pid_t xpid = x11Pid_.exchange(-1);
    if (xpid > 0) {
        kill(xpid, SIGTERM);
        int status = 0;
        for (int i = 0; i < 5; ++i) {
            if (waitpid(xpid, &status, WNOHANG) == xpid) {
                xpid = -1;
                break;
            }
            usleep(20000);
        }
        if (xpid > 0) {
            kill(xpid, SIGKILL);
            waitpid(xpid, &status, 0);
        }
    }

    if (logThread_.joinable()) {
        logThread_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    statusText_ = "STOPPED";
}

void BionicSessionEngine::attachSurface(ANativeWindow* window, int32_t width, int32_t height) {
    detachSurface();
    if (window == nullptr) return;

    {
        std::lock_guard<std::mutex> lock(surfaceMutex_);
        nativeWindow_ = window;
        surfaceWidth_ = width;
        surfaceHeight_ = height;
        renderThreadActive_.store(true);
    }
    renderThread_ = std::thread(&BionicSessionEngine::renderLoop, this);
}

void BionicSessionEngine::detachSurface() {
    renderThreadActive_.store(false);
    if (renderThread_.joinable()) {
        renderThread_.join();
    }
    std::lock_guard<std::mutex> lock(surfaceMutex_);
    if (nativeWindow_ != nullptr) {
        ANativeWindow_release(nativeWindow_);
        nativeWindow_ = nullptr;
    }
}

void BionicSessionEngine::renderLoop() {
    ANativeWindow* win = nullptr;
    {
        std::lock_guard<std::mutex> lock(surfaceMutex_);
        win = nativeWindow_;
    }
    if (win == nullptr) return;

    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, nullptr, nullptr)) {
        return;
    }

    const EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig cfg = nullptr;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(dpy, attribs, &cfg, 1, &numConfigs) || numConfigs < 1) {
        eglTerminate(dpy);
        return;
    }

    EGLSurface surf = eglCreateWindowSurface(dpy, cfg, win, nullptr);
    const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxAttribs);
    if (surf == EGL_NO_SURFACE || ctx == EGL_NO_CONTEXT || !eglMakeCurrent(dpy, surf, surf, ctx)) {
        if (ctx != EGL_NO_CONTEXT) eglDestroyContext(dpy, ctx);
        if (surf != EGL_NO_SURFACE) eglDestroySurface(dpy, surf);
        eglTerminate(dpy);
        return;
    }

    GLuint prog = createQuadProgram();
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    int32_t texW = std::max(64, fbWidth_);
    int32_t texH = std::max(64, fbHeight_);
    std::vector<uint32_t> localFb(static_cast<size_t>(texW) * static_cast<size_t>(texH), 0xFF14181EU);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texW, texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, localFb.data());

    const float quadVerts[] = {
        -1.0f, -1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 0.0f
    };

    GLuint vao = 0, vbo = 0;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVerts), quadVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));

    GLint uTexLoc = glGetUniformLocation(prog, "uTex");
    GLint uCursorLoc = glGetUniformLocation(prog, "uCursor");
    GLint uHasProcLoc = glGetUniformLocation(prog, "uHasProcess");

    int fbFd = -1;
    void* fbMap = MAP_FAILED;
    size_t fbMapSize = static_cast<size_t>(texW) * static_cast<size_t>(texH) * 4U;
    if (!fbSharedPath_.empty()) {
        fbFd = open(fbSharedPath_.c_str(), O_RDONLY | O_CLOEXEC);
        if (fbFd >= 0) {
            fbMap = mmap(nullptr, fbMapSize, PROT_READ, MAP_SHARED, fbFd, 0);
        }
    }

    auto lastFpsTick = std::chrono::steady_clock::now();
    uint32_t framesInWindow = 0;

    while (renderThreadActive_.load()) {
        EGLint w = 0, h = 0;
        eglQuerySurface(dpy, surf, EGL_WIDTH, &w);
        eglQuerySurface(dpy, surf, EGL_HEIGHT, &h);
        glViewport(0, 0, std::max(1, w), std::max(1, h));
        glClearColor(0.07f, 0.07f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (fbMap != MAP_FAILED) {
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texW, texH, GL_RGBA, GL_UNSIGNED_BYTE, fbMap);
        }

        if (prog != 0) {
            glUseProgram(prog);
            glUniform1i(uTexLoc, 0);
            glUniform2f(uCursorLoc, pointerNormX_.load(), pointerNormY_.load());
            glUniform1f(uHasProcLoc, sessionRunning_.load() ? 1.0f : 0.0f);
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        eglSwapBuffers(dpy, surf);
        ++framesInWindow;
        frameCounter_.fetch_add(1);

        auto now = std::chrono::steady_clock::now();
        float elapsedSec = std::chrono::duration<float>(now - lastFpsTick).count();
        if (elapsedSec >= 0.5f) {
            currentFps_.store(static_cast<float>(framesInWindow) / elapsedSec);
            framesInWindow = 0;
            lastFpsTick = now;
        }
    }

    if (fbMap != MAP_FAILED) munmap(fbMap, fbMapSize);
    if (fbFd >= 0) close(fbFd);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &tex);
    if (prog != 0) glDeleteProgram(prog);

    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(dpy, ctx);
    eglDestroySurface(dpy, surf);
    eglTerminate(dpy);
}

void BionicSessionEngine::sendKeyInput(int32_t win32VkCode, bool pressed) {
    inputCounter_.fetch_add(1);
    if (pressed) {
        activeKeyMask_.fetch_or(1U << (win32VkCode & 31));
    } else {
        activeKeyMask_.fetch_and(~(1U << (win32VkCode & 31)));
    }
    if (inputPipeWriteFd_ >= 0) {
        char packet[32];
        int len = std::snprintf(packet, sizeof(packet), "KEY %d %d\n", win32VkCode, pressed ? 1 : 0);
        if (len > 0) {
            ssize_t ignored = write(inputPipeWriteFd_, packet, static_cast<size_t>(len));
            (void)ignored;
        }
    }
}

void BionicSessionEngine::sendMouseDelta(float dx, float dy, uint32_t buttonMask) {
    inputCounter_.fetch_add(1);
    float nx = std::clamp(pointerNormX_.load() + dx * 0.0012f, 0.0f, 1.0f);
    float ny = std::clamp(pointerNormY_.load() + dy * 0.0012f, 0.0f, 1.0f);
    pointerNormX_.store(nx);
    pointerNormY_.store(ny);
    if (inputPipeWriteFd_ >= 0) {
        char packet[48];
        int len = std::snprintf(packet, sizeof(packet), "MOUSE %.2f %.2f %u\n", dx, dy, buttonMask);
        if (len > 0) {
            ssize_t ignored = write(inputPipeWriteFd_, packet, static_cast<size_t>(len));
            (void)ignored;
        }
    }
}

uint64_t BionicSessionEngine::readProcessRssBytes(pid_t pid) const {
    if (pid <= 0) return 0;
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/statm", static_cast<int>(pid));
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return 0;
    char buf[128] = {0};
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;
    unsigned long totalPages = 0, rssPages = 0;
    if (std::sscanf(buf, "%lu %lu", &totalPages, &rssPages) == 2) {
        long pageSize = sysconf(_SC_PAGESIZE);
        if (pageSize <= 0) pageSize = 4096;
        return static_cast<uint64_t>(rssPages) * static_cast<uint64_t>(pageSize);
    }
    return 0;
}

SessionTelemetrySnapshot BionicSessionEngine::pollTelemetry() {
    SessionTelemetrySnapshot snap;
    pid_t wpid = winePid_.load();
    if (wpid > 0) {
        int status = 0;
        pid_t res = waitpid(wpid, &status, WNOHANG);
        if (res == wpid) {
            winePid_.store(-1);
            sessionRunning_.store(false);
            if (WIFEXITED(status)) {
                lastExitCode_.store(WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                lastExitCode_.store(128 + WTERMSIG(status));
            }
            std::lock_guard<std::mutex> lock(mutex_);
            statusText_ = "EXITED (" + std::to_string(lastExitCode_.load()) + ")";
        }
    }

    snap.isRunning = sessionRunning_.load();
    snap.winePid = static_cast<int32_t>(winePid_.load());
    snap.virglPid = static_cast<int32_t>(virglPid_.load());
    snap.exitCode = lastExitCode_.load();
    snap.rssMemoryBytes = readProcessRssBytes(snap.winePid);
    snap.surfaceFps = currentFps_.load();
    snap.renderedFrames = frameCounter_.load();
    snap.dispatchedInputEvents = inputCounter_.load();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        snap.activeStatusText = statusText_;
        snap.recentLogs.assign(logLines_.begin(), logLines_.end());
    }
    return snap;
}

} // namespace gtasa
