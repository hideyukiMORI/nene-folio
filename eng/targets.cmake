file(READ "${CMAKE_SOURCE_DIR}/eng/architecture.json" NENE_ARCHITECTURE)

# 言語標準と警告の集合はここが唯一の出どころ（ADR 0003 の実測に対応する）。CMake 3.31 は clang-cl の C23 を知らない。
# -Wno-nullability-extension: -Wpedantic が _Nonnull / _Nullable を「clang 拡張」として拒否するため、
# -Wnullability-completeness（C-004）を使うにはこの名指しの選択が要る（2026-09-09 実測・ADR 0004）。
set(NENEFOLIO_C_STANDARD_FLAG /clang:-std=c23)
set(NENEFOLIO_C_OPTIONS
    /W4 /WX /utf-8 ${NENEFOLIO_C_STANDARD_FLAG}
    -Wextra -Wpedantic
    -Wno-switch-default -Wno-pre-c23-compat -Wno-nullability-extension
    -Wswitch-enum -Wcovered-switch-default
    -Wcast-qual -Wconversion -Wsign-conversion -Wshadow
    -Wimplicit-fallthrough -Wnullability-completeness
    -Werror=vla -Wstrict-prototypes -Wmissing-prototypes
    -Wdouble-promotion -Wformat=2 -Wundef -Wunused-result)

# 検出層（C-016 / ADR 0004）。Debug 構成の全 target を ASan / UBSan / nullability で計装し、回復させない。
# ASan は Debug CRT（-MTd）と共存できないので、CRT は CMakeLists.txt が常に /MT に固定する。
# ランタイムは clang-cl の resource dir から lld-link へ明示的に結ぶ（CMake は link を driver 経由で呼ばない）。
execute_process(COMMAND "${CMAKE_C_COMPILER}" /clang:-print-resource-dir
                OUTPUT_VARIABLE NENEFOLIO_RESOURCE_DIR OUTPUT_STRIP_TRAILING_WHITESPACE
                COMMAND_ERROR_IS_FATAL ANY)
set(NENEFOLIO_SANITIZE_OPTIONS
    -fsanitize=address -fsanitize=undefined -fsanitize=nullability -fno-sanitize-recover=all)
set(NENEFOLIO_SANITIZE_RUNTIME "${NENEFOLIO_RESOURCE_DIR}/lib/windows")
set(NENEFOLIO_SANITIZE_LIBRARIES
    "${NENEFOLIO_SANITIZE_RUNTIME}/clang_rt.asan-x86_64.lib"
    "${NENEFOLIO_SANITIZE_RUNTIME}/clang_rt.asan_cxx-x86_64.lib"
    "${NENEFOLIO_SANITIZE_RUNTIME}/clang_rt.ubsan_standalone-x86_64.lib"
    "${NENEFOLIO_SANITIZE_RUNTIME}/clang_rt.ubsan_standalone_cxx-x86_64.lib")

function(nenefolio_target target module kind)
    string(JSON module_path ERROR_VARIABLE module_error GET "${NENE_ARCHITECTURE}" modules "${module}" path)
    if(module_error)
        message(FATAL_ERROR "ARC-002: unapproved module ${module}")
    endif()
    if(NOT ARGN)
        message(FATAL_ERROR "ARC-002: empty future modules are forbidden")
    endif()
    foreach(source IN LISTS ARGN)
        cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${CMAKE_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE absolute_source)
        set(module_root "${CMAKE_SOURCE_DIR}/${module_path}")
        cmake_path(IS_PREFIX module_root "${absolute_source}" NORMALIZE inside)
        if(NOT inside)
            message(FATAL_ERROR "ARC-002: ${source} does not belong to ${module}")
        endif()
    endforeach()
    if(kind STREQUAL "EXECUTABLE")
        add_executable(${target} ${ARGN})
        target_link_options(${target} PRIVATE
            "$<$<CONFIG:Debug>:/INCREMENTAL:NO;/WHOLEARCHIVE:${NENEFOLIO_SANITIZE_RUNTIME}/clang_rt.asan-x86_64.lib>")
        foreach(library IN LISTS NENEFOLIO_SANITIZE_LIBRARIES)
            target_link_libraries(${target} PRIVATE "$<$<CONFIG:Debug>:${library}>")
        endforeach()
    elseif(kind STREQUAL "STATIC")
        add_library(${target} STATIC ${ARGN})
    else()
        message(FATAL_ERROR "ARC-002: unsupported target kind ${kind}")
    endif()
    set_property(TARGET ${target} PROPERTY NENE_MODULE "${module}")
    target_compile_options(${target} PRIVATE ${NENEFOLIO_C_OPTIONS}
                           "$<$<CONFIG:Debug>:${NENEFOLIO_SANITIZE_OPTIONS}>")
    target_include_directories(${target} PUBLIC "${CMAKE_SOURCE_DIR}/${module_path}")
    if(module MATCHES "^(adapters_win32|ui_win32|app)$")
        target_compile_definitions(${target} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    endif()
endfunction()

function(nenefolio_system_link target library)
    get_target_property(module ${target} NENE_MODULE)
    string(JSON allowed ERROR_VARIABLE error GET "${NENE_ARCHITECTURE}" platformLibraries "${module}")
    if(error)
        message(FATAL_ERROR "ARC-002: no platform libraries for ${module}")
    endif()
    string(JSON count LENGTH "${allowed}")
    math(EXPR last "${count} - 1")
    foreach(index RANGE 0 ${last})
        string(JSON candidate GET "${allowed}" ${index})
        if(candidate STREQUAL library)
            target_link_libraries(${target} PRIVATE ${library})
            return()
        endif()
    endforeach()
    message(FATAL_ERROR "ARC-002: forbidden platform library ${module} -> ${library}")
endfunction()

function(nenefolio_link target dependency)
    get_target_property(module ${target} NENE_MODULE)
    get_target_property(destination ${dependency} NENE_MODULE)
    string(JSON allowed GET "${NENE_ARCHITECTURE}" modules "${module}" dependencies)
    string(JSON count LENGTH "${allowed}")
    set(found FALSE)
    if(count GREATER 0)
        math(EXPR last "${count} - 1")
        foreach(index RANGE 0 ${last})
            string(JSON candidate GET "${allowed}" ${index})
            if(candidate STREQUAL destination)
                set(found TRUE)
            endif()
        endforeach()
    endif()
    if(NOT found)
        message(FATAL_ERROR "ARC-002: forbidden dependency ${module} -> ${destination}")
    endif()
    target_link_libraries(${target} PRIVATE ${dependency})
endfunction()
