cmake_minimum_required(VERSION 3.31)
# 版はここが唯一の出どころ（DEVELOPMENT_WORKFLOW 第 9 節）。
project(NeNeFolio VERSION 0.1.0 LANGUAGES C)

if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR "QLT-011: this gate requires the pinned clang-cl compiler (CC=clang-cl)")
endif()

# CMAKE_C_EXTENSIONS / CMAKE_C_STANDARD は書かない。書くと CMake が /std:c17 を足し、clang-cl が未使用引数として拒否する（ADR 0003）。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(eng/targets.cmake)
# clang-tidy の cl ドライバは compile line の /clang: を落とすので、言語標準は extra-arg で同じ変数から渡す（ADR 0003）。
find_program(NENETIDY clang-tidy REQUIRED)
set(CMAKE_C_CLANG_TIDY "${NENETIDY};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy;--extra-arg=${NENEFOLIO_C_STANDARD_FLAG}")
enable_testing()
nenefolio_target(toolchain_smoke verification EXECUTABLE tests/build/toolchain_smoke.c)
add_test(NAME toolchain_smoke COMMAND toolchain_smoke)
