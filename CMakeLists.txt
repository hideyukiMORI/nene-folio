cmake_minimum_required(VERSION 3.31)
# 版はここが唯一の出どころ（DEVELOPMENT_WORKFLOW 第 9 節）。
project(NeNeFolio VERSION 0.1.0 LANGUAGES C)

if(NOT CMAKE_C_COMPILER_ID STREQUAL "Clang" OR NOT CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(FATAL_ERROR "QLT-011: this gate requires the pinned clang-cl compiler (CC=clang-cl)")
endif()

# CMAKE_C_EXTENSIONS / CMAKE_C_STANDARD は書かない。書くと CMake が /std:c17 を足し、clang-cl が未使用引数として拒否する（ADR 0003）。
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
# CRT は常に /MT。Debug でも -MTd にしない（ASan が Debug CRT を支持しない・ADR 0004）。実行時依存 0（R1）。
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
# CMake の既定は user32 / gdi32 / advapi32 等を全 target に結ぶので、platformLibraries（ARC-002）を
# 物理的な制約にするため kernel32 だけに絞る。OS ライブラリは nenefolio_system_link でだけ足す。
set(CMAKE_C_STANDARD_LIBRARIES "kernel32.lib")

include(eng/targets.cmake)
# clang-tidy の cl ドライバは compile line の /clang: を落とすので、言語標準は extra-arg で同じ変数から渡す（ADR 0003）。
find_program(NENETIDY clang-tidy REQUIRED)
set(CMAKE_C_CLANG_TIDY "${NENETIDY};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy;--extra-arg=${NENEFOLIO_C_STANDARD_FLAG}")
enable_testing()
nenefolio_target(toolchain_smoke verification EXECUTABLE tests/build/toolchain_smoke.c)
add_test(NAME toolchain_smoke COMMAND toolchain_smoke)

nenefolio_target(nenefolio_core core STATIC
    src/core/rgb_color.c src/core/utf8_text.c src/core/utf16_text.c
    src/core/json_reader.c src/core/json_writer.c src/core/name_list.c
    src/core/category_ledger.c src/core/note_ledger.c src/core/drawer_layout.c
    src/core/text_buffer.c src/core/note_text.c src/core/markdown_rtf.c src/core/rtf_palette.c
    src/core/folio_command.c)
nenefolio_target(nenefolio_application application STATIC src/application/folio_state.c)
nenefolio_link(nenefolio_application nenefolio_core)

nenefolio_target(folio_tests unit_tests EXECUTABLE
    tests/unit/folio_tests.c tests/unit/text_tests.c tests/unit/json_tests.c
    tests/unit/ledger_tests.c tests/unit/layout_tests.c tests/unit/state_tests.c
    tests/unit/allocation_probe.c tests/unit/allocation_tests.c tests/unit/markdown_tests.c
    tests/unit/command_tests.c)
nenefolio_link(folio_tests nenefolio_core)
nenefolio_link(folio_tests nenefolio_application)
add_test(NAME folio_unit COMMAND folio_tests)

nenefolio_target(nenefolio_adapters adapters_win32 STATIC
    src/adapters/win32/file_bytes.c src/adapters/win32/persistence_adapter.c
    src/adapters/win32/appearance_adapter.c)
nenefolio_link(nenefolio_adapters nenefolio_core)
nenefolio_link(nenefolio_adapters nenefolio_application)
nenefolio_system_link(nenefolio_adapters kernel32)
nenefolio_system_link(nenefolio_adapters advapi32)

nenefolio_target(nenefolio_window ui_win32 STATIC
    src/ui/win32/drawer_window.c src/ui/win32/folio_window.c src/ui/win32/note_pane.c
    src/ui/win32/folio_palette.c src/ui/win32/failure_box.c)
nenefolio_link(nenefolio_window nenefolio_core)
nenefolio_link(nenefolio_window nenefolio_application)
nenefolio_system_link(nenefolio_window user32)
nenefolio_system_link(nenefolio_window gdi32)
nenefolio_system_link(nenefolio_window dwmapi)
nenefolio_system_link(nenefolio_window comdlg32)

nenefolio_target(nenefolio app EXECUTABLE src/app/main.c src/app/NeNeFolio.manifest)
set_property(TARGET nenefolio PROPERTY WIN32_EXECUTABLE TRUE)
set_property(TARGET nenefolio PROPERTY OUTPUT_NAME NeNeFolio)
nenefolio_link(nenefolio nenefolio_core)
nenefolio_link(nenefolio nenefolio_application)
nenefolio_link(nenefolio nenefolio_adapters)
nenefolio_link(nenefolio nenefolio_window)
nenefolio_system_link(nenefolio user32)
