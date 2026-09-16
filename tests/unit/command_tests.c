#include "folio_command.h"
#include "unit_tests.h"

#include <string.h>

static void expect_command(const char *_Nonnull text, enum folio_command expected,
                           const char *_Nonnull description)
{
    enum folio_command command = FOLIO_COMMAND_HELP;
    size_t argument = 0;
    require(folio_command_parse(text, strlen(text), &command, &argument) && command == expected,
            description);
}

static void verify_aliases(void)
{
    expect_command("w", FOLIO_COMMAND_SAVE, "w");
    expect_command(":write", FOLIO_COMMAND_SAVE, ":write");
    expect_command(" q ", FOLIO_COMMAND_QUIT, "trim q");
    expect_command(": quit\t", FOLIO_COMMAND_QUIT, "trim :quit");
    expect_command("wq", FOLIO_COMMAND_SAVE_QUIT, "wq");
    expect_command(":x", FOLIO_COMMAND_SAVE_QUIT, ":x");
    expect_command("q!", FOLIO_COMMAND_FORCE_QUIT, "q!");
    expect_command(":help", FOLIO_COMMAND_HELP, ":help");
    expect_command(":h", FOLIO_COMMAND_HELP, ":h uses the same help command");
    expect_command(":startinsert", FOLIO_COMMAND_EDIT, "startinsert begins editing");
    expect_command(":saveas 別の 名前.md", FOLIO_COMMAND_SAVE_AS, "saveas with Japanese name");
    expect_command(":saveas", FOLIO_COMMAND_SAVE_AS, "saveas opens the common name form");
    expect_command(":enew", FOLIO_COMMAND_NEW, "enew creates an untitled note");
}

static void verify_rejections(void)
{
    static const char *const rejected[] = {"",
                                           ":",
                                           "W",
                                           "quit!",
                                           "enew now",
                                           "unknown",
                                           ":e",
                                           ":edit",
                                           ":view",
                                           ":startinsert file",
                                           "保存して閲覧"};
    for (size_t index = 0; index < sizeof rejected / sizeof rejected[0]; ++index)
    {
        enum folio_command command = FOLIO_COMMAND_HELP;
        size_t argument = 0;
        require(!folio_command_parse(rejected[index], strlen(rejected[index]), &command, &argument),
                "invalid Ex input is rejected");
    }
}

static void verify_catalog(void)
{
    require(folio_command_count() == 9, "only implemented commands are registered");
    const enum folio_command expected[] = {
        FOLIO_COMMAND_SAVE,       FOLIO_COMMAND_QUIT, FOLIO_COMMAND_SAVE_QUIT,
        FOLIO_COMMAND_FORCE_QUIT, FOLIO_COMMAND_HELP, FOLIO_COMMAND_EDIT,
        FOLIO_COMMAND_VIEW,       FOLIO_COMMAND_NEW,  FOLIO_COMMAND_SAVE_AS};
    for (size_t index = 0; index < folio_command_count(); ++index)
    {
        enum folio_command command = folio_command_at(index);
        require(command == expected[index], "catalog order is stable");
        require(strlen(folio_command_label(command)) > 0, "palette label comes from catalog");
        for (size_t alias = 0; alias < folio_command_alias_count(command); ++alias)
        {
            enum folio_command parsed = FOLIO_COMMAND_HELP;
            size_t argument = 0;
            const char *_Nonnull name = folio_command_alias(command, alias);
            require(folio_command_parse(name, strlen(name), &parsed, &argument) &&
                        parsed == command,
                    "every catalog alias parses to its command");
        }
    }
    require(folio_command_matches(FOLIO_COMMAND_SAVE, "", 0), "empty palette query shows all");
    require(folio_command_matches(FOLIO_COMMAND_SAVE_QUIT, "wq", 2), "palette finds an alias");
    require(folio_command_matches(FOLIO_COMMAND_FORCE_QUIT, "破棄", strlen("破棄")),
            "palette finds a display label");
    require(!folio_command_matches(FOLIO_COMMAND_HELP, "quit", 4), "palette excludes a mismatch");
    require(folio_command_alias_count(FOLIO_COMMAND_VIEW) == 0,
            "GUI view has no misleading Vim alias");
    require(folio_command_matches(FOLIO_COMMAND_VIEW, "閲覧", strlen("閲覧")),
            "GUI-only command can be found by Japanese label");
}

void run_command_tests(void)
{
    verify_aliases();
    verify_rejections();
    verify_catalog();
}
