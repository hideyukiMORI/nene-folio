#ifndef NENEFOLIO_MARKDOWN_INLINE_TOKENS_H
#define NENEFOLIO_MARKDOWN_INLINE_TOKENS_H
#include "markdown_render.h"
#include "markdown_rtf_outcome.h"
#include "markdown_view.h"
struct text_buffer;
/* Only temporary delimiter metadata is allocated; the source stays borrowed. */
[[nodiscard]] enum markdown_rtf_outcome markdown_inline_tokens_render(struct markdown_render render,
                                                                      struct markdown_view view);
#endif
