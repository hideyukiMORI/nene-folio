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

int main(int argc, char *_Nonnull *_Nonnull argv)
{
    run_text_tests();
    /* eng/coverage.py の反例: 台帳・配置・状態のテストを省いた実行では分岐 90%
     * に届かないことを示す。 */
    if (argc == 2 && strcmp(argv[1], "--coverage-negative") == 0)
    {
        return 0;
    }
    run_json_tests();
    run_ledger_tests();
    run_layout_tests();
    run_markdown_tests();
    run_command_tests();
    run_note_name_tests();
    run_state_tests();
    run_allocation_tests();
    printf("folio unit tests passed\n");
    return 0;
}
