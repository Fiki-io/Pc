package com.example.data

enum class CpuTranslatorOption(val dllName: String) {
    BOX64_WOW64("wowbox64.dll"),
    FEX_WOW64("libwow64fex.dll");

    companion object {
        fun fromDllName(dll: String): CpuTranslatorOption {
            return entries.firstOrNull { it.dllName == dll } ?: BOX64_WOW64
        }
    }
}

enum class MaliGraphicsBackend(val backendId: String) {
    VIRGL_GLES32("virgl-gles32-mali"),
    ZINK_VULKAN("zink-vulkan-mali");

    companion object {
        fun fromId(id: String): MaliGraphicsBackend {
            return entries.firstOrNull { it.backendId == id } ?: VIRGL_GLES32
        }
    }
}

enum class RenderResolutionOption(
    val id: String,
    val width: Int,
    val height: Int
) {
    RES_540P("960x540", 960, 540),
    RES_720P("1280x720", 1280, 720),
    RES_900P("1600x900", 1600, 900),
    RES_1080P("1920x1080", 1920, 1080);

    companion object {
        fun fromId(id: String): RenderResolutionOption {
            return entries.firstOrNull { it.id == id } ?: RES_720P
        }
    }
}

data class GpuProbeData(
    val querySucceeded: Boolean = false,
    val vendor: String = "",
    val renderer: String = "",
    val glVersion: String = "",
    val shadingLanguageVersion: String = "",
    val maxTextureSize: Int = 0,
    val isMaliFamily: Boolean = false,
    val recommendedBackend: String = MaliGraphicsBackend.VIRGL_GLES32.backendId
)

data class RuntimeVerificationState(
    val isVerified: Boolean = false,
    val wineBinaryValidElf64: Boolean = false,
    val wowBox64Present: Boolean = false,
    val wow64FexPresent: Boolean = false,
    val d3d9Present: Boolean = false,
    val dsoundPresent: Boolean = false,
    val winsockPresent: Boolean = false,
    val maliVirglPresent: Boolean = false,
    val maliZinkPresent: Boolean = false,
    val verifiedModuleCount: Int = 0,
    val wineBinaryPath: String = "",
    val lastArchiveSha256: String = "",
    val errorMessage: String = ""
)

data class Pe32ExecutableState(
    val isSelected: Boolean = false,
    val isValidPe32: Boolean = false,
    val machine: Int = 0,
    val sectionCount: Int = 0,
    val timestamp: Long = 0L,
    val characteristics: Int = 0,
    val isLargeAddressAware: Boolean = false,
    val entryPointRva: Long = 0L,
    val imageBase: Long = 0L,
    val sizeOfImage: Long = 0L,
    val subsystem: Int = 0,
    val fileSizeBytes: Long = 0L,
    val detectedVersionCode: String = "",
    val importedDlls: List<String> = emptyList(),
    val localPath: String = "",
    val errorMessage: String = ""
)

data class RenderWareImgState(
    val isSelected: Boolean = false,
    val isValidVer2: Boolean = false,
    val totalEntries: Int = 0,
    val dffModelCount: Int = 0,
    val txdTextureCount: Int = 0,
    val colCollisionCount: Int = 0,
    val ifpAnimCount: Int = 0,
    val fileSizeBytes: Long = 0L,
    val sectorBoundsValid: Boolean = false,
    val errorMessage: String = ""
)

data class ExecutionProfileConfig(
    val cpuTranslator: CpuTranslatorOption = CpuTranslatorOption.BOX64_WOW64,
    val maliBackend: MaliGraphicsBackend = MaliGraphicsBackend.VIRGL_GLES32,
    val resolution: RenderResolutionOption = RenderResolutionOption.RES_720P,
    val useDxvkD3d9: Boolean = false,
    val box64StrongMem: Boolean = true,
    val box64SafeFlags: Boolean = true,
    val box64CallRet: Boolean = true,
    val enableEsync: Boolean = true
)

data class LiveSessionState(
    val isRunning: Boolean = false,
    val winePid: Int = -1,
    val virglPid: Int = -1,
    val exitCode: Int = 0,
    val rssMemoryBytes: Long = 0L,
    val surfaceFps: Float = 0f,
    val renderedFrames: Long = 0L,
    val dispatchedInputEvents: Int = 0,
    val statusText: String = "",
    val recentLogs: List<String> = emptyList()
)
