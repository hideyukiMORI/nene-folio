cmake_minimum_required(VERSION 3.31)
# 版はここが唯一の出どころ（DEVELOPMENT_WORKFLOW 第 9 節）。
project(NeNeFolio VERSION 1.3.1 LANGUAGES C RC)
set(NENEFOLIO_VERSION_COMPONENTS)
foreach(component MAJOR MINOR PATCH TWEAK)
    set(value "${PROJECT_VERSION_${component}}")
    if(value STREQUAL "")
        set(value 0)
    endif()
    if(value GREATER 65535)
        message(FATAL_ERROR "FR-014: Windows version components must be in 0..65535")
    endif()
    list(APPEND NENEFOLIO_VERSION_COMPONENTS "${value}")
endforeach()
list(JOIN NENEFOLIO_VERSION_COMPONENTS "." NENEFOLIO_WINDOWS_VERSION)
list(JOIN NENEFOLIO_VERSION_COMPONENTS "," NENEFOLIO_RC_VERSION)
configure_file(src/app/NeNeFolio.version.json.in product-version.json @ONLY)
configure_file(src/app/NeNeFolio.manifest.in generated/src/app/NeNeFolio.manifest @ONLY)

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
    src/core/rgb_color.c src/core/appearance_snapshot.c src/core/theme_palette.c
    src/core/theme_contrast.c
    src/core/utf8_text.c src/core/utf16_text.c
    src/core/json_reader.c src/core/json_writer.c src/core/name_list.c
    src/core/category_ledger.c src/core/category_name.c src/core/note_ledger.c src/core/drawer_layout.c
    src/core/text_buffer.c src/core/note_text.c src/core/note_file_times.c src/core/markdown_rtf.c src/core/rtf_palette.c
    src/core/rtf_text.c src/core/markdown_blocks.c src/core/markdown_inline_tokens.c
    src/core/code_language.c src/core/code_syntax.c
    src/core/diagram_flow_layout.c
    src/core/diagram_geometry.c
    src/core/diagram_label.c
    src/core/diagram_layout.c
    src/core/diagram_metrics.c
    src/core/diagram_model.c
    src/core/diagram_palette.c
    src/core/diagram_ranks.c
    src/core/diagram_scene.c
    src/core/diagram_sequence_layout.c
    src/core/diagram_statement.c
    src/core/diagram_text.c
    src/core/markdown_media.c
    src/core/mermaid_diagram.c
    src/core/mermaid_flow.c
    src/core/mermaid_sequence.c
    src/core/image_source.c src/core/image_bitmap.c src/core/markdown_images.c src/core/markdown_destination.c
    src/core/image_asset.c src/core/archive_assets.c src/core/markdown_image_copy.c
    src/core/markdown_autolink.c src/core/markdown_character_reference.c
    src/core/web_uri.c src/core/markdown_web_link.c src/core/note_archive.c
    src/core/markdown_line.c src/core/markdown_view.c src/core/markdown_table.c src/core/markdown_containers.c
    src/core/folio_command.c src/core/folio_menu_action.c src/core/folio_shortcut.c src/core/key_guide.c src/core/note_name.c src/core/rename_plan.c src/core/rename_journal.c
    src/core/note_search.c src/core/note_corpus.c src/core/index_filter.c src/core/index_filter_build.c
    src/core/storage_path.c src/core/startup_settings.c
    src/core/folio_settings.c src/core/folio_theme.c src/core/folio_theme_choice.c src/core/line_index.c
    src/core/replace_template.c src/core/replace_edit.c src/core/note_replace.c
    src/core/replace_preview.c
    src/core/ascii_fold.c src/core/ui_font.c src/core/ui_font_fallback.c
    src/core/ui_text.c src/core/font_bundle_verdict.c src/core/product_version.c)
target_compile_definitions(nenefolio_core PRIVATE NENEFOLIO_PRODUCT_VERSION="${PROJECT_VERSION}")
nenefolio_target(nenefolio_application application STATIC src/application/folio_state.c src/application/storage_state.c
    src/application/image_export.c)
nenefolio_link(nenefolio_application nenefolio_core)

nenefolio_target(folio_tests unit_tests EXECUTABLE
    tests/unit/storage_tests.c tests/unit/storage_state_tests.c
    tests/unit/web_link_tests.c tests/unit/transfer_tests.c tests/unit/image_tests.c
    tests/unit/folio_tests.c tests/unit/text_tests.c tests/unit/json_tests.c
    tests/unit/ledger_tests.c tests/unit/layout_tests.c tests/unit/state_tests.c
    tests/unit/allocation_probe.c tests/unit/allocation_tests.c tests/unit/markdown_tests.c
    tests/unit/code_syntax_tests.c
    tests/unit/diagram_tests.c
    tests/unit/command_tests.c tests/unit/note_name_tests.c tests/unit/rename_tests.c
    tests/unit/category_name_tests.c
    tests/unit/search_tests.c tests/unit/filter_tests.c tests/unit/settings_tests.c
    tests/unit/theme_tests.c tests/unit/line_index_tests.c tests/unit/replace_tests.c
    tests/unit/replace_state_tests.c tests/unit/replace_preview_tests.c tests/unit/ui_text_tests.c
    tests/unit/font_bundle_tests.c tests/unit/size_overflow_tests.c)
nenefolio_link(folio_tests nenefolio_core)
nenefolio_link(folio_tests nenefolio_application)
add_test(NAME folio_unit COMMAND folio_tests)

nenefolio_target(nenefolio_adapters adapters_win32 STATIC
    src/adapters/win32/storage_adapter.c src/adapters/win32/storage_native_path.c src/adapters/win32/data_lock.c
    src/adapters/win32/file_bytes.c src/adapters/win32/file_times.c src/adapters/win32/persistence_adapter.c
    src/adapters/win32/appearance_adapter.c src/adapters/win32/regex_adapter.c
    src/adapters/win32/web_link_adapter.c src/adapters/win32/transfer_adapter.c
    src/adapters/win32/diagram_canvas.c
    src/adapters/win32/diagram_native_shape.c
    src/adapters/win32/diagram_render.c
    src/adapters/win32/image_adapter.c src/adapters/win32/image_job.c src/adapters/win32/image_cache.c
    src/adapters/win32/image_decode.c src/adapters/win32/image_download.c src/adapters/win32/image_file.c
    src/adapters/win32/font_bundle.c src/adapters/win32/recycle_item.c)
nenefolio_link(nenefolio_adapters nenefolio_core)
nenefolio_link(nenefolio_adapters nenefolio_application)
nenefolio_system_link(nenefolio_adapters kernel32)
nenefolio_system_link(nenefolio_adapters shell32)
nenefolio_system_link(nenefolio_adapters ole32)
nenefolio_system_link(nenefolio_adapters uuid)
nenefolio_system_link(nenefolio_adapters windowscodecs)
nenefolio_system_link(nenefolio_adapters winhttp)
nenefolio_system_link(nenefolio_adapters advapi32)
# OS高コントラストと利用者のシステム色は外観adapterだけが読む（ADR0057）。
nenefolio_system_link(nenefolio_adapters user32)
# Windows 同梱の ICU（ADR 0028 の決定 1）。配布物は増えず、版は OS が決める。
nenefolio_system_link(nenefolio_adapters icu)
# 同梱の書体を AddFontResourceExW(FR_PRIVATE) で登録する（ADR 0036 の決定 3・6・ARC-002）。
nenefolio_system_link(nenefolio_adapters gdi32)

nenefolio_target(nenefolio_window ui_win32 STATIC
    src/ui/win32/storage_prompt.c src/ui/win32/folder_picker.c src/ui/win32/file_picker.c
    src/ui/win32/note_pane_links.c src/ui/win32/note_pane_images.c src/ui/win32/note_minimap.c src/ui/win32/note_pane_sample.c
    src/ui/win32/note_pane_geometry.c
    src/ui/win32/accessible_item.c src/ui/win32/accessibility_host.c src/ui/win32/accessibility_node.c
    src/ui/win32/accessibility_native.c
    src/ui/win32/drawer_window.c src/ui/win32/folio_window.c src/ui/win32/command_help_fonts.c src/ui/win32/note_pane.c src/ui/win32/note_pane_table.c
    src/ui/win32/folio_palette.c src/ui/win32/failure_box.c src/ui/win32/name_prompt.c
    src/ui/win32/ui_face.c src/ui/win32/icon_paint.c src/ui/win32/dialog_theme.c
    src/ui/win32/ui_part.c src/ui/win32/ui_pointer.c src/ui/win32/modal_owner.c src/ui/win32/name_prompt_metrics.c
    src/ui/win32/command_candidates.c src/ui/win32/theme_candidates.c)
nenefolio_link(nenefolio_window nenefolio_core)
nenefolio_link(nenefolio_window nenefolio_application)
nenefolio_system_link(nenefolio_window user32)
nenefolio_system_link(nenefolio_window gdi32)
nenefolio_system_link(nenefolio_window dwmapi)
nenefolio_system_link(nenefolio_window comdlg32)
# 標準Tooltip control（ADR0048）。OS同梱DLL、外部配布物は増えない。
nenefolio_system_link(nenefolio_window comctl32)
# Windows同梱のUI Automationとnative controlへの名前注釈（ADR0056）。
nenefolio_system_link(nenefolio_window uiautomationcore)
nenefolio_system_link(nenefolio_window ole32)
# OS folder pickerの初期位置をshell itemへ写す（ADR0077）。
nenefolio_system_link(nenefolio_window shell32)
nenefolio_system_link(nenefolio_window oleaut32)
nenefolio_system_link(nenefolio_window oleacc)
nenefolio_system_link(nenefolio_window uuid)
# 設定画面のレイヤーから IME 文脈を外す（ADR 0031 の決定 7・ARC-002）。
nenefolio_system_link(nenefolio_window imm32)
# 閉じる・設定・折畳・選択の印を面のパスで塗る（ADR 0033 の決定 1・ARC-002）。
nenefolio_system_link(nenefolio_window gdiplus)

set_source_files_properties(src/app/NeNeFolio.manifest.in src/app/NeNeFolio.version.json.in
    PROPERTIES HEADER_FILE_ONLY TRUE)
set_source_files_properties(src/app/NeNeFolio.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/generated/src/app/NeNeFolio.manifest")
nenefolio_target(nenefolio app EXECUTABLE src/app/main.c src/app/NeNeFolio.rc
    src/app/NeNeFolio.manifest.in src/app/NeNeFolio.version.json.in)
target_compile_definitions(nenefolio PRIVATE
    "$<$<COMPILE_LANGUAGE:RC>:NENEFOLIO_RC_VERSION=${NENEFOLIO_RC_VERSION}>"
    "$<$<COMPILE_LANGUAGE:RC>:NENEFOLIO_PRODUCT_VERSION=\"${PROJECT_VERSION}\">"
    "$<$<COMPILE_LANGUAGE:RC>:NENEFOLIO_WINDOWS_VERSION=\"${NENEFOLIO_WINDOWS_VERSION}\">")
target_include_directories(nenefolio PRIVATE
    "$<$<COMPILE_LANGUAGE:RC>:${CMAKE_CURRENT_BINARY_DIR}/generated/src/app>")
target_link_options(nenefolio PRIVATE /MANIFEST:NO)
set_property(TARGET nenefolio PROPERTY WIN32_EXECUTABLE TRUE)
set_property(TARGET nenefolio PROPERTY OUTPUT_NAME NeNeFolio)
nenefolio_link(nenefolio nenefolio_core)
nenefolio_link(nenefolio nenefolio_application)
nenefolio_link(nenefolio nenefolio_adapters)
nenefolio_link(nenefolio nenefolio_window)
nenefolio_system_link(nenefolio user32)
# ADR 0036 決定 6: 同梱の書体を exe の隣の fonts/ へ複写する（中身の一致は CNF-012 が守る）
add_custom_command(TARGET nenefolio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/fonts $<TARGET_FILE_DIR:nenefolio>/fonts
    VERBATIM)

# #504: 設定のUIA巡回が有限で一意なことを実windowの公開境界で固定する。
nenefolio_target(settings_accessibility_tests win32_tests EXECUTABLE
    tests/win32/settings_accessibility_tests.c tests/win32/settings_accessibility_tests.rc)
nenefolio_link(settings_accessibility_tests nenefolio_core)
nenefolio_link(settings_accessibility_tests nenefolio_application)
nenefolio_link(settings_accessibility_tests nenefolio_adapters)
nenefolio_link(settings_accessibility_tests nenefolio_window)
nenefolio_system_link(settings_accessibility_tests user32)
nenefolio_system_link(settings_accessibility_tests ole32)
target_link_options(settings_accessibility_tests PRIVATE /MANIFEST:NO)
target_include_directories(settings_accessibility_tests PRIVATE
    "$<$<COMPILE_LANGUAGE:RC>:${CMAKE_CURRENT_BINARY_DIR}/generated/src/app>")
set_source_files_properties(tests/win32/settings_accessibility_tests.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/generated/src/app/NeNeFolio.manifest")
set_target_properties(settings_accessibility_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/verification-settings")
add_custom_command(TARGET settings_accessibility_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/fonts $<TARGET_FILE_DIR:settings_accessibility_tests>/fonts
    VERBATIM)
add_test(NAME settings_accessibility COMMAND settings_accessibility_tests)
set_tests_properties(settings_accessibility PROPERTIES TIMEOUT 30)

nenefolio_target(minimap_structure_tests win32_tests EXECUTABLE tests/win32/minimap_structure_tests.c)
nenefolio_link(minimap_structure_tests nenefolio_core)
nenefolio_link(minimap_structure_tests nenefolio_application)
nenefolio_link(minimap_structure_tests nenefolio_window)
nenefolio_system_link(minimap_structure_tests user32)
nenefolio_system_link(minimap_structure_tests ole32)
add_test(NAME minimap_structure COMMAND minimap_structure_tests)
set_tests_properties(minimap_structure PROPERTIES TIMEOUT 30)

# #510: document readiness and bounded idle work without timer delivery.
nenefolio_target(minimap_scheduling_tests win32_tests EXECUTABLE
    tests/win32/minimap_scheduling_tests.c tests/win32/minimap_scheduling_tests.rc)
nenefolio_link(minimap_scheduling_tests nenefolio_core)
nenefolio_link(minimap_scheduling_tests nenefolio_application)
nenefolio_link(minimap_scheduling_tests nenefolio_adapters)
nenefolio_link(minimap_scheduling_tests nenefolio_window)
nenefolio_system_link(minimap_scheduling_tests user32)
nenefolio_system_link(minimap_scheduling_tests ole32)
target_link_options(minimap_scheduling_tests PRIVATE /MANIFEST:NO)
target_include_directories(minimap_scheduling_tests PRIVATE
    "$<$<COMPILE_LANGUAGE:RC>:${CMAKE_CURRENT_BINARY_DIR}/generated/src/app>")
set_source_files_properties(tests/win32/minimap_scheduling_tests.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/generated/src/app/NeNeFolio.manifest")
set_target_properties(minimap_scheduling_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/verification-minimap")
add_custom_command(TARGET minimap_scheduling_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/fonts $<TARGET_FILE_DIR:minimap_scheduling_tests>/fonts
    VERBATIM)
add_test(NAME minimap_scheduling COMMAND minimap_scheduling_tests)
set_tests_properties(minimap_scheduling PROPERTIES TIMEOUT 30)

# #520: product image path on isolated native controls.
nenefolio_target(image_display_tests win32_tests EXECUTABLE tests/win32/image_display_tests.c)
nenefolio_link(image_display_tests nenefolio_core)
nenefolio_link(image_display_tests nenefolio_application)
nenefolio_link(image_display_tests nenefolio_window)
nenefolio_system_link(image_display_tests user32)
nenefolio_system_link(image_display_tests ole32)
add_test(NAME image_display COMMAND image_display_tests)
set_tests_properties(image_display PROPERTIES TIMEOUT 30)

nenefolio_target(image_adapter_tests win32_tests EXECUTABLE tests/win32/image_adapter_tests.c)
nenefolio_link(image_adapter_tests nenefolio_core)
nenefolio_link(image_adapter_tests nenefolio_application)
nenefolio_link(image_adapter_tests nenefolio_adapters)
nenefolio_system_link(image_adapter_tests ole32)
add_test(NAME image_adapter COMMAND image_adapter_tests)
set_tests_properties(image_adapter PROPERTIES TIMEOUT 60)

# #544: shared cache eviction and ownership, isolated from network and native windows.
nenefolio_target(image_cache_tests win32_tests EXECUTABLE tests/win32/image_cache_tests.c)
nenefolio_link(image_cache_tests nenefolio_core)
nenefolio_link(image_cache_tests nenefolio_application)
nenefolio_link(image_cache_tests nenefolio_adapters)
add_test(NAME image_cache COMMAND image_cache_tests)
set_tests_properties(image_cache PROPERTIES TIMEOUT 30)

# #539: native font measurement, raster, shared worker and exact cache keys.
nenefolio_target(diagram_render_tests win32_tests EXECUTABLE tests/win32/diagram_render_tests.c)
nenefolio_link(diagram_render_tests nenefolio_core)
nenefolio_link(diagram_render_tests nenefolio_application)
nenefolio_link(diagram_render_tests nenefolio_adapters)
nenefolio_system_link(diagram_render_tests ole32)
nenefolio_system_link(diagram_render_tests gdi32)
add_custom_command(TARGET diagram_render_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/fonts $<TARGET_FILE_DIR:diagram_render_tests>/fonts
    VERBATIM)
add_test(NAME diagram_render COMMAND diagram_render_tests)
set_tests_properties(diagram_render PROPERTIES TIMEOUT 60)

# #520: main-window progress and cancellation on an isolated desktop.
nenefolio_target(image_status_tests win32_tests EXECUTABLE
    tests/win32/image_status_tests.c tests/win32/image_status_tests.rc)
nenefolio_link(image_status_tests nenefolio_core)
nenefolio_link(image_status_tests nenefolio_application)
nenefolio_link(image_status_tests nenefolio_adapters)
nenefolio_link(image_status_tests nenefolio_window)
nenefolio_system_link(image_status_tests user32)
nenefolio_system_link(image_status_tests gdi32)
nenefolio_system_link(image_status_tests ole32)
target_link_options(image_status_tests PRIVATE /MANIFEST:NO)
target_include_directories(image_status_tests PRIVATE
    "$<$<COMPILE_LANGUAGE:RC>:${CMAKE_CURRENT_BINARY_DIR}/generated/src/app>")
set_source_files_properties(tests/win32/image_status_tests.rc PROPERTIES
    OBJECT_DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/generated/src/app/NeNeFolio.manifest")
set_target_properties(image_status_tests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/verification-images")
add_custom_command(TARGET image_status_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/fonts $<TARGET_FILE_DIR:image_status_tests>/fonts
    VERBATIM)
add_test(NAME image_status COMMAND image_status_tests)
add_test(NAME diagram_status COMMAND image_status_tests --mermaid)
add_test(NAME search_native COMMAND image_status_tests --search)
add_test(NAME note_switch_native COMMAND image_status_tests --note-switch)
add_test(NAME note_zoom_native COMMAND image_status_tests --zoom)
add_test(NAME theme_preview_native COMMAND image_status_tests --theme-preview)
add_test(NAME settings_edit_native COMMAND image_status_tests --settings-edit)
add_test(NAME note_mode_native COMMAND image_status_tests --mode-save)
add_test(NAME discard_native COMMAND image_status_tests --discard)
add_test(NAME palette_native COMMAND image_status_tests --palette)
add_test(NAME paste_native COMMAND image_status_tests --paste)
add_test(NAME editor_format_native COMMAND image_status_tests --format-keys)
add_test(NAME layout_edit_native COMMAND image_status_tests --layout)
set_tests_properties(layout_edit_native PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
set_tests_properties(editor_format_native PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
set_tests_properties(paste_native PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
set_tests_properties(palette_native PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
set_tests_properties(discard_native PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
set_tests_properties(note_mode_native PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
set_tests_properties(settings_edit_native PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
set_tests_properties(theme_preview_native PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
set_tests_properties(note_zoom_native PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
set_tests_properties(note_switch_native PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
set_tests_properties(search_native PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
add_test(NAME replace_native COMMAND image_status_tests --replace)
set_tests_properties(replace_native PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
add_test(NAME history_restore COMMAND image_status_tests --history)
set_tests_properties(history_restore PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
set_tests_properties(diagram_status PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
set_tests_properties(image_status PROPERTIES TIMEOUT 30)
foreach(status_case ja en zh find replace)
    add_test(NAME note_status_${status_case} COMMAND image_status_tests --status-${status_case})
    set_tests_properties(note_status_${status_case} PROPERTIES TIMEOUT 30)
endforeach()
set_tests_properties(image_status note_status_ja note_status_en note_status_zh
    note_status_find note_status_replace PROPERTIES RESOURCE_LOCK image_status_data)

# The same icon resource is used by the product and real-window verification hosts (ADR0088).
foreach(icon_target nenefolio image_status_tests minimap_scheduling_tests settings_accessibility_tests)
    target_include_directories(${icon_target} PRIVATE
        "$<$<COMPILE_LANGUAGE:RC>:${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows>")
endforeach()
foreach(icon_resource src/app/NeNeFolio.rc tests/win32/image_status_tests.rc
        tests/win32/minimap_scheduling_tests.rc tests/win32/settings_accessibility_tests.rc)
    set_property(SOURCE ${icon_resource} APPEND PROPERTY OBJECT_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/app-icon.rc"
        "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/NeNeFolio.ico")
endforeach()

add_test(NAME window_icon COMMAND image_status_tests --icon)
set_tests_properties(window_icon PROPERTIES TIMEOUT 30 RESOURCE_LOCK image_status_data)
