package com.example.data

object RunnerConstants {
    const val NATIVE_LIB_NAME = "gtasa_engine"
    const val DATASTORE_NAME = "sa_pc_runner_prefs"

    const val RUNTIME_DIR_NAME = "bionic-runtime"
    const val GAME_DIR_NAME = "gtasa-pc"
    const val STAGING_DIR_NAME = "staging"
    const val STAGING_ARCHIVE_FILENAME = "runtime_package.tar.xz"
    const val DEFAULT_EXE_FILENAME = "gta_sa.exe"
    const val DEFAULT_IMG_FILENAME = "gta3.img"

    const val GITHUB_RUNTIME_RELEASE_URL =
        "https://github.com/Fiki-io/gtasa-bionic-runtime/releases/download/runtime-v11.16/gtasa-bionic-runtime-arm64.tar.xz"

    const val MIME_ANY_BINARY = "*/*"
    const val STREAM_BUFFER_SIZE = 65536
    const val HTTP_CONNECT_TIMEOUT_SEC = 30L
    const val HTTP_READ_TIMEOUT_SEC = 120L
    const val TELEMETRY_POLL_INTERVAL_MS = 400L

    // DataStore Preference Keys
    const val KEY_CPU_TRANSLATOR = "pref_cpu_translator"
    const val KEY_MALI_BACKEND = "pref_mali_backend"
    const val KEY_RENDER_RESOLUTION = "pref_render_resolution"
    const val KEY_USE_DXVK = "pref_use_dxvk_d3d9"
    const val KEY_BOX64_STRONGMEM = "pref_box64_strongmem"
    const val KEY_BOX64_SAFEFLAGS = "pref_box64_safeflags"
    const val KEY_BOX64_CALLRET = "pref_box64_callret"
    const val KEY_ENABLE_ESYNC = "pref_enable_esync"
    const val KEY_LAST_ARCHIVE_SHA256 = "pref_last_archive_sha256"

    // Win32 Virtual-Key Codes for GTA SA & SA-MP Controls
    const val VK_TAB = 0x09
    const val VK_RETURN = 0x0D
    const val VK_SHIFT = 0x10
    const val VK_CONTROL = 0x11
    const val VK_ESCAPE = 0x1B
    const val VK_SPACE = 0x20
    const val VK_KEY_A = 0x41
    const val VK_KEY_D = 0x44
    const val VK_KEY_F = 0x46
    const val VK_KEY_S = 0x53
    const val VK_KEY_T = 0x54
    const val VK_KEY_W = 0x57
    const val MOUSE_BTN_LEFT = 1
    const val MOUSE_BTN_RIGHT = 2
}
