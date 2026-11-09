#include <jni.h>
#include <android/native_window_jni.h>
#include <sstream>
#include <string>

#include "bionic_session_engine.h"
#include "pe32_inspector.h"
#include "sha256_stream.h"
#include "tar_posix_extractor.h"

namespace {

std::string jstringToStd(JNIEnv* env, jstring jstr) {
    if (jstr == nullptr) return "";
    const char* utf = env->GetStringUTFChars(jstr, nullptr);
    if (utf == nullptr) return "";
    std::string out(utf);
    env->ReleaseStringUTFChars(jstr, utf);
    return out;
}

std::string escapeField(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (char c : raw) {
        if (c == '|' || c == '\n' || c == '\r') {
            out.push_back(' ');
        } else {
            out.push_back(c);
        }
    }
    return out;
}

} // namespace

extern "C" {

JNIEXPORT jstring JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativeProbeGpuHardware(
    JNIEnv* env,
    jobject /* thiz */
) {
    gtasa::GpuHardwareInfo info = gtasa::BionicSessionEngine::instance().probeDeviceGpuHardware();
    std::ostringstream oss;
    oss << (info.querySucceeded ? "1" : "0") << "|"
        << escapeField(info.vendor) << "|"
        << escapeField(info.renderer) << "|"
        << escapeField(info.glVersion) << "|"
        << escapeField(info.shadingLanguageVersion) << "|"
        << info.maxTextureSize << "|"
        << (info.isMaliFamily ? "1" : "0") << "|"
        << escapeField(info.recommendedBackend);
    return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativeComputeFileSha256(
    JNIEnv* env,
    jobject /* thiz */,
    jstring jpath
) {
    std::string path = jstringToStd(env, jpath);
    std::string hex;
    uint64_t bytesRead = 0;
    std::string err;
    bool ok = gtasa::Sha256Hasher::computePathSha256(path, hex, bytesRead, err);
    std::ostringstream oss;
    oss << (ok ? "1" : "0") << "|"
        << hex << "|"
        << bytesRead << "|"
        << escapeField(err);
    return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativeInspectPe32Binary(
    JNIEnv* env,
    jobject /* thiz */,
    jstring jpath
) {
    std::string path = jstringToStd(env, jpath);
    gtasa::Pe32BinaryReport rep = gtasa::Pe32Inspector::inspectPe32Path(path);
    std::ostringstream dllStream;
    for (size_t i = 0; i < rep.importedDlls.size(); ++i) {
        if (i > 0) dllStream << ",";
        dllStream << escapeField(rep.importedDlls[i]);
    }
    std::ostringstream oss;
    oss << (rep.isValidPe32 ? "1" : "0") << "|"
        << rep.machine << "|"
        << rep.sectionCount << "|"
        << rep.timestamp << "|"
        << rep.characteristics << "|"
        << (rep.isLargeAddressAware ? "1" : "0") << "|"
        << rep.entryPointRva << "|"
        << rep.imageBase << "|"
        << rep.sizeOfImage << "|"
        << rep.subsystem << "|"
        << rep.fileSizeBytes << "|"
        << escapeField(rep.detectedVersionCode) << "|"
        << dllStream.str() << "|"
        << escapeField(rep.errorMessage);
    return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativePatchLargeAddressAware(
    JNIEnv* env,
    jobject /* thiz */,
    jstring jpath,
    jboolean jenable
) {
    std::string path = jstringToStd(env, jpath);
    bool verifiedState = false;
    std::string err;
    bool ok = gtasa::Pe32Inspector::patchLargeAddressAwarePath(
        path,
        jenable == JNI_TRUE,
        verifiedState,
        err
    );
    std::ostringstream oss;
    oss << (ok ? "1" : "0") << "|"
        << (verifiedState ? "1" : "0") << "|"
        << escapeField(err);
    return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativeInspectRenderWareImg(
    JNIEnv* env,
    jobject /* thiz */,
    jstring jpath
) {
    std::string path = jstringToStd(env, jpath);
    gtasa::RenderWareImgReport rep = gtasa::Pe32Inspector::inspectRenderWareImgPath(path);
    std::ostringstream oss;
    oss << (rep.isValidVer2 ? "1" : "0") << "|"
        << rep.totalEntries << "|"
        << rep.dffModelCount << "|"
        << rep.txdTextureCount << "|"
        << rep.colCollisionCount << "|"
        << rep.ifpAnimCount << "|"
        << rep.fileSizeBytes << "|"
        << (rep.sectorBoundsValid ? "1" : "0") << "|"
        << escapeField(rep.errorMessage);
    return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativeExtractRuntimeArchive(
    JNIEnv* env,
    jobject /* thiz */,
    jstring jarchivePath,
    jstring jdestDir
) {
    std::string archivePath = jstringToStd(env, jarchivePath);
    std::string destDir = jstringToStd(env, jdestDir);
    gtasa::RuntimeExtractionResult res = gtasa::TarPosixExtractor::extractArchivePath(archivePath, destDir);
    std::ostringstream oss;
    oss << (res.success ? "1" : "0") << "|"
        << res.extractedFiles << "|"
        << res.extractedSymlinks << "|"
        << res.extractedDirs << "|"
        << res.totalBytesWritten << "|"
        << res.archiveSha256 << "|"
        << escapeField(res.errorMessage);
    return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativeVerifyInstalledRuntime(
    JNIEnv* env,
    jobject /* thiz */,
    jstring jruntimeRootDir
) {
    std::string rootDir = jstringToStd(env, jruntimeRootDir);
    gtasa::InstalledRuntimeVerification v = gtasa::TarPosixExtractor::verifyInstalledRuntime(rootDir);
    std::ostringstream oss;
    oss << (v.isVerified ? "1" : "0") << "|"
        << (v.wineBinaryValidElf64 ? "1" : "0") << "|"
        << (v.wowBox64Present ? "1" : "0") << "|"
        << (v.wow64FexPresent ? "1" : "0") << "|"
        << (v.d3d9Present ? "1" : "0") << "|"
        << (v.dsoundPresent ? "1" : "0") << "|"
        << (v.winsockPresent ? "1" : "0") << "|"
        << (v.maliVirglPresent ? "1" : "0") << "|"
        << (v.maliZinkPresent ? "1" : "0") << "|"
        << v.verifiedModuleCount << "|"
        << escapeField(v.wineBinaryPath) << "|"
        << escapeField(v.errorMessage);
    return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT jstring JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativeStartSession(
    JNIEnv* env,
    jobject /* thiz */,
    jstring jruntimeRootDir,
    jstring jgameDirPath,
    jstring jexeName,
    jstring jcpuTranslatorDll,
    jstring jmaliBackend,
    jboolean juseDxvk,
    jint jrenderWidth,
    jint jrenderHeight,
    jint jbox64SafeFlags,
    jint jbox64StrongMem,
    jint jbox64BigBlock,
    jboolean jbox64CallRet,
    jboolean jbox64FastNan,
    jboolean jenableEsync
) {
    gtasa::SessionLaunchConfig cfg;
    cfg.runtimeRootDir = jstringToStd(env, jruntimeRootDir);
    cfg.gameDirectoryPath = jstringToStd(env, jgameDirPath);
    cfg.executableName = jstringToStd(env, jexeName);
    cfg.cpuTranslatorDll = jstringToStd(env, jcpuTranslatorDll);
    cfg.maliGraphicsBackend = jstringToStd(env, jmaliBackend);
    cfg.useDxvkD3d9 = (juseDxvk == JNI_TRUE);
    cfg.renderWidth = static_cast<int32_t>(jrenderWidth);
    cfg.renderHeight = static_cast<int32_t>(jrenderHeight);
    cfg.box64SafeFlags = static_cast<int32_t>(jbox64SafeFlags);
    cfg.box64StrongMem = static_cast<int32_t>(jbox64StrongMem);
    cfg.box64BigBlock = static_cast<int32_t>(jbox64BigBlock);
    cfg.box64CallRet = (jbox64CallRet == JNI_TRUE);
    cfg.box64FastNan = (jbox64FastNan == JNI_TRUE);
    cfg.enableEsync = (jenableEsync == JNI_TRUE);

    std::string err;
    bool ok = gtasa::BionicSessionEngine::instance().startSession(cfg, err);
    std::ostringstream oss;
    oss << (ok ? "1" : "0") << "|" << escapeField(err);
    return env->NewStringUTF(oss.str().c_str());
}

JNIEXPORT void JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativeStopSession(
    JNIEnv* /* env */,
    jobject /* thiz */
) {
    gtasa::BionicSessionEngine::instance().stopSession();
}

JNIEXPORT void JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativeAttachSurface(
    JNIEnv* env,
    jobject /* thiz */,
    jobject jsurface,
    jint width,
    jint height
) {
    if (jsurface == nullptr) return;
    ANativeWindow* window = ANativeWindow_fromSurface(env, jsurface);
    if (window != nullptr) {
        gtasa::BionicSessionEngine::instance().attachSurface(
            window,
            static_cast<int32_t>(width),
            static_cast<int32_t>(height)
        );
    }
}

JNIEXPORT void JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativeDetachSurface(
    JNIEnv* /* env */,
    jobject /* thiz */
) {
    gtasa::BionicSessionEngine::instance().detachSurface();
}

JNIEXPORT void JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativeSendKeyInput(
    JNIEnv* /* env */,
    jobject /* thiz */,
    jint win32VkCode,
    jboolean pressed
) {
    gtasa::BionicSessionEngine::instance().sendKeyInput(
        static_cast<int32_t>(win32VkCode),
        pressed == JNI_TRUE
    );
}

JNIEXPORT void JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativeSendMouseDelta(
    JNIEnv* /* env */,
    jobject /* thiz */,
    jfloat dx,
    jfloat dy,
    jint buttonMask
) {
    gtasa::BionicSessionEngine::instance().sendMouseDelta(
        static_cast<float>(dx),
        static_cast<float>(dy),
        static_cast<uint32_t>(buttonMask)
    );
}

JNIEXPORT jstring JNICALL
Java_com_example_domain_engine_NativeEngineBridge_nativePollSessionTelemetry(
    JNIEnv* env,
    jobject /* thiz */
) {
    gtasa::SessionTelemetrySnapshot snap = gtasa::BionicSessionEngine::instance().pollTelemetry();
    std::ostringstream logsStream;
    for (size_t i = 0; i < snap.recentLogs.size(); ++i) {
        if (i > 0) logsStream << "\n";
        logsStream << snap.recentLogs[i];
    }
    std::ostringstream oss;
    oss << (snap.isRunning ? "1" : "0") << "|"
        << snap.winePid << "|"
        << snap.virglPid << "|"
        << snap.exitCode << "|"
        << snap.rssMemoryBytes << "|"
        << snap.surfaceFps << "|"
        << snap.renderedFrames << "|"
        << snap.dispatchedInputEvents << "|"
        << escapeField(snap.activeStatusText) << "|"
        << logsStream.str();
    return env->NewStringUTF(oss.str().c_str());
}

} // extern "C"
