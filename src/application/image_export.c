#include "image_export.h"
#include "archive_assets.h"
#include "ascii_fold.h"
#include "folio_theme.h"
#include "image_asset.h"
#include "image_port.h"
#include "markdown_image_copy.h"
#include "markdown_images.h"
#include "markdown_rtf.h"
#include "note_archive.h"
#include "note_text.h"
#include "theme_palette.h"
#include "ui_font.h"
#include <stdlib.h>
#include <string.h>
struct image_export
{
    struct image_port port;
    enum image_export_policy policy;
    struct archive_assets *_Nullable files;
    struct image_asset *_Nullable owners[markdown_images_reference_limit];
    const char *_Nullable categories[markdown_images_reference_limit];
    size_t generated_numbers[markdown_images_reference_limit];
    size_t count;
    size_t references;
    struct note_text *_Nullable *_Nullable copies;
    size_t notes;
    size_t used_notes;
    bool external;
};
enum folio_state_outcome image_export_create(const struct image_port *_Nonnull port,
                                             enum image_export_policy policy, size_t notes,
                                             struct image_export *_Nullable *_Nonnull out)
{
    if (notes > note_archive_count_limit)
        return FOLIO_STATE_EXPORT_TOO_LARGE;
    struct image_export *_Nullable export = calloc(1, sizeof *export);
    if (export == nullptr)
        return FOLIO_STATE_OUT_OF_MEMORY;
    export->port = *port;
    export->policy = policy;
    export->notes = notes;
    export->copies = calloc(notes == 0 ? 1 : notes, sizeof *export->copies);
    if (export->copies == nullptr || archive_assets_create(&export->files) != NOTE_ARCHIVE_READY)
    {
        image_export_destroy(export);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    *out = export;
    return FOLIO_STATE_READY;
}
static enum folio_state_outcome from_read(enum image_outcome outcome)
{
    switch (outcome)
    {
    case IMAGE_READY:
        return FOLIO_STATE_READY;
    case IMAGE_TOO_LARGE:
        return FOLIO_STATE_EXPORT_TOO_LARGE;
    case IMAGE_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    case IMAGE_PENDING:
    case IMAGE_FAILED:
    case IMAGE_UNSUPPORTED:
    case IMAGE_CANCELLED:
    case IMAGE_SKIPPED:
        return FOLIO_STATE_EXPORT_IMAGE_UNREADABLE;
    }
    return FOLIO_STATE_EXPORT_IMAGE_UNREADABLE;
}
static enum folio_state_outcome from_archive(enum note_archive_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_ARCHIVE_READY:
        return FOLIO_STATE_READY;
    case NOTE_ARCHIVE_TOO_LARGE:
        return FOLIO_STATE_EXPORT_TOO_LARGE;
    case NOTE_ARCHIVE_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    case NOTE_ARCHIVE_INVALID:
        return FOLIO_STATE_EXPORT_IMAGE_UNREADABLE;
    }
    return FOLIO_STATE_EXPORT_IMAGE_UNREADABLE;
}
static size_t existing_asset(const struct image_export *_Nonnull export,
                             const char *_Nonnull category,
                             const struct image_asset_input *_Nonnull input)
{
    for (size_t index = 0; index < export->count; ++index)
    {
        const struct image_asset_input *existing = image_asset_content(export->owners[index]);
        if (strcmp(export->categories[index], category) == 0 &&
            strcmp(existing->identity, input->identity) == 0)
            return index;
    }
    return SIZE_MAX;
}
static void append_number(char *_Nonnull path, size_t number)
{
    char digits[20];
    size_t count = 0;
    do
    {
        digits[count++] = (char)('0' + number % 10);
        number /= 10;
    } while (number != 0);
    size_t offset = strlen(path);
    while (count != 0)
        path[offset++] = digits[--count];
    path[offset] = '\0';
}
static const char *_Nonnull extension(const struct image_asset_input *_Nonnull input)
{
    const char *_Nullable last = nullptr;
    for (const char *cursor = input->identity; *cursor != '\0'; ++cursor)
    {
        if (*cursor == '.')
            last = cursor;
        if (*cursor == '\\' || *cursor == '/')
            last = nullptr;
    }
    if (last == nullptr || strlen(last) > 16)
        return ".img";
    return last;
}
static bool same_spelling(const char *_Nonnull first, const char *_Nonnull second)
{
    size_t index = 0;
    while (first[index] != '\0' && ascii_fold_byte(first[index]) == ascii_fold_byte(second[index]))
        ++index;
    return first[index] == '\0' && second[index] == '\0';
}
static size_t next_number(const struct image_export *_Nonnull export, const char *_Nonnull category,
                          const char *_Nonnull suffix)
{
    for (size_t cursor = export->count; cursor != 0; --cursor)
    {
        size_t index = cursor - 1;
        if (export->generated_numbers[index] == 0 ||
            !same_spelling(export->categories[index], category))
            continue;
        const struct image_asset_input *existing = image_asset_content(export->owners[index]);
        if (same_spelling(extension(existing), suffix))
            return export->generated_numbers[index] + 1;
    }
    return 1;
}
static bool asset_path(struct image_export *_Nonnull export, const char *_Nonnull category,
                       const struct image_asset_input *_Nonnull input, char *_Nonnull path)
{
    export->generated_numbers[export->count] = 0;
    size_t category_length = strlen(category);
    size_t relative_length = strlen(input->relative);
    if (category_length + relative_length + 2 > image_source_byte_limit + 256)
        return false;
    memcpy(path, category, category_length);
    path[category_length] = '/';
    char *relative = path + category_length + 1;
    if (relative_length != 0)
    {
        memcpy(relative, input->relative, relative_length + 1);
        return true;
    }
    const char *suffix = extension(input);
    size_t first = next_number(export, category, suffix);
    for (size_t number = first; number <= markdown_images_reference_limit; ++number)
    {
        memcpy(relative, "images/_nenefolio/", sizeof "images/_nenefolio/");
        append_number(relative, number);
        memcpy(relative + strlen(relative), suffix, strlen(suffix) + 1);
        if (!archive_assets_contains(export->files, path))
        {
            export->generated_numbers[export->count] = number;
            return true;
        }
    }
    return false;
}
static enum folio_state_outcome adopt_asset(struct image_export *_Nonnull export,
                                            const char *_Nonnull category,
                                            struct image_asset *_Nonnull asset,
                                            size_t *_Nonnull index)
{
    const struct image_asset_input *input = image_asset_content(asset);
    size_t existing = existing_asset(export, category, input);
    if (existing != SIZE_MAX)
    {
        const struct image_asset_input *previous = image_asset_content(export->owners[existing]);
        if (previous->length != input->length ||
            memcmp(previous->bytes, input->bytes, input->length) != 0)
            return FOLIO_STATE_EXPORT_IMAGE_UNREADABLE;
        *index = existing;
        return FOLIO_STATE_READY;
    }
    if (export->count == markdown_images_reference_limit)
        return FOLIO_STATE_EXPORT_TOO_LARGE;
    char path[image_source_byte_limit + 256];
    if (!asset_path(export, category, input, path))
        return FOLIO_STATE_EXPORT_TOO_LARGE;
    struct archive_file file = {
        .path = path, .bytes = (const char *)input->bytes, .length = input->length};
    enum folio_state_outcome added = from_archive(archive_assets_append(export->files, file));
    if (added != FOLIO_STATE_READY)
        return added;
    *index = export->count;
    export->owners[export->count] = asset;
    export->categories[export->count++] = category;
    return FOLIO_STATE_READY;
}
static enum folio_state_outcome resolve(struct image_export *_Nonnull export,
                                        const struct image_request *_Nonnull request,
                                        const char *_Nullable *_Nonnull relative)
{
    if (image_source_kind(request->source) != IMAGE_SOURCE_LOCAL)
        return FOLIO_STATE_READY;
    struct image_asset *_Nullable asset = nullptr;
    enum image_outcome read =
        export->port.asset(export->port.adapter, request, export->policy, &asset);
    if (read == IMAGE_SKIPPED)
    {
        export->external = true;
        return FOLIO_STATE_READY;
    }
    if (read != IMAGE_READY)
        return from_read(read);
    size_t index = SIZE_MAX;
    enum folio_state_outcome result = adopt_asset(export, request->category, asset, &index);
    if (result == FOLIO_STATE_READY)
    {
        const struct image_asset_input *input = image_asset_content(asset);
        if (!image_source_recommended(request->source) || input->relative[0] == '\0')
            *relative =
                archive_assets_at(export->files, index)->path + strlen(request->category) + 1;
    }
    if (index == SIZE_MAX || export->owners[index] != asset)
        image_asset_destroy(asset);
    return result;
}
static enum folio_state_outcome from_copy(enum note_text_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_TEXT_ACCEPTED:
        return FOLIO_STATE_READY;
    case NOTE_TEXT_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    case NOTE_TEXT_TOO_LARGE:
        return FOLIO_STATE_EXPORT_TOO_LARGE;
    case NOTE_TEXT_INVALID_UTF8:
    case NOTE_TEXT_EMBEDDED_NUL:
        return FOLIO_STATE_EXPORT_IMAGE_UNREADABLE;
    }
    return FOLIO_STATE_EXPORT_IMAGE_UNREADABLE;
}
static enum folio_state_outcome collect_relocations(struct image_export *_Nonnull export,
                                                    const struct archive_note *_Nonnull entry,
                                                    const struct markdown_images *_Nonnull images,
                                                    struct image_relocation *_Nonnull relocations)
{
    size_t used = 0;
    for (size_t index = 0; index < markdown_images_count(images); ++index)
    {
        struct image_request request = {.category = entry->category,
                                        .source = markdown_images_at(images, index)};
        const char *_Nullable relative = nullptr;
        enum folio_state_outcome outcome = resolve(export, &request, &relative);
        if (outcome != FOLIO_STATE_READY)
            return outcome;
        if (relative != nullptr)
            relocations[used++] = (struct image_relocation){
                .source = image_source_span(request.source), .destination = relative};
    }
    if (used == 0)
        return FOLIO_STATE_READY;
    return from_copy(markdown_image_copy_create(entry->body, relocations, used,
                                                &export->copies[export->used_notes]));
}
static enum folio_state_outcome append_images(struct image_export *_Nonnull export,
                                              struct archive_note *_Nonnull entry,
                                              const struct markdown_images *_Nonnull images)
{
    size_t count = markdown_images_count(images);
    if (markdown_images_overflow(images) ||
        count > markdown_images_reference_limit - export->references)
        return FOLIO_STATE_EXPORT_TOO_LARGE;
    export->references += count;
    struct image_relocation *_Nullable relocations =
        calloc(count == 0 ? 1 : count, sizeof *relocations);
    if (relocations == nullptr)
        return FOLIO_STATE_OUT_OF_MEMORY;
    enum folio_state_outcome result = collect_relocations(export, entry, images, relocations);
    if (export->copies[export->used_notes] != nullptr)
        entry->body = export->copies[export->used_notes];
    free(relocations);
    return result;
}
enum folio_state_outcome image_export_append(struct image_export *_Nonnull export,
                                             struct archive_note *_Nonnull entry)
{
    if (export->used_notes == export->notes)
        return FOLIO_STATE_EXPORT_TOO_LARGE;
    struct markdown_rtf *_Nullable rendered = nullptr;
    enum markdown_rtf_outcome parsed =
        markdown_rtf_create(entry->body, folio_theme_palette(FOLIO_THEME_LIGHT)->rtf,
                            ui_font_face(FOLIO_LANGUAGE_JA), &rendered);
    if (parsed != MARKDOWN_RTF_CONVERTED)
        return parsed == MARKDOWN_RTF_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                    : FOLIO_STATE_EXPORT_FAILED;
    const struct markdown_images *images = markdown_rtf_images(rendered);
    enum folio_state_outcome result = append_images(export, entry, images);
    markdown_rtf_destroy(rendered);
    ++export->used_notes;
    return result;
}
bool image_export_external(const struct image_export *_Nonnull export)
{
    return export->external;
}
const struct archive_assets *_Nonnull image_export_assets(
    const struct image_export *_Nonnull export)
{
    return export->files;
}
void image_export_destroy(struct image_export *_Nullable export)
{
    if (export != nullptr)
    {
        for (size_t index = 0; index < export->count; ++index)
            image_asset_destroy(export->owners[index]);
        if (export->copies != nullptr)
            for (size_t index = 0; index < export->used_notes; ++index)
                note_text_destroy(export->copies[index]);
        free(export->copies);
        archive_assets_destroy(export->files);
    }
    free(export);
}
