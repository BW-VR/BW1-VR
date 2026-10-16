# The headset app of the mod: libbwvr_quest.so, the native activity of the
# APK (docs/android.md). bwvr/tools/android/build_apk.ps1 configures this
# folder with the toolchain of the Android NDK, once for each ABI, and puts
# the libraries into the APK.
cmake_minimum_required(VERSION 3.20)
project(bwvr_quest LANGUAGES C CXX)

if(NOT ANDROID)
  message(FATAL_ERROR
      "This folder builds for Android. Use bwvr/tools/android/build_apk.ps1.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BWVR_THIRD_PARTY "${CMAKE_SOURCE_DIR}/../../third_party" CACHE PATH
    "The folder that holds OpenXR-SDK and openxr-android")
set(BWVR_BUILD_ID_NOW "unknown" CACHE STRING "The build id for the log")
configure_file("${CMAKE_SOURCE_DIR}/../src/common/build_id.h.in"
               "${CMAKE_BINARY_DIR}/generated/build_id.h" @ONLY)

# The OpenXR loader of Khronos for Android, release 1.1.43
# (third_party/README.md).
set(XR_LOADER "${BWVR_THIRD_PARTY}/openxr-android/${ANDROID_ABI}/libopenxr_loader.so")
if(NOT EXISTS "${XR_LOADER}")
  message(FATAL_ERROR "The OpenXR loader for ${ANDROID_ABI} is not at ${XR_LOADER}")
endif()
add_library(openxr_loader SHARED IMPORTED)
set_target_properties(openxr_loader PROPERTIES IMPORTED_LOCATION "${XR_LOADER}")

add_library(native_app_glue STATIC
    "${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
target_include_directories(native_app_glue PUBLIC
    "${ANDROID_NDK}/sources/android/native_app_glue")

add_library(bwvr_quest SHARED
    src/main.cpp
    src/app_log.cpp
    src/canvas.cpp
    src/flat_view.cpp
    src/game_folder.cpp
    src/gpu.cpp
    src/menu_paint.cpp
    src/xr_app.cpp
)
target_include_directories(bwvr_quest PRIVATE
    "${BWVR_THIRD_PARTY}/OpenXR-SDK/include"
    "${CMAKE_BINARY_DIR}/generated")
target_compile_options(bwvr_quest PRIVATE -Wall -Wextra -Wno-unused-parameter
    -Wno-missing-field-initializers)
# The glue calls ANativeActivity_onCreate, and the linker must keep it. The
# pages of 16 KB fit each device with Android 15 and later too.
target_link_options(bwvr_quest PRIVATE
    "-u" "ANativeActivity_onCreate"
    "-Wl,-z,max-page-size=16384")
target_link_libraries(bwvr_quest PRIVATE
    native_app_glue openxr_loader vulkan android jnigraphics log)
