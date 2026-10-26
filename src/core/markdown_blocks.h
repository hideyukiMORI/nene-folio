#ifndef NENEFOLIO_MARKDOWN_BLOCKS_H
#define NENEFOLIO_MARKDOWN_BLOCKS_H
#include "markdown_rtf_outcome.h"
#include <stddef.h>
struct text_buffer;
[[nodiscard]] enum markdown_rtf_outcome
markdown_blocks_render(struct text_buffer *_Nonnull out, const char *_Nonnull text, size_t length);
#endif
