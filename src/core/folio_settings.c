#include "folio_settings.h"

#include "json_reader.h"
#include "json_writer.h"

#include <stdlib.h>
#include <string.h>

struct folio_settings
{
    bool number;
};

constexpr uint32_t settings_version = 1;

/* 決めた値を 1 つ持つ設定を作る。既定値と複製が共有する唯一の確保点（ARC-001）。 */
static enum folio_settings_outcome create(bool number,
                                          struct folio_settings *_Nullable *_Nonnull out)
{
    struct folio_settings *_Nullable settings = calloc(1, sizeof *settings);
    if (settings == nullptr)
    {
        return FOLIO_SETTINGS_OUT_OF_MEMORY;
    }
    settings->number = number;
    *out = settings;
    return FOLIO_SETTINGS_READY;
}

enum folio_settings_outcome folio_settings_default(struct folio_settings *_Nullable *_Nonnull out)
{
    return create(false, out);
}

enum folio_settings_outcome
folio_settings_with_number(const struct folio_settings *_Nonnull settings, bool number,
                           struct folio_settings *_Nullable *_Nonnull out)
{
    struct folio_settings *_Nullable copy = nullptr;
    enum folio_settings_outcome outcome = create(settings->number, &copy);
    if (outcome != FOLIO_SETTINGS_READY)
    {
        return outcome;
    }
    copy->number = number;
    *out = copy;
    return FOLIO_SETTINGS_READY;
}

static bool expect_key(struct json_reader *_Nonnull reader, const char *_Nonnull key)
{
    return json_reader_next(reader) == JSON_TOKEN_KEY && strcmp(json_reader_text(reader), key) == 0;
}

/* {"version": 1, "number": <bool>} を頭から順に読む。順序も含めて版 1 の形である。 */
static enum folio_settings_outcome parse_document(struct json_reader *_Nonnull reader,
                                                  bool *_Nonnull number)
{
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_BEGIN || !expect_key(reader, "version") ||
        json_reader_next(reader) != JSON_TOKEN_UNSIGNED)
    {
        return FOLIO_SETTINGS_MALFORMED;
    }
    if (json_reader_unsigned(reader) != settings_version)
    {
        return FOLIO_SETTINGS_UNSUPPORTED_VERSION;
    }
    if (!expect_key(reader, "number"))
    {
        return FOLIO_SETTINGS_MALFORMED;
    }
    enum json_token flag = json_reader_next(reader);
    if (flag != JSON_TOKEN_TRUE && flag != JSON_TOKEN_FALSE)
    {
        return FOLIO_SETTINGS_MALFORMED;
    }
    *number = flag == JSON_TOKEN_TRUE;
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_END ||
        json_reader_next(reader) != JSON_TOKEN_END)
    {
        return FOLIO_SETTINGS_MALFORMED;
    }
    return FOLIO_SETTINGS_READY;
}

enum folio_settings_outcome folio_settings_parse(const char *_Nonnull text, size_t length,
                                                 struct folio_settings *_Nullable *_Nonnull out)
{
    struct json_reader *_Nullable reader = nullptr;
    if (json_reader_create(text, length, &reader) != JSON_READER_CREATED)
    {
        return FOLIO_SETTINGS_OUT_OF_MEMORY;
    }
    bool number = false;
    enum folio_settings_outcome outcome = parse_document(reader, &number);
    json_reader_destroy(reader);
    if (outcome != FOLIO_SETTINGS_READY)
    {
        return outcome;
    }
    return create(number, out);
}

void folio_settings_write(const struct folio_settings *_Nonnull settings,
                          struct json_writer *_Nonnull writer)
{
    json_writer_object_begin(writer);
    json_writer_key(writer, "version");
    json_writer_unsigned(writer, settings_version);
    json_writer_key(writer, "number");
    json_writer_boolean(writer, settings->number);
    json_writer_object_end(writer);
}

bool folio_settings_number(const struct folio_settings *_Nonnull settings)
{
    return settings->number;
}

void folio_settings_destroy(struct folio_settings *_Nullable settings)
{
    free(settings);
}
