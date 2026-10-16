# The OpenXR test runtime for the Android emulator (test_runtime.cpp,
# docs/android.md, "Test the APK"). It is not in the APK.
# bwvr/tools/android/emu_xr_test.ps1 builds it for x86_64.
cmake_minimum_required(VERSION 3.20)
project(bwvr_test_runtime LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BWVR_THIRD_PARTY "${CMAKE_SOURCE_DIR}/../../../third_party" CACHE PATH
    "The folder that holds OpenXR-SDK")

add_library(bwvr_test_runtime SHARED test_runtime.cpp)
target_include_directories(bwvr_test_runtime PRIVATE
    "${BWVR_THIRD_PARTY}/OpenXR-SDK/include")
target_compile_options(bwvr_test_runtime PRIVATE -Wall -Wno-unused-parameter
    -Wno-missing-field-initializers -fvisibility=hidden)
target_link_libraries(bwvr_test_runtime PRIVATE log)
