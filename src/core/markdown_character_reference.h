#ifndef NENEFOLIO_MARKDOWN_CHARACTER_REFERENCE_H
#define NENEFOLIO_MARKDOWN_CHARACTER_REFERENCE_H
#include "markdown_view.h"
#include <stdint.h>
/* Validated borrowed inline input. No match leaves code_point unchanged. */
[[nodiscard]] size_t markdown_character_reference_read(struct markdown_view view, size_t start,
                                                       uint32_t *_Nonnull code_point);
#endif
