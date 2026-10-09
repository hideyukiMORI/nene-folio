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

int main(int argc, char *_Nonnull *_Nonnull argv)
{
    if (argc == 2 && run_target_tests(argv[1]))
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
    run_note_size_state_tests();
    run_note_nul_text_tests();
    run_note_nul_state_tests();
    run_json_tests();
    run_ledger_tests();
    run_layout_tests();
    run_markdown_tests();
    run_command_tests();
    run_note_name_tests();
    run_category_name_tests();
    run_rename_tests();
    run_search_tests();
    run_filter_tests();
    run_settings_tests();
    run_line_index_tests();
    run_replace_tests();
    run_state_tests();
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
