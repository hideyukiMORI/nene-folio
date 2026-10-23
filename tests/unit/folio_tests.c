#include "unit_tests.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void require(bool condition, const char *_Nonnull description)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", description);
        exit(1);
    }
}

bool same_text(const char *_Nonnull actual, const char *_Nonnull expected)
{
    if (strcmp(actual, expected) == 0)
    {
        return true;
    }
    fprintf(stderr, "  actual:   %s\n  expected: %s\n", actual, expected);
    return false;
}

static bool run_document_settings_target_tests(const char *_Nonnull argument)
{
    if (strcmp(argument, "--system-contrast") == 0)
    {
        run_system_contrast_tests();
        run_ui_text_tests();
        printf("system contrast and UI text tests passed\n");
        return true;
    }
    if (strcmp(argument, "--accessibility") == 0)
    {
        run_accessibility_state_tests();
        run_ui_text_tests();
        printf("accessibility state and UI text tests passed\n");
        return true;
    }
    if (strcmp(argument, "--untitled-expansion-query") == 0)
    {
        run_untitled_expansion_query_tests();
        printf("untitled expansion query tests passed\n");
        return true;
    }
    if (strcmp(argument, "--unsaved-departure-state") == 0)
    {
        run_unsaved_departure_tests();
        printf("unsaved departure state tests passed\n");
        return true;
    }
    if (strcmp(argument, "--unsaved-departure") == 0)
    {
        run_first_save_target_tests();
        run_unsaved_departure_tests();
        run_command_tests();
        run_ui_text_tests();
        printf("unsaved departure, first-save, command and UI text tests passed\n");
        return true;
    }
    if (strcmp(argument, "--themes") == 0)
    {
        run_theme_tests();
        run_settings_tests();
        run_theme_state_tests();
        run_command_tests();
        run_ui_text_tests();
        printf("theme catalog, settings, state, Ex and UI text tests passed\n");
        return true;
    }
    if (strcmp(argument, "--layout-modes") == 0)
    {
        run_layout_mode_state_tests();
        run_command_tests();
        run_ui_text_tests();
        printf("layout state, command and UI text tests passed\n");
        return true;
    }
    return false;
}

static bool run_target_tests(const char *_Nonnull argument)
{
    if (strcmp(argument, "--markdown-rtf") == 0)
    {
        run_markdown_tests();
        return true;
    }
    if (strcmp(argument, "--commands-ui") == 0)
    {
        run_command_tests();
        run_ui_text_tests();
        printf("command and UI text tests passed\n");
        return true;
    }
    if (strcmp(argument, "--rename-d1") == 0)
    {
        run_ledger_tests();
        run_rename_tests();
        run_rename_state_tests();
        run_rename_allocation_tests();
        printf("rename D1 tests passed\n");
        return true;
    }
    if (strcmp(argument, "--rename-d2") == 0)
    {
        run_category_rename_state_tests();
        run_rename_state_tests();
        run_ui_text_tests();
        run_category_rename_allocation_tests();
        printf("rename D2 tests passed\n");
        return true;
    }
    if (strcmp(argument, "--note-size-limit") == 0)
    {
        run_note_size_text_tests();
        run_note_size_state_tests();
        run_ui_text_tests();
        printf("note size limit tests passed\n");
        return true;
    }
    if (strcmp(argument, "--note-nul") == 0)
    {
        run_note_nul_text_tests();
        run_note_nul_state_tests();
        run_ui_text_tests();
        printf("note NUL tests passed\n");
        return true;
    }
    if (strcmp(argument, "--index-cache-retry") == 0)
    {
        run_index_cache_retry_state_tests();
        printf("index cache retry state tests passed\n");
        return true;
    }
    if (strcmp(argument, "--index-cache-retry-oom") == 0)
    {
        run_index_cache_retry_allocation_tests();
        printf("index cache retry allocation tests passed\n");
        return true;
    }
    return false;
}

static bool run_filter_target_tests(const char *_Nonnull argument)
{
    if (strcmp(argument, "--filter-lookup") == 0)
    {
        run_filter_tests();
        run_bom_filter_tests();
        run_filter_lookup_state_tests();
        run_index_cache_retry_state_tests();
        printf("filter corpus and application boundary tests passed\n");
        return true;
    }
    if (strcmp(argument, "--filter-lookup-oom") == 0)
    {
        run_filter_allocation_tests();
        run_corpus_allocation_tests();
        run_index_cache_retry_allocation_tests();
        printf("filter corpus allocation and cache retry tests passed\n");
        return true;
    }
    return false;
}

static void run_bom_boundary_tests(void)
{
    run_text_tests();
    run_note_size_text_tests();
    run_note_nul_text_tests();
    run_json_tests();
    run_bom_ledger_tests();
    run_bom_settings_tests();
    run_bom_rename_tests();
    run_bom_filter_tests();
    run_bom_state_tests();
    run_bom_allocation_tests();
    printf("BOM boundary tests passed\n");
}

static bool run_pane_load_target_tests(const char *_Nonnull argument)
{
    if (strcmp(argument, "--pane-load") == 0)
    {
        run_pane_load_state_tests();
        run_ui_text_tests();
        printf("pane load state and UI text tests passed\n");
        return true;
    }
    return false;
}

static bool run_size_overflow_target_tests(const char *_Nonnull argument)
{
    if (strcmp(argument, "--size-overflow") != 0)
    {
        return false;
    }
    run_size_overflow_tests();
    run_text_tests();
    run_markdown_tests();
    run_replace_tests();
    run_replace_state_tests();
    run_size_overflow_state_tests();
    run_note_size_text_tests();
    run_note_size_state_tests();
    printf("size overflow, text, Markdown RTF, replacement and note size boundary tests passed\n");
    return true;
}

static bool run_menu_target_tests(const char *_Nonnull argument)
{
    if (strcmp(argument, "--menu-surface") != 0)
        return false;

    run_command_tests();
    run_ui_text_tests();
    run_menu_availability_tests();
    printf("menu catalog, shortcut, text and availability tests passed\n");
    return true;
    return true;
}

int main(int argc, char *_Nonnull *_Nonnull argv)
{
    if (argc == 2 && strcmp(argv[1], "--bom-boundary") == 0)
    {
        run_bom_boundary_tests();
        return 0;
    }
    if (argc == 2 &&
        (run_menu_target_tests(argv[1]) || run_size_overflow_target_tests(argv[1]) ||
         run_pane_load_target_tests(argv[1]) || run_document_settings_target_tests(argv[1]) ||
         run_target_tests(argv[1]) || run_filter_target_tests(argv[1])))
    {
        return 0;
    }
    run_text_tests();
    /* eng/coverage.py の反例: 台帳・配置・状態のテストを省いた実行では分岐 90%
     * に届かないことを示す。 */
    if (argc == 2 && strcmp(argv[1], "--coverage-negative") == 0)
    {
        return 0;
    }
    run_note_size_text_tests();
    run_size_overflow_tests();
    run_size_overflow_state_tests();
    run_note_size_state_tests();
    run_note_nul_text_tests();
    run_note_nul_state_tests();
    run_bom_ledger_tests();
    run_bom_settings_tests();
    run_bom_rename_tests();
    run_bom_filter_tests();
    run_bom_state_tests();
    run_bom_allocation_tests();
    run_json_tests();
    run_ledger_tests();
    run_layout_tests();
    run_markdown_tests();
    run_command_tests();
    run_menu_availability_tests();
    run_note_name_tests();
    run_category_name_tests();
    run_rename_tests();
    run_search_tests();
    run_filter_tests();
    run_settings_tests();
    run_theme_tests();
    run_line_index_tests();
    run_replace_tests();
    run_state_tests();
    run_accessibility_state_tests();
    run_unsaved_departure_tests();
    run_index_cache_retry_state_tests();
    run_index_cache_retry_allocation_tests();
    run_category_rename_state_tests();
    run_replace_state_tests();
    run_ui_text_tests();
    run_font_bundle_tests();
    run_allocation_tests();
    run_category_rename_allocation_tests();
    printf("folio unit tests passed\n");
    return 0;
}
