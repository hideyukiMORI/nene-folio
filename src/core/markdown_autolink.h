#ifndef NENEFOLIO_MARKDOWN_AUTOLINK_H
#define NENEFOLIO_MARKDOWN_AUTOLINK_H
#include "markdown_view.h"
/* Validated borrowed inline input. Zero means no complete <URI>/<email> at start. */
[[nodiscard]] size_t markdown_autolink_length(struct markdown_view view, size_t start);
#endif
