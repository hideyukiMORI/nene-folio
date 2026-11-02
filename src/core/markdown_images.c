#include "markdown_images.h"
#include "markdown_destination.h"
#include "text_buffer.h"
#include <stdlib.h>

struct markdown_images
{
    const char *_Nonnull source;
    struct image_source *_Nullable entries[markdown_images_reference_limit];
    size_t count;
    bool overflow;
};

enum image_outcome markdown_images_create(const char *_Nonnull source,
                                          struct markdown_images *_Nullable *_Nonnull out)
{
    struct markdown_images *_Nullable images = calloc(1, sizeof *images);
    if (images == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    images->source = source;
    *out = images;
    return IMAGE_READY;
}

static size_t raw_offset(const struct markdown_images *_Nonnull images, struct markdown_view view,
                         size_t offset)
{
    for (size_t index = 0; index < view.count; ++index)
    {
        const struct markdown_span *span = &view.spans[index];
        if (offset >= span->offset && offset <= span->offset + span->length)
            return (size_t)(span->bytes - images->source) + offset - span->offset;
    }
    return 0;
}

enum image_outcome markdown_images_append(struct markdown_images *_Nonnull images,
                                          struct markdown_view view,
                                          struct note_search_span destination)
{
    if (images->count == markdown_images_reference_limit)
    {
        images->overflow = true;
        return IMAGE_TOO_LARGE;
    }
    struct markdown_destination *_Nullable decoded = nullptr;
    enum image_outcome outcome = markdown_destination_create(view, destination, &decoded);
    if (outcome != IMAGE_READY)
        return outcome;
    destination = markdown_destination_span(decoded);
    struct note_search_span raw = {.start = raw_offset(images, view, destination.start),
                                   .end = raw_offset(images, view, destination.end)};
    outcome = image_source_create(markdown_destination_bytes(decoded), raw,
                                  &images->entries[images->count]);
    markdown_destination_destroy(decoded);
    if (outcome == IMAGE_READY)
        ++images->count;
    return outcome;
}

size_t markdown_images_count(const struct markdown_images *_Nonnull images)
{
    return images->count;
}
bool markdown_images_overflow(const struct markdown_images *_Nonnull images)
{
    return images->overflow;
}
bool markdown_images_advisory(const struct markdown_images *_Nonnull images)
{
    for (size_t index = 0; index < images->count; ++index)
        if (image_source_kind(images->entries[index]) == IMAGE_SOURCE_LOCAL &&
            !image_source_recommended(images->entries[index]))
            return true;
    return false;
}
const struct image_source *_Nonnull markdown_images_at(
    const struct markdown_images *_Nonnull images, size_t index)
{
    return images->entries[index];
}

void markdown_images_placeholder(struct text_buffer *_Nonnull output, size_t index)
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

void markdown_images_destroy(struct markdown_images *_Nullable images)
{
    if (images != nullptr)
        for (size_t index = 0; index < images->count; ++index)
            image_source_destroy(images->entries[index]);
    free(images);
}
