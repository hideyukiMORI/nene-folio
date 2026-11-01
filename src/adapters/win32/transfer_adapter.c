#include "transfer_adapter.h"
#include "file_bytes.h"
#include "note_archive.h"
#include "note_text.h"
#include "storage_native_path.h"

static enum persistence_outcome read_note(const struct storage_path *_Nonnull path,
                                          struct note_text *_Nullable *_Nonnull out)
{
    wchar_t native[storage_native_capacity];
    size_t length = 0;
    if (!storage_native_path_fill(path, native, &length))
        return PERSISTENCE_UNREADABLE;
    struct file_bytes *_Nullable bytes = nullptr;
    enum persistence_outcome read = file_bytes_read(native, &bytes);
    if (read != PERSISTENCE_LOADED)
        return read;
    enum note_text_outcome accepted =
        note_text_create(file_bytes_data(bytes), file_bytes_length(bytes), out);
    file_bytes_destroy(bytes);
    switch (accepted)
    {
    case NOTE_TEXT_ACCEPTED:
        return PERSISTENCE_LOADED;
    case NOTE_TEXT_INVALID_UTF8:
    case NOTE_TEXT_EMBEDDED_NUL:
    case NOTE_TEXT_TOO_LARGE:
        return PERSISTENCE_MALFORMED;
    case NOTE_TEXT_OUT_OF_MEMORY:
        return PERSISTENCE_OUT_OF_MEMORY;
    }
    return PERSISTENCE_MALFORMED;
}

static enum persistence_outcome write_note(const struct storage_path *_Nonnull path,
                                           const struct note_text *_Nonnull body)
{
    wchar_t native[storage_native_capacity];
    size_t length = 0;
    if (!storage_native_path_fill(path, native, &length))
        return PERSISTENCE_UNWRITABLE;
    return file_bytes_create(native, note_text_file_bytes(body), note_text_file_length(body));
}

static enum persistence_outcome write_archive(const struct storage_path *_Nonnull path,
                                              const struct note_archive *_Nonnull archive)
{
    wchar_t native[storage_native_capacity];
    size_t length = 0;
    if (!storage_native_path_fill(path, native, &length))
        return PERSISTENCE_UNWRITABLE;
    return file_bytes_export_archive(native, archive);
}

struct transfer_port transfer_adapter_port(void)
{
    return (struct transfer_port){
        .read = read_note, .write_note = write_note, .write_archive = write_archive};
}
