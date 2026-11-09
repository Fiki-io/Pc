#pragma once

#include <android/native_window.h>
#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace gtasa {

struct GpuHardwareInfo {
    bool querySucceeded = false;
    std::string vendor;
    std::string renderer;
    std::string glVersion;
    std::string shadingLanguageVersion;
    int32_t maxTextureSize = 0;
    bool isMaliFamily = false;
    std::string recommendedBackend;
};

struct SessionLaunchConfig {
    std::string runtimeRootDir;
    std::string gameDirectoryPath;
    std::string executableName;
    std::string cpuTranslatorDll;     // "wowbox64.dll" or "libwow64fex.dll"
    std::string maliGraphicsBackend;  // "virgl-gles32-mali" or "zink-vulkan-mali"
    bool useDxvkD3d9 = false;
    int32_t renderWidth = 1280;
    int32_t renderHeight = 720;
    int32_t box64SafeFlags = 1;
    int32_t box64StrongMem = 1;
    int32_t box64BigBlock = 2;
    bool box64CallRet = true;
    bool box64FastNan = true;
    bool enableEsync = true;
};

struct SessionTelemetrySnapshot {
    bool isRunning = false;
    int32_t winePid = -1;
    int32_t virglPid = -1;
    int32_t exitCode = 0;
    uint64_t rssMemoryBytes = 0;
    float surfaceFps = 0.0f;
    uint64_t renderedFrames = 0;
    uint32_t dispatchedInputEvents = 0;
    std::string activeStatusText;
    std::vector<std::string> recentLogs;
};

class BionicSessionEngine {
public:
    static BionicSessionEngine& instance();

    GpuHardwareInfo probeDeviceGpuHardware();

    bool startSession(const SessionLaunchConfig& config, std::string& outError);
    void stopSession();

    void attachSurface(ANativeWindow* window, int32_t width, int32_t height);
    void detachSurface();

    void sendKeyInput(int32_t win32VkCode, bool pressed);
    void sendMouseDelta(float dx, float dy, uint32_t buttonMask);

    SessionTelemetrySnapshot pollTelemetry();

private:
    BionicSessionEngine();
    ~BionicSessionEngine();

    void renderLoop();
    void logReaderLoop(int pipeFd);
    void appendLog(const std::string& line);
    uint64_t readProcessRssBytes(pid_t pid) const;

    mutable std::mutex mutex_;
    std::atomic<bool> sessionRunning_{false};
    std::atomic<pid_t> winePid_{-1};
    std::atomic<pid_t> virglPid_{-1};
    std::atomic<pid_t> x11Pid_{-1};
    std::atomic<int32_t> lastExitCode_{0};

    int inputPipeWriteFd_ = -1;
    int logPipeReadFd_ = -1;
    std::thread logThread_;

    std::mutex surfaceMutex_;
    ANativeWindow* nativeWindow_ = nullptr;
    int32_t surfaceWidth_ = 0;
    int32_t surfaceHeight_ = 0;
    std::atomic<bool> renderThreadActive_{false};
    std::thread renderThread_;

    std::string fbSharedPath_;
    int32_t fbWidth_ = 1280;
    int32_t fbHeight_ = 720;

    std::atomic<float> currentFps_{0.0f};
    std::atomic<uint64_t> frameCounter_{0};
    std::atomic<uint32_t> inputCounter_{0};
    std::atomic<float> pointerNormX_{0.5f};
    std::atomic<float> pointerNormY_{0.5f};
    std::atomic<uint32_t> activeKeyMask_{0};

    std::deque<std::string> logLines_;
    std::string statusText_;
};

} // namespace gtasa
