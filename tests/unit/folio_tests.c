#include "unit_tests.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_failure(const char *_Nonnull description)
{
    fprintf(stderr, "FAIL: %s\n", description);
    exit(1);
}

void require(bool condition, const char *_Nonnull description)
{
    if (!condition)
    {
        test_failure(description);
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

static bool run_text_validation_target_tests(const char *_Nonnull argument)
{
    if (strcmp(argument, "--text-validation") != 0)
        return false;
    run_text_tests();
    run_note_nul_text_tests();
    run_note_size_text_tests();
    printf("text validation tests passed\n");
    return true;
}

static bool run_note_search_target_tests(const char *_Nonnull argument)
{
    if (strcmp(argument, "--note-search") != 0)
    {
        return false;
    }
    run_search_tests();
    printf("note search tests passed\n");
    return true;
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

static bool run_bounded_filter_target_tests(const char *_Nonnull argument)
{
    if (strcmp(argument, "--bounded-index-warm") == 0)
    {
        run_index_bounded_warm_tests();
        printf("bounded warm index matching test passed\n");
        return true;
    }
    if (strcmp(argument, "--bounded-index-matching") == 0)
    {
        run_index_filter_tests();
        run_index_preparation_tests();
        run_filter_lookup_state_tests();
        run_index_cache_retry_state_tests();
        run_index_filter_size_tests();
        printf("bounded core and application matching tests passed\n");
        return true;
    }
    if (strcmp(argument, "--bounded-index-matching-oom") == 0)
    {
        run_index_filter_size_allocation_tests();
        run_index_preparation_allocation_tests();
        run_index_cache_retry_allocation_tests();
        printf("bounded matching allocation and ownership tests passed\n");
        return true;
    }
    return false;
}

static bool run_filter_target_tests(const char *_Nonnull argument)
{
    if (strcmp(argument, "--index-preparation") == 0)
    {
        run_index_preparation_tests();
        run_index_preparation_allocation_tests();
        run_filter_lookup_state_tests();
        run_index_cache_retry_state_tests();
        run_index_cache_retry_allocation_tests();
        run_ui_text_tests();
        printf("index preparation and affected filter boundaries passed\n");
        return true;
    }
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
    return run_bounded_filter_target_tests(argument);
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
    if (strcmp(argument, "--index-filter-size-allocation") == 0)
    {
        run_index_filter_size_allocation_tests();
        run_index_filter_tests();
        printf("index filter capacity and allocation tests passed\n");
        return true;
    }
    if (strcmp(argument, "--index-filter-size") == 0)
    {
        run_index_filter_size_tests();
        run_index_filter_tests();
        printf("index filter capacity and semantics tests passed\n");
        return true;
    }
    if (strcmp(argument, "--replacement-size-allocation") == 0)
    {
        run_replace_factory_allocation_tests();
        run_replace_factory_size_tests();
        run_replace_tests();
        printf("replacement capacity, failure injection and ownership tests passed\n");
        return true;
    }
    if (strcmp(argument, "--replacement-size") == 0)
    {
        run_replace_factory_size_tests();
        run_replace_tests();
        printf("replacement factory capacity and template tests passed\n");
        return true;
    }
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
}

static bool run_surface_target_tests(const char *_Nonnull argument)
{
    if (strcmp(argument, "--inline-phrase") == 0)
    {
        require(run_markdown_inline_phrase_tests(),
                "inline phrase selector requires actual allocation instrumentation");
        printf("Markdown inline phrase storage tests passed\n");
        return true;
    }
    if (strcmp(argument, "--markdown-tasks") == 0)
    {
        run_markdown_task_tests();
        printf("Markdown task marker tests passed\n");
        return true;
    }
    if (strcmp(argument, "--table-start-setext") == 0)
    {
        run_markdown_table_setext_tests();
        printf("Markdown table-start setext test passed\n");
        return true;
    }
    if (strcmp(argument, "--table-start-bare") == 0)
    {
        run_markdown_table_bare_tests();
        printf("Markdown table-start bare-header test passed\n");
        return true;
    }
    if (strcmp(argument, "--table-start-priority") == 0)
    {
        run_markdown_table_start_tests();
        printf("Markdown table-start priority and container tests passed\n");
        return true;
    }
    if (strcmp(argument, "--markdown-links") == 0)
    {
        run_markdown_link_tests();
        printf("Markdown link scope and escape tests passed\n");
        return true;
    }
    if (strcmp(argument, "--surface-rules") != 0)
        return false;
    run_theme_tests();
    run_ui_text_tests();
    run_name_unchanged_state_tests();
    printf("surface geometry text, theme contrast and unchanged-name tests passed\n");
    return true;
}

static bool run_selected_tests(const char *_Nonnull argument)
{
    if (strcmp(argument, "--storage-location") == 0)
    {
        run_storage_tests();
        run_storage_state_tests();
        run_ui_text_tests();
        printf("storage path, startup codec, state and text tests passed\n");
        return true;
    }
    if (strcmp(argument, "--inline-storage-oom") == 0)
    {
        require(run_markdown_inline_storage_allocation_tests() >= 3,
                "inline storage OOM selector requires metadata and factory failure injection");
        printf("Markdown inline storage allocation tests passed\n");
        return true;
    }
    if (strcmp(argument, "--inline-storage") == 0)
    {
        require(run_markdown_inline_storage_tests(),
                "inline storage selector requires actual allocation instrumentation");
        printf("Markdown inline storage tests passed\n");
        return true;
    }
    if (strcmp(argument, "--drawer-scroll-delta") == 0)
    {
        run_drawer_scroll_delta_tests();
        printf("drawer scroll delta boundary tests passed\n");
        return true;
    }
    if (strcmp(argument, "--markdown-tables") == 0)
    {
        run_markdown_table_indent_tests();
        printf("Markdown table indent and representation tests passed\n");
        return true;
    }
    if (strcmp(argument, "--table-indent-oom") == 0)
    {
        require(run_markdown_table_allocation_tests() != 0,
                "table indent OOM selector requires actual failure injection");
        printf("table indent allocation tests passed\n");
        return true;
    }
    if (strcmp(argument, "--nullable-allocation") == 0)
    {
        run_nullable_allocation_tests();
        run_bom_allocation_tests();
        run_replace_template_tests();
        printf("nullable allocation, preview ownership, BOM and template tests passed\n");
        return true;
    }
    return (run_text_validation_target_tests(argument) || run_note_search_target_tests(argument) ||
            run_surface_target_tests(argument) || run_menu_target_tests(argument) ||
            run_size_overflow_target_tests(argument) || run_pane_load_target_tests(argument) ||
            run_document_settings_target_tests(argument) || run_target_tests(argument) ||
            run_filter_target_tests(argument));
}

static void run_full_suite(void)
{
    run_storage_tests();
    run_storage_state_tests();
    run_text_tests();
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
    run_index_preparation_tests();
    run_index_preparation_allocation_tests();
    run_index_cache_retry_state_tests();
    run_index_cache_retry_allocation_tests();
    run_category_rename_state_tests();
    run_replace_state_tests();
    run_ui_text_tests();
    run_font_bundle_tests();
    run_allocation_tests();
    run_category_rename_allocation_tests();
    printf("folio unit tests passed\n");
}

int main(int argc, char *_Nonnull *_Nonnull argv)
{
    if (argc == 2 && strcmp(argv[1], "--bom-boundary") == 0)
    {
        run_bom_boundary_tests();
        return 0;
    }
    if (argc == 2 && run_selected_tests(argv[1]))
    {
        return 0;
    }
    if (argc != 1 && (argc != 2 || strcmp(argv[1], "--coverage-negative") != 0))
    {
        fprintf(stderr, "unknown test selector\n");
        return 2;
    }
    /* eng/coverage.py の反例は text のみを実行し、全体の分岐下限を満たさない。 */
    if (argc == 2 && strcmp(argv[1], "--coverage-negative") == 0)
    {
        run_text_tests();
        return 0;
    }
    run_full_suite();
    return 0;
}
