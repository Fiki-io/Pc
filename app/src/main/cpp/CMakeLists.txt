cmake_minimum_required(VERSION 3.22.1)
project("gtasa_engine" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_compile_definitions(
    XZ_DEC_CONCATENATED
    XZ_USE_CRC64
    XZ_DEC_ANY_CHECK
    XZ_DEC_X86
    XZ_DEC_ARM
    XZ_DEC_ARMTHUMB
    XZ_DEC_ARM64
)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/xz)

add_library(
    gtasa_engine
    SHARED
    xz/xz_crc32.c
    xz/xz_crc64.c
    xz/xz_sha256.c
    xz/xz_dec_stream.c
    xz/xz_dec_lzma2.c
    xz/xz_dec_bcj.c
    sha256_stream.cpp
    pe32_inspector.cpp
    tar_posix_extractor.cpp
    bionic_session_engine.cpp
    gtasa_jni_bridge.cpp
)

add_library(
    bionic_compat
    SHARED
    bionic_compat_preload.cpp
)

find_library(android-lib android)
find_library(dl-lib dl)
find_library(egl-lib EGL)
find_library(gles3-lib GLESv3)
find_library(log-lib log)
find_library(z-lib z)

target_link_libraries(
    bionic_compat
    ${dl-lib}
    ${log-lib}
)

target_link_libraries(
    gtasa_engine
    ${android-lib}
    ${dl-lib}
    ${egl-lib}
    ${gles3-lib}
    ${log-lib}
    ${z-lib}
)

