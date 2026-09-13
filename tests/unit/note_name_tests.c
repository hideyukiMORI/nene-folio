#include "folio_command.h"
#include "note_name.h"
#include "unit_tests.h"
#include <string.h>

static void verify_names(void)
{
    const char *_Nonnull const rejected[] = {
        "",     ".md",  "..",   "CON",      "con.md", "NUL.foo.md", "PRN",  "aux.MD",
        "COM1", "lpt9", "COM¹", "lpt².txt", "LPT³",   "foo ",       "foo.", "foo .md",
        "a/b",  "a\\b", "a:b",  "a?b",      "a\nb",   "\xFF"};
    for (size_t index = 0; index < sizeof rejected / sizeof rejected[0]; ++index)
    {
        struct note_name *name = nullptr;
        require(note_name_create(rejected[index], strlen(rejected[index]), &name) ==
                    NOTE_NAME_INVALID,
                rejected[index]);
        require(name == nullptr, "invalid names do not publish an object");
    }
    const char *_Nonnull const accepted[] = {"日本語 空白.MD", "COM10",   "console",
                                             ".hidden",        "a.md.md", " 日報"};
    const char *_Nonnull const stems[] = {"日本語 空白", "COM10", "console",
                                          ".hidden",     "a.md",  " 日報"};
    for (size_t index = 0; index < sizeof accepted / sizeof accepted[0]; ++index)
    {
        struct note_name *name = nullptr;
        require(note_name_create(accepted[index], strlen(accepted[index]), &name) ==
                    NOTE_NAME_ACCEPTED,
                "valid note name");
        require(same_text(note_name_stem(name), stems[index]), "one extension is removed");
        note_name_destroy(name);
    }
    char long_name[256];
    memset(long_name, 'a', sizeof long_name);
    struct note_name *name = nullptr;
    require(note_name_create(long_name, 253, &name) == NOTE_NAME_INVALID, "full filename limit");
    require(note_name_create(long_name, 252, &name) == NOTE_NAME_ACCEPTED,
            "252 byte stem plus .md fits");
    note_name_destroy(name);
    note_name_destroy(nullptr);
}

static void verify_arguments(void)
{
    const char *_Nonnull const commands[] = {":w 日報 名前.md", "write 名 前 ", " : w foo", "w   ",
                                             ":enew"};
    const char *_Nonnull const arguments[] = {"日報 名前.md", "名 前 ", "foo", "", ""};
    for (size_t index = 0; index < sizeof commands / sizeof commands[0]; ++index)
    {
        enum folio_command command = FOLIO_COMMAND_HELP;
        size_t start = 0;
        require(folio_command_parse(commands[index], strlen(commands[index]), &command, &start),
                "registered command with optional save name");
        require(same_text(commands[index] + start, arguments[index]), "argument bytes stay intact");
    }
}

void run_note_name_tests(void)
{
    verify_names();
    verify_arguments();
}
