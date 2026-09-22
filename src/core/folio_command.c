#include "folio_command.h"

#include "ui_text.h"

#include <string.h>

constexpr size_t alias_limit = 2;

static const struct
{
    enum folio_command command;
    enum ui_text label;
    size_t aliases;
    const char *_Nonnull names[alias_limit];
} catalog[] = {
    [FOLIO_COMMAND_SAVE] = {FOLIO_COMMAND_SAVE, UI_TEXT_COMMAND_SAVE, 2, {"w", "write"}},
    [FOLIO_COMMAND_QUIT] = {FOLIO_COMMAND_QUIT, UI_TEXT_COMMAND_QUIT, 2, {"q", "quit"}},
    [FOLIO_COMMAND_SAVE_QUIT] = {FOLIO_COMMAND_SAVE_QUIT,
                                 UI_TEXT_COMMAND_SAVE_QUIT,
                                 2,
                                 {"wq", "x"}},
    [FOLIO_COMMAND_FORCE_QUIT] = {FOLIO_COMMAND_FORCE_QUIT,
                                  UI_TEXT_COMMAND_FORCE_QUIT,
                                  1,
                                  {"q!", ""}},
    [FOLIO_COMMAND_HELP] = {FOLIO_COMMAND_HELP, UI_TEXT_COMMAND_HELP, 2, {"help", "h"}},
    [FOLIO_COMMAND_EDIT] = {FOLIO_COMMAND_EDIT, UI_TEXT_COMMAND_EDIT, 1, {"startinsert", ""}},
    [FOLIO_COMMAND_VIEW] = {FOLIO_COMMAND_VIEW, UI_TEXT_COMMAND_VIEW, 0, {"", ""}},
    [FOLIO_COMMAND_NEW] = {FOLIO_COMMAND_NEW, UI_TEXT_COMMAND_NEW, 1, {"enew", ""}},
    [FOLIO_COMMAND_SAVE_AS] = {FOLIO_COMMAND_SAVE_AS, UI_TEXT_COMMAND_SAVE_AS, 1, {"saveas", ""}},
    [FOLIO_COMMAND_RENAME] = {FOLIO_COMMAND_RENAME, UI_TEXT_COMMAND_RENAME, 1, {"rename", ""}},
    /* GUI 専用操作（ADR 0018）。`/` `?` `n` `N` と Ctrl+F が同じ ID を実行する（ADR 0023）。 */
    [FOLIO_COMMAND_FIND] = {FOLIO_COMMAND_FIND, UI_TEXT_COMMAND_FIND, 0, {"", ""}},
    /* 語を要る Ex の文法（ADR 0026 の決定 8）。パレットとメニューには出ない。 */
    [FOLIO_COMMAND_SET] = {FOLIO_COMMAND_SET, UI_TEXT_COMMAND_SET, 2, {"set", "se"}},
    [FOLIO_COMMAND_TOGGLE_NUMBER] = {FOLIO_COMMAND_TOGGLE_NUMBER,
                                     UI_TEXT_COMMAND_TOGGLE_NUMBER,
                                     0,
                                     {"", ""}},
    /* GUI 専用操作（ADR 0028 の決定 8(a)）。置換の欄を開くだけで、モードは変えない。 */
    [FOLIO_COMMAND_REPLACE] = {FOLIO_COMMAND_REPLACE, UI_TEXT_COMMAND_REPLACE, 0, {"", ""}},
    /* 区切りで引数を取る Ex の文法（決定 8(b)）。別名の照合ではなく parse の特例で解ける。 */
    [FOLIO_COMMAND_SUBSTITUTE] = {FOLIO_COMMAND_SUBSTITUTE,
                                  UI_TEXT_COMMAND_SUBSTITUTE,
                                  0,
                                  {"", ""}},
};

/* 設定の語（ADR 0026 の決定 8）。効果を実装した語だけを並べる。 */
static const struct
{
    const char *_Nonnull name;
    enum folio_option option;
} options[] = {
    {"number", FOLIO_OPTION_NUMBER_SHOW},      {"nu", FOLIO_OPTION_NUMBER_SHOW},
    {"nonumber", FOLIO_OPTION_NUMBER_HIDE},    {"nonu", FOLIO_OPTION_NUMBER_HIDE},
    {"number!", FOLIO_OPTION_NUMBER_TOGGLE},   {"nu!", FOLIO_OPTION_NUMBER_TOGGLE},
    {"invnumber", FOLIO_OPTION_NUMBER_TOGGLE}, {"invnu", FOLIO_OPTION_NUMBER_TOGGLE},
};

/* 引数の種類（ADR0020 / ADR0022 / ADR 0026）。閉じた集合なので増えたらここで落ちる。 */
static enum folio_argument_kind argument_kind(enum folio_command command)
{
    switch (command)
    {
    case FOLIO_COMMAND_SAVE:
    case FOLIO_COMMAND_SAVE_AS:
    case FOLIO_COMMAND_RENAME:
        return FOLIO_ARGUMENT_NAME;
    case FOLIO_COMMAND_SET:
        return FOLIO_ARGUMENT_OPTION;
    case FOLIO_COMMAND_SUBSTITUTE:
        return FOLIO_ARGUMENT_SUBSTITUTE;
    case FOLIO_COMMAND_QUIT:
    case FOLIO_COMMAND_SAVE_QUIT:
    case FOLIO_COMMAND_FORCE_QUIT:
    case FOLIO_COMMAND_HELP:
    case FOLIO_COMMAND_EDIT:
    case FOLIO_COMMAND_VIEW:
    case FOLIO_COMMAND_NEW:
    case FOLIO_COMMAND_FIND:
    case FOLIO_COMMAND_TOGGLE_NUMBER:
    case FOLIO_COMMAND_REPLACE:
        return FOLIO_ARGUMENT_NONE;
    }
    return FOLIO_ARGUMENT_NONE;
}

bool folio_command_listed(enum folio_command command)
{
    switch (command)
    {
    case FOLIO_COMMAND_SET:
    case FOLIO_COMMAND_SUBSTITUTE:
        return false;
    case FOLIO_COMMAND_SAVE:
    case FOLIO_COMMAND_QUIT:
    case FOLIO_COMMAND_SAVE_QUIT:
    case FOLIO_COMMAND_FORCE_QUIT:
    case FOLIO_COMMAND_HELP:
    case FOLIO_COMMAND_EDIT:
    case FOLIO_COMMAND_VIEW:
    case FOLIO_COMMAND_NEW:
    case FOLIO_COMMAND_SAVE_AS:
    case FOLIO_COMMAND_RENAME:
    case FOLIO_COMMAND_FIND:
    case FOLIO_COMMAND_TOGGLE_NUMBER:
    case FOLIO_COMMAND_REPLACE:
        return true;
    }
    return true;
}

size_t folio_command_listed_count(void)
{
    size_t listed = 0;
    for (size_t item = 0; item < folio_command_count(); ++item)
    {
        if (folio_command_listed(catalog[item].command))
        {
            listed += 1;
        }
    }
    return listed;
}

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

static bool ascii_alnum(char value)
{
    return (value >= '0' && value <= '9') || (value >= 'A' && value <= 'Z') ||
           (value >= 'a' && value <= 'z');
}

/* `%s` で始まり、その直後が英数字でないトークンだけを `:%s` と見る（ADR 0028 の決定 8(b)）。
 * 別名の照合より**前**に判定するが、他の別名の照合規則は何も変えない。
 * `:s`（`%` なし・行の範囲）はこの単位では意図して未対応で、別名に無いので解けない。 */
static bool substitute_token(const char *_Nonnull text, size_t begin, size_t token_end)
{
    if (token_end - begin < 2 || text[begin] != '%' || text[begin + 1] != 's')
    {
        return false;
    }
    return token_end - begin == 2 || !ascii_alnum(text[begin + 2]);
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

const char *_Nonnull folio_command_label(enum folio_command command, enum folio_language language)
{
    return ui_text_line(catalog[command].label, language);
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
    if (substitute_token(text, begin, token_end))
    {
        *out = FOLIO_COMMAND_SUBSTITUTE;
        *argument = begin + 2; /* 区切り文字の位置。分けるのは parse_substitute */
        return true;
    }
    enum folio_command command = FOLIO_COMMAND_SAVE;
    if (!find_alias(text + begin, token_end - begin, &command))
    {
        return false;
    }
    size_t name = skip_spaces(text, token_end, length);
    if (name < length && argument_kind(command) == FOLIO_ARGUMENT_NONE)
    {
        return false;
    }
    *out = command;
    *argument = name;
    return true;
}

bool folio_command_parse_option(const char *_Nonnull text, size_t length,
                                enum folio_option *_Nonnull out)
{
    size_t begin = 0;
    size_t end = 0;
    trim(text, length, &begin, &end);
    for (size_t at = begin; at < end; ++at)
    {
        if (ascii_space(text[at]))
        {
            return false; /* 余計な語。語は 1 つだけ（ADR 0026 の決定 8） */
        }
    }
    for (size_t item = 0; item < sizeof options / sizeof options[0]; ++item)
    {
        if (same_span(text + begin, end - begin, options[item].name))
        {
            *out = options[item].option;
            return true;
        }
    }
    return false;
}

/* エスケープされていない次の区切り。`\` の次の 1 文字は区切りにならない。無ければ length。
 * UTF-8 の後続バイトは最上位ビットが立っているので、多バイト文字を誤って切らない。 */
static size_t next_delimiter(const char *_Nonnull text, size_t at, size_t length)
{
    while (at < length)
    {
        if (text[at] == '\\')
        {
            at += 2;
            continue;
        }
        if (text[at] == '/')
        {
            return at;
        }
        at += 1;
    }
    return length;
}

/* 旗は `g` だけ（ADR 0028 の決定 8(b)）。前後の空白は `:set` と同じ流儀で落とす。 */
static bool substitute_flags(const char *_Nonnull text, size_t length, bool *_Nonnull global)
{
    size_t begin = 0;
    size_t end = 0;
    trim(text, length, &begin, &end);
    if (begin == end)
    {
        *global = false;
        return true;
    }
    if (end - begin != 1 || text[begin] != 'g')
    {
        return false;
    }
    *global = true;
    return true;
}

bool folio_command_parse_substitute(const char *_Nonnull text, size_t length,
                                    struct ex_substitute *_Nonnull out)
{
    if (length == 0 || text[0] != '/')
    {
        return false;
    }
    size_t first = next_delimiter(text, 1, length);
    if (first == length || first == 1)
    {
        return false; /* 区切りが足りない、または空のパターン */
    }
    size_t second = next_delimiter(text, first + 1, length);
    if (second == length)
    {
        return false;
    }
    bool global = false;
    if (!substitute_flags(text + second + 1, length - second - 1, &global))
    {
        return false;
    }
    out->pattern = 1;
    out->pattern_length = first - 1;
    out->replacement = first + 1;
    out->replacement_length = second - first - 1;
    out->global = global;
    return true;
}

bool folio_command_matches(enum folio_command command, const char *_Nonnull text, size_t length,
                           enum folio_language language)
{
    size_t item = command;
    const char *_Nonnull label = folio_command_label(command, language);
    if (contains_span(label, strlen(label), text, length))
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
