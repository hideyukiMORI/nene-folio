#include "folio_settings.h"

#include "json_reader.h"
#include "json_writer.h"

#include <stdlib.h>
#include <string.h>

struct folio_settings
{
    bool number;
    enum folio_theme_choice theme;
};

constexpr uint32_t settings_version = 2;
constexpr uint32_t settings_version_1 = 1;

/* 版 2 の theme の語（ADR 0031 の決定 1）。読みと書きが引く唯一の表で、
 * 網羅は CNF-009（eng/conformance-rules.json の lineTables）が守る。表の外の語は MALFORMED。 */
static const char *_Nonnull const theme_names[] = {
    [FOLIO_THEME_CHOICE_SYSTEM] = "system",
    [FOLIO_THEME_CHOICE_LIGHT] = "light",
    [FOLIO_THEME_CHOICE_DARK] = "dark",
};

/* 決めた値を持つ設定を作る。既定値と複製が共有する唯一の確保点（ARC-001）。
 * **値は struct のまま受ける**ので、キーが増えても引数は 2 つのままである（ADR 0032 の決定 1）。 */
static enum folio_settings_outcome create(const struct folio_settings *_Nonnull values,
                                          struct folio_settings *_Nullable *_Nonnull out)
{
    struct folio_settings *_Nullable settings = calloc(1, sizeof *settings);
    if (settings == nullptr)
    {
        return FOLIO_SETTINGS_OUT_OF_MEMORY;
    }
    *settings = *values;
    *out = settings;
    return FOLIO_SETTINGS_READY;
}

/* ファイルが無いときと、解析を始めるときの初期値。既定値を書く場所はここだけである。 */
static struct folio_settings defaults(void)
{
    struct folio_settings values = {.number = false, .theme = FOLIO_THEME_CHOICE_SYSTEM};
    return values;
}

enum folio_settings_outcome folio_settings_default(struct folio_settings *_Nullable *_Nonnull out)
{
    struct folio_settings values = defaults();
    return create(&values, out);
}

enum folio_settings_outcome
folio_settings_with_number(const struct folio_settings *_Nonnull settings, bool number,
                           struct folio_settings *_Nullable *_Nonnull out)
{
    /* 元の設定を写してから number だけを変える。写す値が増えても写す場所はここだけである。 */
    struct folio_settings values = *settings;
    values.number = number;
    return create(&values, out);
}

enum folio_settings_outcome
folio_settings_with_theme(const struct folio_settings *_Nonnull settings,
                          enum folio_theme_choice theme,
                          struct folio_settings *_Nullable *_Nonnull out)
{
    struct folio_settings values = *settings;
    values.theme = theme;
    return create(&values, out);
}

static bool expect_key(struct json_reader *_Nonnull reader, const char *_Nonnull key)
{
    return json_reader_next(reader) == JSON_TOKEN_KEY && strcmp(json_reader_text(reader), key) == 0;
}

static bool read_boolean(struct json_reader *_Nonnull reader, bool *_Nonnull value)
{
    enum json_token flag = json_reader_next(reader);
    if (flag != JSON_TOKEN_TRUE && flag != JSON_TOKEN_FALSE)
    {
        return false;
    }
    *value = flag == JSON_TOKEN_TRUE;
    return true;
}

static bool read_theme(struct json_reader *_Nonnull reader, enum folio_theme_choice *_Nonnull value)
{
    if (json_reader_next(reader) != JSON_TOKEN_STRING)
    {
        return false;
    }
    const char *_Nonnull text = json_reader_text(reader);
    for (size_t item = 0; item < sizeof theme_names / sizeof theme_names[0]; ++item)
    {
        if (strcmp(text, theme_names[item]) == 0)
        {
            *value = (enum folio_theme_choice)item;
            return true;
        }
    }
    return false;
}

static bool document_end(struct json_reader *_Nonnull reader)
{
    return json_reader_next(reader) == JSON_TOKEN_OBJECT_END &&
           json_reader_next(reader) == JSON_TOKEN_END;
}

/* 版 1 の本体（`"number": <bool>` まで）。theme は既定値のまま版 2 へ移す（決定 1）。 */
static enum folio_settings_outcome parse_version_1(struct json_reader *_Nonnull reader,
                                                   struct folio_settings *_Nonnull values)
{
    if (!expect_key(reader, "number") || !read_boolean(reader, &values->number) ||
        !document_end(reader))
    {
        return FOLIO_SETTINGS_MALFORMED;
    }
    return FOLIO_SETTINGS_READY;
}

/* 版 2 の本体（`"number": <bool>, "theme": "…"` まで）。順序も形のうち。 */
static enum folio_settings_outcome parse_version_2(struct json_reader *_Nonnull reader,
                                                   struct folio_settings *_Nonnull values)
{
    if (!expect_key(reader, "number") || !read_boolean(reader, &values->number) ||
        !expect_key(reader, "theme") || !read_theme(reader, &values->theme) ||
        !document_end(reader))
    {
        return FOLIO_SETTINGS_MALFORMED;
    }
    return FOLIO_SETTINGS_READY;
}

/* 先頭の `{"version": <n>` だけを読み、その版の本体の解析へ渡す（決定 1）。 */
static enum folio_settings_outcome parse_document(struct json_reader *_Nonnull reader,
                                                  struct folio_settings *_Nonnull values)
{
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_BEGIN || !expect_key(reader, "version") ||
        json_reader_next(reader) != JSON_TOKEN_UNSIGNED)
    {
        return FOLIO_SETTINGS_MALFORMED;
    }
    uint32_t version = json_reader_unsigned(reader);
    if (version == settings_version_1)
    {
        return parse_version_1(reader, values);
    }
    if (version == settings_version)
    {
        return parse_version_2(reader, values);
    }
    return FOLIO_SETTINGS_UNSUPPORTED_VERSION;
}

enum folio_settings_outcome folio_settings_parse(const char *_Nonnull text, size_t length,
                                                 struct folio_settings *_Nullable *_Nonnull out)
{
    struct json_reader *_Nullable reader = nullptr;
    if (json_reader_create(text, length, &reader) != JSON_READER_CREATED)
    {
        return FOLIO_SETTINGS_OUT_OF_MEMORY;
    }
    struct folio_settings values = defaults();
    enum folio_settings_outcome outcome = parse_document(reader, &values);
    json_reader_destroy(reader);
    if (outcome != FOLIO_SETTINGS_READY)
    {
        return outcome;
    }
    return create(&values, out);
}

void folio_settings_write(const struct folio_settings *_Nonnull settings,
                          struct json_writer *_Nonnull writer)
{
    json_writer_object_begin(writer);
    json_writer_key(writer, "version");
    json_writer_unsigned(writer, settings_version);
    json_writer_key(writer, "number");
    json_writer_boolean(writer, settings->number);
    json_writer_key(writer, "theme");
    json_writer_string(writer, theme_names[settings->theme]);
    json_writer_object_end(writer);
}

bool folio_settings_number(const struct folio_settings *_Nonnull settings)
{
    return settings->number;
}

enum folio_theme_choice folio_settings_theme(const struct folio_settings *_Nonnull settings)
{
    return settings->theme;
}

void folio_settings_destroy(struct folio_settings *_Nullable settings)
{
    free(settings);
}
