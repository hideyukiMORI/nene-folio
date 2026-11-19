#ifndef NENEFOLIO_MARKDOWN_BLOCKS_H
#define NENEFOLIO_MARKDOWN_BLOCKS_H
#include "markdown_blocks_options.h"
#include "markdown_render.h"
#include "markdown_rtf_outcome.h"
#include <stddef.h>
struct text_buffer;
struct note_text;
/* language はコードブロックのコピー操作の文言、contrast はその下線の要否だけに使う（ADR0098）。 */
[[nodiscard]] enum markdown_rtf_outcome
markdown_blocks_render(struct markdown_render render, const struct note_text *_Nonnull text,
                       struct markdown_blocks_options options);
#endif
