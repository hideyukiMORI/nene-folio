#ifndef NENEFOLIO_MARKDOWN_BLOCKS_H
#define NENEFOLIO_MARKDOWN_BLOCKS_H
#include "markdown_render.h"
#include "markdown_rtf_outcome.h"
#include <stddef.h>
struct text_buffer;
struct note_text;
[[nodiscard]] enum markdown_rtf_outcome
markdown_blocks_render(struct markdown_render render, const struct note_text *_Nonnull text);
#endif
