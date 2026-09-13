#include "folio_command.h"

#include <string.h>

constexpr size_t alias_limit = 2;

static const struct
{
    enum folio_command command;
    const char *_Nonnull label;
    size_t aliases;
    const char *_Nonnull names[alias_limit];
} catalog[] = {
    [FOLIO_COMMAND_SAVE] = {FOLIO_COMMAND_SAVE, "保存", 2, {"w", "write"}},
    [FOLIO_COMMAND_QUIT] = {FOLIO_COMMAND_QUIT, "保存済みなら終了", 2, {"q", "quit"}},
    [FOLIO_COMMAND_SAVE_QUIT] = {FOLIO_COMMAND_SAVE_QUIT, "保存して終了", 2, {"wq", "x"}},
    [FOLIO_COMMAND_FORCE_QUIT] = {FOLIO_COMMAND_FORCE_QUIT,
                                  "未保存変更を破棄して終了",
                                  1,
                                  {"q!", ""}},
    [FOLIO_COMMAND_HELP] = {FOLIO_COMMAND_HELP, "ヘルプ", 2, {"help", "h"}},
    [FOLIO_COMMAND_EDIT] = {FOLIO_COMMAND_EDIT, "編集", 1, {"startinsert", ""}},
    [FOLIO_COMMAND_VIEW] = {FOLIO_COMMAND_VIEW, "保存して閲覧", 0, {"", ""}},
    [FOLIO_COMMAND_NEW] = {FOLIO_COMMAND_NEW, "新しいノート", 1, {"enew", ""}},
};

static bool ascii_space(char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n' || value == '\f' ||
           value == '\v';
}

static void trim(const char *_Nonnull text, size_t length, size_t *_Nonnull begin,
                 size_t *_Nonnull end)
{
    *begin = 0;
    while (*begin < length && ascii_space(text[*begin]))
    {
        *begin += 1;
    }
    *end = length;
    while (*end > *begin && ascii_space(text[*end - 1]))
    {
        *end -= 1;
    }
}

static bool same_span(const char *_Nonnull text, size_t length, const char *_Nonnull expected)
{
    size_t expected_length = strlen(expected);
    return length == expected_length && memcmp(text, expected, length) == 0;
}

static size_t skip_spaces(const char *_Nonnull text, size_t begin, size_t end)
{
    while (begin < end && ascii_space(text[begin]))
    {
        begin += 1;
    }
    return begin;
}

static bool item_has_alias(size_t item, const char *_Nonnull text, size_t length)
{
    for (size_t alias = 0; alias < catalog[item].aliases; ++alias)
    {
        if (same_span(text, length, catalog[item].names[alias]))
        {
            return true;
        }
    }
    return false;
}

static bool find_alias(const char *_Nonnull text, size_t length, enum folio_command *_Nonnull out)
{
    for (size_t item = 0; item < folio_command_count(); ++item)
    {
        if (item_has_alias(item, text, length))
        {
            *out = catalog[item].command;
            return true;
        }
    }
    return false;
}

static bool contains_span(const char *_Nonnull text, size_t length, const char *_Nonnull query,
                          size_t query_length)
{
    if (query_length == 0)
    {
        return true;
    }
    if (query_length > length)
    {
        return false;
    }
    for (size_t index = 0; index <= length - query_length; ++index)
    {
        if (memcmp(text + index, query, query_length) == 0)
        {
            return true;
        }
    }
    return false;
}

size_t folio_command_count(void)
{
    return sizeof catalog / sizeof catalog[0];
}

enum folio_command folio_command_at(size_t index)
{
    return catalog[index].command;
}

const char *_Nonnull folio_command_label(enum folio_command command)
{
    return catalog[command].label;
}

size_t folio_command_alias_count(enum folio_command command)
{
    return catalog[command].aliases;
}

const char *_Nonnull folio_command_alias(enum folio_command command, size_t index)
{
    return catalog[command].names[index];
}

bool folio_command_parse(const char *_Nonnull text, size_t length, enum folio_command *_Nonnull out,
                         size_t *_Nonnull argument)
{
    size_t begin = 0;
    size_t end = 0;
    trim(text, length, &begin, &end);
    if (begin < end && text[begin] == ':')
    {
        begin = skip_spaces(text, begin + 1, end);
    }
    size_t token_end = begin;
    while (token_end < end && !ascii_space(text[token_end]))
    {
        token_end += 1;
    }
    enum folio_command command = FOLIO_COMMAND_SAVE;
    if (!find_alias(text + begin, token_end - begin, &command))
    {
        return false;
    }
    size_t name = skip_spaces(text, token_end, length);
    if (name < length && command != FOLIO_COMMAND_SAVE)
    {
        return false;
    }
    *out = command;
    *argument = name;
    return true;
}

bool folio_command_matches(enum folio_command command, const char *_Nonnull text, size_t length)
{
    size_t item = command;
    if (contains_span(catalog[item].label, strlen(catalog[item].label), text, length))
    {
        return true;
    }
    for (size_t alias = 0; alias < catalog[item].aliases; ++alias)
    {
        const char *_Nonnull name = catalog[item].names[alias];
        if (contains_span(name, strlen(name), text, length))
        {
            return true;
        }
    }
    return false;
}
