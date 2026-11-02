#ifndef NENEFOLIO_MARKDOWN_RENDER_H
#define NENEFOLIO_MARKDOWN_RENDER_H
struct text_buffer;
struct markdown_images;
/* Borrowed output sinks of the same canonical Markdown traversal. */
struct markdown_render
{
    struct text_buffer *_Nonnull text;
    struct markdown_images *_Nonnull images;
};
#endif
