#include "markdown_media.h"
#include "diagram_limits.h"
#include "text_buffer.h"
#include <stdlib.h>
constexpr size_t source_byte_limit = 4 * 1024 * 1024;
struct markdown_media
{
    struct markdown_images *_Nonnull images;
    enum markdown_media_kind kinds[markdown_media_display_limit];
    enum image_outcome statuses[markdown_media_display_limit];
    size_t image_indices[markdown_media_display_limit];
    struct note_text *_Nullable diagrams[markdown_media_display_limit];
    struct text_buffer *_Nullable collecting;
    size_t collecting_slot;
    size_t source_bytes;
    size_t count;
    bool overflow;
};
static void prepared_destroy(struct markdown_media *_Nullable media,
                             struct markdown_images *_Nullable images)
{
    markdown_images_destroy(images);
    free(media);
}
enum image_outcome markdown_media_create(const char *_Nonnull source,
                                         struct markdown_media *_Nullable *_Nonnull out)
{
    struct markdown_images *_Nullable images = nullptr;
    enum image_outcome outcome = markdown_images_create(source, &images);
    if (outcome != IMAGE_READY)
        return outcome;
    struct markdown_media *_Nullable media = calloc(1, sizeof *media);
    if (media == nullptr)
    {
        prepared_destroy(media, images);
        return IMAGE_OUT_OF_MEMORY;
    }
    media->images = images;
    media->collecting_slot = diagram_none;
    *out = media;
    return IMAGE_READY;
}
enum image_outcome markdown_media_image(struct markdown_media *_Nonnull media,
                                        struct markdown_view view,
                                        struct note_search_span destination, size_t *_Nonnull slot)
{
    size_t image = markdown_images_count(media->images);
    enum image_outcome outcome = markdown_images_append(media->images, view, destination);
    if (outcome != IMAGE_READY)
        return outcome;
    if (media->count == markdown_media_display_limit)
    {
        media->overflow = true;
        return IMAGE_TOO_LARGE;
    }
    *slot = media->count;
    media->kinds[media->count] = MARKDOWN_MEDIA_IMAGE;
    media->statuses[media->count] = IMAGE_READY;
    media->image_indices[media->count++] = image;
    return IMAGE_READY;
}
enum image_outcome markdown_media_begin_diagram(struct markdown_media *_Nonnull media,
                                                size_t *_Nonnull slot)
{
    if (media->collecting != nullptr)
        return IMAGE_FAILED;
    if (media->count == markdown_media_display_limit)
    {
        media->overflow = true;
        return IMAGE_TOO_LARGE;
    }
    if (text_buffer_create(&media->collecting) != TEXT_BUFFER_ACCEPTED)
        return IMAGE_OUT_OF_MEMORY;
    *slot = media->collecting_slot = media->count;
    media->kinds[media->count] = MARKDOWN_MEDIA_DIAGRAM;
    media->statuses[media->count++] = IMAGE_READY;
    return IMAGE_READY;
}
void markdown_media_diagram_line(struct markdown_media *_Nonnull media, size_t slot,
                                 struct markdown_line line)
{
    if (slot == diagram_none || slot != media->collecting_slot ||
        media->statuses[slot] != IMAGE_READY)
        return;
    size_t length = line.length - line.prefix + line.padding + 1;
    size_t held = text_buffer_length(media->collecting);
    if (length > diagram_source_limit - held || length > source_byte_limit - media->source_bytes)
    {
        media->statuses[slot] = IMAGE_TOO_LARGE;
        return;
    }
    for (size_t index = 0; index < line.padding; ++index)
        text_buffer_append_text(media->collecting, " ");
    text_buffer_append(media->collecting, line.bytes + line.prefix, line.length - line.prefix);
    text_buffer_append_text(media->collecting, "\n");
    media->source_bytes += length;
}
enum image_outcome markdown_media_end_diagram(struct markdown_media *_Nonnull media, size_t slot)
{
    if (slot == diagram_none)
        return IMAGE_READY;
    if (slot != media->collecting_slot || media->collecting == nullptr)
        return IMAGE_FAILED;
    if (text_buffer_finish(media->collecting) != TEXT_BUFFER_ACCEPTED)
        media->statuses[slot] = IMAGE_OUT_OF_MEMORY;
    if (media->statuses[slot] == IMAGE_READY)
    {
        enum note_text_outcome result =
            note_text_create(text_buffer_bytes(media->collecting),
                             text_buffer_length(media->collecting), &media->diagrams[slot]);
        if (result != NOTE_TEXT_ACCEPTED)
            media->statuses[slot] = IMAGE_OUT_OF_MEMORY;
    }
    text_buffer_destroy(media->collecting);
    media->collecting = nullptr;
    media->collecting_slot = diagram_none;
    return media->statuses[slot] == IMAGE_OUT_OF_MEMORY ? IMAGE_OUT_OF_MEMORY : IMAGE_READY;
}
size_t markdown_media_count(const struct markdown_media *_Nonnull media)
{
    return media->count;
}
bool markdown_media_overflow(const struct markdown_media *_Nonnull media)
{
    return media->overflow || markdown_images_overflow(media->images);
}
const struct markdown_images *_Nonnull markdown_media_images(
    const struct markdown_media *_Nonnull media)
{
    return media->images;
}
enum markdown_media_kind markdown_media_kind(const struct markdown_media *_Nonnull media,
                                             size_t slot)
{
    return media->kinds[slot];
}
enum image_outcome markdown_media_status(const struct markdown_media *_Nonnull media, size_t slot)
{
    return media->statuses[slot];
}
size_t markdown_media_image_index(const struct markdown_media *_Nonnull media, size_t slot)
{
    return media->image_indices[slot];
}
const struct note_text *_Nonnull markdown_media_diagram(const struct markdown_media *_Nonnull media,
                                                        size_t slot)
{
    return media->diagrams[slot];
}
void markdown_media_destroy(struct markdown_media *_Nullable media)
{
    if (media == nullptr)
        return;
    for (size_t index = 0; index < media->count; ++index)
        note_text_destroy(media->diagrams[index]);
    text_buffer_destroy(media->collecting);
    markdown_images_destroy(media->images);
    free(media);
}

void markdown_media_placeholder(struct text_buffer *_Nonnull output, size_t index)
{
    text_buffer_append_text(output, "{\\v <nenefolio:image:");
    char digits[5];
    size_t count = 0;
    do
    {
        digits[count++] = (char)('0' + index % 10);
        index /= 10;
    } while (index != 0 && count < sizeof digits);
    while (count != 0)
        text_buffer_append(output, &digits[--count], 1);
    text_buffer_append_text(output, ">}{\\pict\\pngblip\\picw16\\pich16\\picwgoal240\\pichgoal240 "
                                    "89504e470d0a1a0a0000000d49484452000000100000001008060000001ff3"
                                    "ff610000003f49444154789c6360a006c8"
                                    "ab68fa4f0e4631801c4b470da08501a41884d300428660b50cdd005c8620cb"
                                    "113400dd107471a20cc06730f56381a2cc"
                                    "44090000b793b8fce98c622c0000000049454e44ae426082"
                                    "}");
}
