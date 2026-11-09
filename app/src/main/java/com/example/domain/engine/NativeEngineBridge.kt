package com.example.domain.engine

import android.view.Surface
import com.example.data.ExecutionProfileConfig
import com.example.data.GpuProbeData
import com.example.data.LiveSessionState
import com.example.data.Pe32ExecutableState
import com.example.data.RenderWareImgState
import com.example.data.RunnerConstants
import com.example.data.RuntimeVerificationState

data class ArchiveExtractionOutcome(
    val success: Boolean,
    val extractedFiles: Int,
    val extractedSymlinks: Int,
    val extractedDirs: Int,
    val totalBytesWritten: Long,
    val archiveSha256: String,
    val errorMessage: String
)

data class LaaPatchOutcome(
    val success: Boolean,
    val verifiedEnabled: Boolean,
    val errorMessage: String
)

class NativeEngineBridge {

    init {
        ensureLoaded()
    }

    fun probeGpuHardware(): GpuProbeData {
        if (!isLibraryLoaded) {
            return GpuProbeData(querySucceeded = false)
        }
        val raw = nativeProbeGpuHardware()
        val parts = raw.split("|", limit = 8)
        if (parts.size < 8) return GpuProbeData(querySucceeded = false)
        return GpuProbeData(
            querySucceeded = parts[0] == "1",
            vendor = parts[1],
            renderer = parts[2],
            glVersion = parts[3],
            shadingLanguageVersion = parts[4],
            maxTextureSize = parts[5].toIntOrNull() ?: 0,
            isMaliFamily = parts[6] == "1",
            recommendedBackend = parts[7]
        )
    }

    fun inspectPe32Binary(path: String): Pe32ExecutableState {
        if (!isLibraryLoaded) {
            return Pe32ExecutableState(
                isSelected = true,
                isValidPe32 = false,
                localPath = path,
                errorMessage = "Native engine library not loaded"
            )
        }
        val raw = nativeInspectPe32Binary(path)
        val parts = raw.split("|", limit = 14)
        if (parts.size < 14) {
            return Pe32ExecutableState(
                isSelected = true,
                isValidPe32 = false,
                localPath = path,
                errorMessage = "Malformed native PE32 inspection response"
            )
        }
        val dlls = if (parts[12].isBlank()) {
            emptyList()
        } else {
            parts[12].split(",").filter { it.isNotBlank() }
        }
        return Pe32ExecutableState(
            isSelected = true,
            isValidPe32 = parts[0] == "1",
            machine = parts[1].toIntOrNull() ?: 0,
            sectionCount = parts[2].toIntOrNull() ?: 0,
            timestamp = parts[3].toLongOrNull() ?: 0L,
            characteristics = parts[4].toIntOrNull() ?: 0,
            isLargeAddressAware = parts[5] == "1",
            entryPointRva = parts[6].toLongOrNull() ?: 0L,
            imageBase = parts[7].toLongOrNull() ?: 0L,
            sizeOfImage = parts[8].toLongOrNull() ?: 0L,
            subsystem = parts[9].toIntOrNull() ?: 0,
            fileSizeBytes = parts[10].toLongOrNull() ?: 0L,
            detectedVersionCode = parts[11],
            importedDlls = dlls,
            localPath = path,
            errorMessage = parts[13]
        )
    }

    fun patchLargeAddressAware(path: String, enable: Boolean): LaaPatchOutcome {
        if (!isLibraryLoaded) {
            return LaaPatchOutcome(false, false, "Native engine library not loaded")
        }
        val raw = nativePatchLargeAddressAware(path, enable)
        val parts = raw.split("|", limit = 3)
        if (parts.size < 3) {
            return LaaPatchOutcome(false, false, "Malformed LAA patch response")
        }
        return LaaPatchOutcome(
            success = parts[0] == "1",
            verifiedEnabled = parts[1] == "1",
            errorMessage = parts[2]
        )
    }

    fun inspectRenderWareImg(path: String): RenderWareImgState {
        if (!isLibraryLoaded) {
            return RenderWareImgState(
                isSelected = true,
                isValidVer2 = false,
                errorMessage = "Native engine library not loaded"
            )
        }
        val raw = nativeInspectRenderWareImg(path)
        val parts = raw.split("|", limit = 9)
        if (parts.size < 9) {
            return RenderWareImgState(
                isSelected = true,
                isValidVer2 = false,
                errorMessage = "Malformed IMG inspection response"
            )
        }
        return RenderWareImgState(
            isSelected = true,
            isValidVer2 = parts[0] == "1",
            totalEntries = parts[1].toIntOrNull() ?: 0,
            dffModelCount = parts[2].toIntOrNull() ?: 0,
            txdTextureCount = parts[3].toIntOrNull() ?: 0,
            colCollisionCount = parts[4].toIntOrNull() ?: 0,
            ifpAnimCount = parts[5].toIntOrNull() ?: 0,
            fileSizeBytes = parts[6].toLongOrNull() ?: 0L,
            sectorBoundsValid = parts[7] == "1",
            errorMessage = parts[8]
        )
    }

    fun extractRuntimeArchive(archivePath: String, destDir: String): ArchiveExtractionOutcome {
        if (!isLibraryLoaded) {
            return ArchiveExtractionOutcome(
                false, 0, 0, 0, 0L, "", "Native engine library not loaded"
            )
        }
        val raw = nativeExtractRuntimeArchive(archivePath, destDir)
        val parts = raw.split("|", limit = 7)
        if (parts.size < 7) {
            return ArchiveExtractionOutcome(
                false, 0, 0, 0, 0L, "", "Malformed extraction response"
            )
        }
        return ArchiveExtractionOutcome(
            success = parts[0] == "1",
            extractedFiles = parts[1].toIntOrNull() ?: 0,
            extractedSymlinks = parts[2].toIntOrNull() ?: 0,
            extractedDirs = parts[3].toIntOrNull() ?: 0,
            totalBytesWritten = parts[4].toLongOrNull() ?: 0L,
            archiveSha256 = parts[5],
            errorMessage = parts[6]
        )
    }

    fun verifyInstalledRuntime(runtimeRootDir: String, lastSha256: String): RuntimeVerificationState {
        if (!isLibraryLoaded) {
            return RuntimeVerificationState(
                isVerified = false,
                lastArchiveSha256 = lastSha256,
                errorMessage = "Native engine library not loaded"
            )
        }
        val raw = nativeVerifyInstalledRuntime(runtimeRootDir)
        val parts = raw.split("|", limit = 12)
        if (parts.size < 12) {
            return RuntimeVerificationState(
                isVerified = false,
                lastArchiveSha256 = lastSha256,
                errorMessage = "Malformed runtime verification response"
            )
        }
        return RuntimeVerificationState(
            isVerified = parts[0] == "1",
            wineBinaryValidElf64 = parts[1] == "1",
            wowBox64Present = parts[2] == "1",
            wow64FexPresent = parts[3] == "1",
            d3d9Present = parts[4] == "1",
            dsoundPresent = parts[5] == "1",
            winsockPresent = parts[6] == "1",
            maliVirglPresent = parts[7] == "1",
            maliZinkPresent = parts[8] == "1",
            verifiedModuleCount = parts[9].toIntOrNull() ?: 0,
            wineBinaryPath = parts[10],
            lastArchiveSha256 = lastSha256,
            errorMessage = parts[11]
        )
    }

    fun startSession(
        runtimeRootDir: String,
        gameDirPath: String,
        exeName: String,
        profile: ExecutionProfileConfig
    ): Result<Unit> {
        if (!isLibraryLoaded) {
            return Result.failure(IllegalStateException("Native engine library not loaded"))
        }
        val raw = nativeStartSession(
            runtimeRootDir = runtimeRootDir,
            gameDirPath = gameDirPath,
            exeName = exeName,
            cpuTranslatorDll = profile.cpuTranslator.dllName,
            maliBackend = profile.maliBackend.backendId,
            useDxvk = profile.useDxvkD3d9,
            renderWidth = profile.resolution.width,
            renderHeight = profile.resolution.height,
            box64SafeFlags = if (profile.box64SafeFlags) 1 else 0,
            box64StrongMem = if (profile.box64StrongMem) 1 else 0,
            box64BigBlock = 2,
            box64CallRet = profile.box64CallRet,
            box64FastNan = true,
            enableEsync = profile.enableEsync
        )
        val parts = raw.split("|", limit = 2)
        return if (parts.firstOrNull() == "1") {
            Result.success(Unit)
        } else {
            Result.failure(IllegalStateException(parts.getOrNull(1).orEmpty()))
        }
    }

    fun stopSession() {
        if (isLibraryLoaded) {
            nativeStopSession()
        }
    }

    fun attachSurface(surface: Surface, width: Int, height: Int) {
        if (isLibraryLoaded) {
            nativeAttachSurface(surface, width, height)
        }
    }

    fun detachSurface() {
        if (isLibraryLoaded) {
            nativeDetachSurface()
        }
    }

    fun sendKeyInput(win32VkCode: Int, pressed: Boolean) {
        if (isLibraryLoaded) {
            nativeSendKeyInput(win32VkCode, pressed)
        }
    }

    fun sendMouseDelta(dx: Float, dy: Float, buttonMask: Int) {
        if (isLibraryLoaded) {
            nativeSendMouseDelta(dx, dy, buttonMask)
        }
    }

    fun pollSessionTelemetry(): LiveSessionState {
        if (!isLibraryLoaded) return LiveSessionState()
        val raw = nativePollSessionTelemetry()
        val parts = raw.split("|", limit = 10)
        if (parts.size < 10) return LiveSessionState()
        val logs = if (parts[9].isBlank()) {
            emptyList()
        } else {
            parts[9].lines().filter { it.isNotBlank() }
        }
        return LiveSessionState(
            isRunning = parts[0] == "1",
            winePid = parts[1].toIntOrNull() ?: -1,
            virglPid = parts[2].toIntOrNull() ?: -1,
            exitCode = parts[3].toIntOrNull() ?: 0,
            rssMemoryBytes = parts[4].toLongOrNull() ?: 0L,
            surfaceFps = parts[5].toFloatOrNull() ?: 0f,
            renderedFrames = parts[6].toLongOrNull() ?: 0L,
            dispatchedInputEvents = parts[7].toIntOrNull() ?: 0,
            statusText = parts[8],
            recentLogs = logs
        )
    }

    private external fun nativeProbeGpuHardware(): String
    private external fun nativeComputeFileSha256(path: String): String
    private external fun nativeInspectPe32Binary(path: String): String
    private external fun nativePatchLargeAddressAware(path: String, enable: Boolean): String
    private external fun nativeInspectRenderWareImg(path: String): String
    private external fun nativeExtractRuntimeArchive(archivePath: String, destDir: String): String
    private external fun nativeVerifyInstalledRuntime(runtimeRootDir: String): String
    private external fun nativeStartSession(
        runtimeRootDir: String,
        gameDirPath: String,
        exeName: String,
        cpuTranslatorDll: String,
        maliBackend: String,
        useDxvk: Boolean,
        renderWidth: Int,
        renderHeight: Int,
        box64SafeFlags: Int,
        box64StrongMem: Int,
        box64BigBlock: Int,
        box64CallRet: Boolean,
        box64FastNan: Boolean,
        enableEsync: Boolean
    ): String
    private external fun nativeStopSession()
    private external fun nativeAttachSurface(surface: Surface, width: Int, height: Int)
    private external fun nativeDetachSurface()
    private external fun nativeSendKeyInput(win32VkCode: Int, pressed: Boolean)
    private external fun nativeSendMouseDelta(dx: Float, dy: Float, buttonMask: Int)
    private external fun nativePollSessionTelemetry(): String

    companion object {
        @Volatile
        private var isLibraryLoaded = false

        @Synchronized
        fun ensureLoaded() {
            if (!isLibraryLoaded) {
                try {
                    System.loadLibrary(RunnerConstants.NATIVE_LIB_NAME)
                    isLibraryLoaded = true
                } catch (_: UnsatisfiedLinkError) {
                    isLibraryLoaded = false
                }
            }
        }
    }
}
