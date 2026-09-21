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
    expect_command(":rename 新しい 名前.md", FOLIO_COMMAND_RENAME, "rename with a Japanese name");
    expect_command("rename", FOLIO_COMMAND_RENAME, "rename opens the common name form");
    expect_command(":set number", FOLIO_COMMAND_SET, ":set takes an option word");
    expect_command(":se nu!", FOLIO_COMMAND_SET, ":se is the same command");
    expect_command(":set", FOLIO_COMMAND_SET, ":set alone parses; the word is refused later");
}

/* `:set` の語（ADR 0026 の決定 8）。引数の開始位置は folio_command_parse が答える。 */
static void expect_option(const char *_Nonnull line, enum folio_option expected,
                          const char *_Nonnull description)
{
    enum folio_command command = FOLIO_COMMAND_HELP;
    size_t argument = 0;
    require(folio_command_parse(line, strlen(line), &command, &argument) &&
                command == FOLIO_COMMAND_SET,
            description);
    enum folio_option option = FOLIO_OPTION_NUMBER_HIDE;
    require(folio_command_parse_option(line + argument, strlen(line) - argument, &option) &&
                option == expected,
            description);
}

static void verify_options(void)
{
    expect_option(":set number", FOLIO_OPTION_NUMBER_SHOW, "number shows");
    expect_option(":set nu", FOLIO_OPTION_NUMBER_SHOW, "nu shows");
    expect_option(":se nonumber", FOLIO_OPTION_NUMBER_HIDE, "nonumber hides");
    expect_option(":set nonu", FOLIO_OPTION_NUMBER_HIDE, "nonu hides");
    expect_option(":set number!", FOLIO_OPTION_NUMBER_TOGGLE, "number! toggles");
    expect_option(":set nu!", FOLIO_OPTION_NUMBER_TOGGLE, "nu! toggles");
    expect_option(":set invnumber", FOLIO_OPTION_NUMBER_TOGGLE, "invnumber toggles");
    expect_option(":set invnu ", FOLIO_OPTION_NUMBER_TOGGLE, "invnu toggles and is trimmed");
    static const char *const refused[] = {"",          " ",     "numbers", "NUMBER",   "no",
                                          "number no", "nu nu", "!number", "invisible"};
    for (size_t index = 0; index < sizeof refused / sizeof refused[0]; ++index)
    {
        enum folio_option option = FOLIO_OPTION_NUMBER_SHOW;
        require(!folio_command_parse_option(refused[index], strlen(refused[index]), &option),
                "an unknown, missing or extra option word is refused");
        require(option == FOLIO_OPTION_NUMBER_SHOW, "a refused word leaves the option alone");
    }
}

/* パレットと「操作」メニューに出るのは、語を渡せる面で意味のある操作だけ（決定 8 の補正）。 */
static void verify_listed(void)
{
    require(!folio_command_listed(FOLIO_COMMAND_SET),
            "the Ex grammar that needs a word is not listed");
    for (size_t index = 0; index < folio_command_count(); ++index)
    {
        enum folio_command command = folio_command_at(index);
        require(folio_command_listed(command) == (command != FOLIO_COMMAND_SET),
                "every other operation stays listed");
    }
    require(
        folio_command_alias_count(FOLIO_COMMAND_TOGGLE_NUMBER) == 0 &&
            same_text(folio_command_label(FOLIO_COMMAND_TOGGLE_NUMBER), "行番号の表示を切り替える"),
        "the toggle is a listed operation with a Japanese label and no alias");
    require(folio_command_matches(FOLIO_COMMAND_TOGGLE_NUMBER, "行番号", strlen("行番号")),
            "the palette finds the toggle by label");
    /* パレットの箱の高さはこの数で決まる。総数（13）で取ると 1 行ぶん余る（補正 9）。 */
    require(folio_command_listed_count() == 12, "twelve operations are offered on a surface");
    require(folio_command_listed_count() == folio_command_count() - 1,
            "exactly the one unlisted Ex grammar is left out");
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
    require(folio_command_count() == 13, "only implemented commands are registered");
    const enum folio_command expected[] = {
        FOLIO_COMMAND_SAVE,         FOLIO_COMMAND_QUIT, FOLIO_COMMAND_SAVE_QUIT,
        FOLIO_COMMAND_FORCE_QUIT,   FOLIO_COMMAND_HELP, FOLIO_COMMAND_EDIT,
        FOLIO_COMMAND_VIEW,         FOLIO_COMMAND_NEW,  FOLIO_COMMAND_SAVE_AS,
        FOLIO_COMMAND_RENAME,       FOLIO_COMMAND_FIND, FOLIO_COMMAND_SET,
        FOLIO_COMMAND_TOGGLE_NUMBER};
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
    require(folio_command_matches(FOLIO_COMMAND_RENAME, "名前", strlen("名前")),
            "rename is found by its Japanese label");
    /* `?` はヘルプではなく後方検索になった（ADR 0023 の決定 6）。ヘルプは F1 / :h / Ctrl+P。 */
    require(folio_command_alias_count(FOLIO_COMMAND_FIND) == 0 &&
                same_text(folio_command_label(FOLIO_COMMAND_FIND), "このノート内を検索"),
            "in-note search is a GUI-only operation with a Japanese label");
    require(folio_command_matches(FOLIO_COMMAND_FIND, "検索", strlen("検索")),
            "the operations menu finds it by label");
}

void run_command_tests(void)
{
    verify_aliases();
    verify_rejections();
    verify_catalog();
    verify_options();
    verify_listed();
}
