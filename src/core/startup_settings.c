#include "startup_settings.h"
#include "json_reader.h"
#include "json_writer.h"
#include "storage_path.h"
#include <stdlib.h>
#include <string.h>

struct startup_settings
{
    struct storage_path *_Nullable directory;
};

enum startup_settings_outcome
startup_settings_create(const struct storage_path *_Nullable directory,
                        struct startup_settings *_Nullable *_Nonnull out)
{
    struct startup_settings *_Nullable settings = calloc(1, sizeof *settings);
    if (settings == nullptr)
        return STARTUP_SETTINGS_OUT_OF_MEMORY;
    if (directory != nullptr &&
        storage_path_copy(directory, &settings->directory) != STORAGE_PATH_READY)
    {
        startup_settings_destroy(settings);
        return STARTUP_SETTINGS_OUT_OF_MEMORY;
    }
    *out = settings;
    return STARTUP_SETTINGS_READY;
}

static bool key(struct json_reader *_Nonnull reader, const char *_Nonnull name)
{
    return json_reader_next(reader) == JSON_TOKEN_KEY &&
           strcmp(json_reader_text(reader), name) == 0;
}

static enum startup_settings_outcome read_directory(struct json_reader *_Nonnull reader,
                                                    struct storage_path *_Nullable *_Nonnull out)
{
    if (!key(reader, "directory") || json_reader_next(reader) != JSON_TOKEN_STRING)
        return STARTUP_SETTINGS_MALFORMED;
    size_t length = json_reader_text_length(reader);
    if (length == 0)
        return STARTUP_SETTINGS_READY;
    switch (storage_path_create(json_reader_text(reader), length, out))
    {
    case STORAGE_PATH_READY:
        return STARTUP_SETTINGS_READY;
    case STORAGE_PATH_INVALID:
        return STARTUP_SETTINGS_MALFORMED;
    case STORAGE_PATH_OUT_OF_MEMORY:
        return STARTUP_SETTINGS_OUT_OF_MEMORY;
    }
    return STARTUP_SETTINGS_MALFORMED;
}

static enum startup_settings_outcome read_document(struct json_reader *_Nonnull reader,
                                                   struct storage_path *_Nullable *_Nonnull out)
{
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_BEGIN || !key(reader, "version") ||
        json_reader_next(reader) != JSON_TOKEN_UNSIGNED)
        return STARTUP_SETTINGS_MALFORMED;
    if (json_reader_unsigned(reader) != 1)
        return STARTUP_SETTINGS_UNSUPPORTED_VERSION;
    enum startup_settings_outcome result = read_directory(reader, out);
    if (result != STARTUP_SETTINGS_READY)
        return result;
    return json_reader_next(reader) == JSON_TOKEN_OBJECT_END &&
                   json_reader_next(reader) == JSON_TOKEN_END
               ? STARTUP_SETTINGS_READY
               : STARTUP_SETTINGS_MALFORMED;
}

enum startup_settings_outcome
startup_settings_parse(const char *_Nonnull text, size_t length,
                       struct startup_settings *_Nullable *_Nonnull out)
{
    struct json_reader *_Nullable reader = nullptr;
    if (json_reader_create(text, length, &reader) != JSON_READER_CREATED)
        return STARTUP_SETTINGS_OUT_OF_MEMORY;
    struct storage_path *_Nullable path = nullptr;
    enum startup_settings_outcome result = read_document(reader, &path);
    if (result == STARTUP_SETTINGS_MALFORMED &&
        json_reader_next(reader) == JSON_TOKEN_OUT_OF_MEMORY)
        result = STARTUP_SETTINGS_OUT_OF_MEMORY;
    json_reader_destroy(reader);
    if (result == STARTUP_SETTINGS_READY)
        result = startup_settings_create(path, out);
    storage_path_destroy(path);
    return result;
}

void startup_settings_write(const struct startup_settings *_Nonnull settings,
                            struct json_writer *_Nonnull writer)
{
    json_writer_object_begin(writer);
    json_writer_key(writer, "version");
    json_writer_unsigned(writer, 1);
    json_writer_key(writer, "directory");
    json_writer_string(
        writer, settings->directory == nullptr ? "" : storage_path_text(settings->directory));
    json_writer_object_end(writer);
}

const struct storage_path *_Nullable startup_settings_directory(
    const struct startup_settings *_Nonnull settings)
{
    return settings->directory;
}

bool startup_settings_equal(const struct startup_settings *_Nonnull left,
                            const struct startup_settings *_Nonnull right)
{
    if (left->directory == nullptr || right->directory == nullptr)
        return left->directory == right->directory;
    return storage_path_equal(left->directory, right->directory);
}

void startup_settings_destroy(struct startup_settings *_Nullable settings)
{
    if (settings != nullptr)
        storage_path_destroy(settings->directory);
    free(settings);
}
