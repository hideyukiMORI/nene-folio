#include "ascii_fold.h"
#include "folio_command.h"
#include "folio_language.h"
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
    expect_command(":%s/a/b/", FOLIO_COMMAND_SUBSTITUTE, ":%s is read before the alias table");
    expect_command("%s/a/b/g", FOLIO_COMMAND_SUBSTITUTE, "the leading colon is optional");
    expect_command(":%s/a b/c d/g", FOLIO_COMMAND_SUBSTITUTE, "spaces inside the arguments");
    expect_command(":%s", FOLIO_COMMAND_SUBSTITUTE,
                   ":%s alone parses; the delimiters are refused later");
}

/* `:%s/パターン/置換/[g]` の分解（ADR 0028 の決定 8(b)）。 */
static void expect_substitute(const char *_Nonnull line, const char *_Nonnull pattern,
                              const char *_Nonnull replacement, bool global)
{
    enum folio_command command = FOLIO_COMMAND_HELP;
    size_t argument = 0;
    require(folio_command_parse(line, strlen(line), &command, &argument) &&
                command == FOLIO_COMMAND_SUBSTITUTE,
            "the substitute grammar parses");
    struct ex_substitute parts = {.pattern = 0,
                                  .pattern_length = 0,
                                  .replacement = 0,
                                  .replacement_length = 0,
                                  .global = false};
    const char *_Nonnull tail = line + argument;
    require(folio_command_parse_substitute(tail, strlen(line) - argument, &parts),
            "the delimiters split");
    require(parts.pattern_length == strlen(pattern) &&
                memcmp(tail + parts.pattern, pattern, parts.pattern_length) == 0,
            "the pattern is the first field");
    require(parts.replacement_length == strlen(replacement) &&
                memcmp(tail + parts.replacement, replacement, parts.replacement_length) == 0,
            "the replacement is the second field");
    require(parts.global == global, "the g flag decides whether every match is replaced");
}

static void verify_substitute(void)
{
    expect_substitute(":%s/a/b/", "a", "b", false);
    expect_substitute(":%s/a/b/g", "a", "b", true);
    expect_substitute(":%s/a/b/g  ", "a", "b", true);
    expect_substitute(":%s/a/b/ ", "a", "b", false);
    expect_substitute(":%s/a//", "a", "", false);
    /* パターンの `\/` はそのまま渡す（ICU で `/`）。置換の `\/` は replace_template が解く。 */
    expect_substitute(":%s/a\\/b/c\\/d/g", "a\\/b", "c\\/d", true);
    expect_substitute(":%s/\\\\/x/", "\\\\", "x", false);
    expect_substitute(":%s/日本語/にほんご/g", "日本語", "にほんご", true);
    expect_substitute(":%s/ a / b /g", " a ", " b ", true);
    /* 区切りの不足・空のパターン・知らない旗は、未知のコマンドと同じ 1 行になる（決定 8(b)）。 */
    static const char *const refused[] = {"",        "/a/b",   "/a/b/x", "//b/",
                                          "/a/b/gg", "a/b/c/", "/a\\/b", "/"};
    for (size_t index = 0; index < sizeof refused / sizeof refused[0]; ++index)
    {
        struct ex_substitute parts = {.pattern = 1,
                                      .pattern_length = 1,
                                      .replacement = 1,
                                      .replacement_length = 1,
                                      .global = true};
        require(!folio_command_parse_substitute(refused[index], strlen(refused[index]), &parts),
                "an ill formed substitute is refused");
        require(parts.pattern == 1 && parts.global, "a refused line leaves the result alone");
    }
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
    /* テーマの 3 語（ADR 0031 の決定 8(c)）。完全一致の表なので parser は変わっていない。 */
    expect_option(":set theme=system", FOLIO_OPTION_THEME_SYSTEM, "theme=system follows the OS");
    expect_option(":se theme=light", FOLIO_OPTION_THEME_LIGHT, "theme=light is explicit");
    expect_option(":set theme=dark ", FOLIO_OPTION_THEME_DARK, "theme=dark is trimmed");
    static const char *const refused[] = {
        "",        " ",         "numbers", "NUMBER", "no",         "number no",   "nu nu",
        "!number", "invisible", "theme",   "theme=", "theme=blue", "theme = dark"};
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
    require(!folio_command_listed(FOLIO_COMMAND_SET) &&
                !folio_command_listed(FOLIO_COMMAND_SUBSTITUTE),
            "the Ex grammars that need an argument are not listed");
    for (size_t index = 0; index < folio_command_count(); ++index)
    {
        enum folio_command command = folio_command_at(index);
        require(folio_command_listed(command) ==
                    (command != FOLIO_COMMAND_SET && command != FOLIO_COMMAND_SUBSTITUTE),
                "every other operation stays listed");
    }
    require(folio_command_alias_count(FOLIO_COMMAND_TOGGLE_NUMBER) == 0 &&
                same_text(folio_command_label(FOLIO_COMMAND_TOGGLE_NUMBER, FOLIO_LANGUAGE_JA),
                          "行番号の表示を切り替える"),
            "the toggle is a listed operation with a Japanese label and no alias");
    require(folio_command_matches(FOLIO_COMMAND_TOGGLE_NUMBER, "行番号", strlen("行番号"),
                                  FOLIO_LANGUAGE_JA),
            "the palette finds the toggle by label");
    /* パレットの箱の高さはこの数で決まる。総数（16）で取ると 2 行ぶん余る（補正 9）。 */
    require(folio_command_listed_count() == 14, "fourteen operations are offered on a surface");
    require(folio_command_listed_count() == folio_command_count() - 2,
            "exactly the two unlisted Ex grammars are left out");
    require(folio_command_alias_count(FOLIO_COMMAND_REPLACE) == 0 &&
                same_text(folio_command_label(FOLIO_COMMAND_REPLACE, FOLIO_LANGUAGE_JA), "置換"),
            "replace is a listed GUI operation with a Japanese label and no alias");
    require(folio_command_matches(FOLIO_COMMAND_REPLACE, "置換", strlen("置換"), FOLIO_LANGUAGE_JA),
            "the palette finds replace by label");
    require(folio_command_alias_count(FOLIO_COMMAND_SETTINGS) == 0 &&
                same_text(folio_command_label(FOLIO_COMMAND_SETTINGS, FOLIO_LANGUAGE_JA), "設定"),
            "settings is a listed GUI operation with a Japanese label and no alias");
    require(
        folio_command_matches(FOLIO_COMMAND_SETTINGS, "設定", strlen("設定"), FOLIO_LANGUAGE_JA),
        "the palette finds settings by label");
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
    require(folio_command_count() == 16, "only implemented commands are registered");
    const enum folio_command expected[] = {
        FOLIO_COMMAND_SAVE,          FOLIO_COMMAND_QUIT,    FOLIO_COMMAND_SAVE_QUIT,
        FOLIO_COMMAND_FORCE_QUIT,    FOLIO_COMMAND_HELP,    FOLIO_COMMAND_EDIT,
        FOLIO_COMMAND_VIEW,          FOLIO_COMMAND_NEW,     FOLIO_COMMAND_SAVE_AS,
        FOLIO_COMMAND_RENAME,        FOLIO_COMMAND_FIND,    FOLIO_COMMAND_SET,
        FOLIO_COMMAND_TOGGLE_NUMBER, FOLIO_COMMAND_REPLACE, FOLIO_COMMAND_SUBSTITUTE,
        FOLIO_COMMAND_SETTINGS};
    for (size_t index = 0; index < folio_command_count(); ++index)
    {
        enum folio_command command = folio_command_at(index);
        require(command == expected[index], "catalog order is stable");
        require(strlen(folio_command_label(command, FOLIO_LANGUAGE_JA)) > 0,
                "palette label comes from catalog");
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
    require(folio_command_matches(FOLIO_COMMAND_SAVE, "", 0, FOLIO_LANGUAGE_JA),
            "empty palette query shows all");
    require(folio_command_matches(FOLIO_COMMAND_SAVE_QUIT, "wq", 2, FOLIO_LANGUAGE_JA),
            "palette finds an alias");
    require(
        folio_command_matches(FOLIO_COMMAND_FORCE_QUIT, "破棄", strlen("破棄"), FOLIO_LANGUAGE_JA),
        "palette finds a display label");
    require(!folio_command_matches(FOLIO_COMMAND_HELP, "quit", 4, FOLIO_LANGUAGE_JA),
            "palette excludes a mismatch");
    require(folio_command_alias_count(FOLIO_COMMAND_VIEW) == 0,
            "GUI view has no misleading Vim alias");
    require(folio_command_matches(FOLIO_COMMAND_VIEW, "閲覧", strlen("閲覧"), FOLIO_LANGUAGE_JA),
            "GUI-only command can be found by Japanese label");
    require(folio_command_matches(FOLIO_COMMAND_RENAME, "名前", strlen("名前"), FOLIO_LANGUAGE_JA),
            "rename is found by its Japanese label");
    /* `?` はヘルプではなく後方検索になった（ADR 0023 の決定 6）。ヘルプは F1 / :h / Ctrl+P。 */
    require(folio_command_alias_count(FOLIO_COMMAND_FIND) == 0 &&
                same_text(folio_command_label(FOLIO_COMMAND_FIND, FOLIO_LANGUAGE_JA),
                          "このノート内を検索"),
            "in-note search is a GUI-only operation with a Japanese label");
    require(folio_command_matches(FOLIO_COMMAND_FIND, "検索", strlen("検索"), FOLIO_LANGUAGE_JA),
            "the operations menu finds it by label");
}

/* ASCII の英字だけ大小を無視する（ADR 0032 の決定 8）。3 か所が引く唯一の規則である。 */
static void verify_ascii_fold(void)
{
    require(ascii_fold_byte('A') == 'a' && ascii_fold_byte('Z') == 'z', "upper folds to lower");
    require(ascii_fold_byte('a') == 'a' && ascii_fold_byte('0') == '0' &&
                ascii_fold_byte('@') == '@' && ascii_fold_byte('[') == '[',
            "the neighbours of the letters are untouched");
    require(ascii_fold_byte((char)0xE6) == (char)0xE6,
            "a byte of a multi-byte sequence is untouched");
    require(ascii_fold_unit(u'A') == u'a' && ascii_fold_unit(u'z') == u'z' &&
                ascii_fold_unit(u'0') == u'0',
            "the UTF-16 unit folds the same way");
    require(ascii_fold_unit(0x00C0) == 0x00C0, "a non-ASCII unit is untouched");
}

/* パレットの部分一致は ASCII の英字だけ大小を無視する。Ex の完全一致は区別したまま
 * （ADR 0016 の 2026-09-23 の補正 3）。 */
static void verify_case_insensitive_palette(void)
{
    require(
        folio_command_matches(FOLIO_COMMAND_SAVE_AS, "SAVEAS", strlen("SAVEAS"), FOLIO_LANGUAGE_JA),
        "an alias matches whatever the case");
    require(folio_command_matches(FOLIO_COMMAND_SAVE, "save", strlen("save"), FOLIO_LANGUAGE_EN),
            "the English label matches a lower case query");
    require(folio_command_matches(FOLIO_COMMAND_SAVE, "SAVE", strlen("SAVE"), FOLIO_LANGUAGE_EN),
            "and an upper case query");
    require(!folio_command_matches(FOLIO_COMMAND_SAVE, "zz", strlen("zz"), FOLIO_LANGUAGE_EN),
            "a mismatch is still a mismatch");
    /* 完全一致は区別する: `W` は別名ではない（verify_rejections も同じ行を持つ）。 */
    enum folio_command command = FOLIO_COMMAND_HELP;
    size_t argument = 0;
    require(!folio_command_parse("W", 1, &command, &argument),
            "the Ex parser still tells the case apart");
}

void run_command_tests(void)
{
    verify_aliases();
    verify_rejections();
    verify_catalog();
    verify_options();
    verify_substitute();
    verify_listed();
    verify_ascii_fold();
    verify_case_insensitive_palette();
}
