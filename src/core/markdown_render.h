#ifndef NENEFOLIO_MARKDOWN_RENDER_H
#define NENEFOLIO_MARKDOWN_RENDER_H
struct text_buffer;
struct markdown_media;
struct markdown_references;
/* Borrowed output sinks of the same canonical Markdown traversal. */
struct markdown_render
{
    struct text_buffer *_Nonnull text;
    struct markdown_media *_Nonnull media;
    /* The size of the surrounding text in RTF half-points (a heading's, or the body's 22).
     * Inline code is drawn relative to it (#853). */
    size_t half_points;
    /* The note's link reference definitions (#861), or none outside a full render. */
    struct markdown_references *_Nullable references; /* footnotes are numbered as met */
};
#endif
