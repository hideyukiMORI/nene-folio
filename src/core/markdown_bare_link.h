#ifndef NENEFOLIO_MARKDOWN_BARE_LINK_H
#define NENEFOLIO_MARKDOWN_BARE_LINK_H
#include "markdown_view.h"
/* The longest domain a bare link may name (DNS), which also bounds a failed attempt's scan. */
constexpr size_t markdown_bare_link_domain_limit = 253;
/* GFM extended autolink over borrowed plain text (#850 / ADR0099). Zero means no bare link
 * starts at `start`; otherwise the link's byte length, never reaching past `limit`. */
[[nodiscard]] size_t markdown_bare_link_length(struct markdown_view view, size_t start,
                                               size_t limit);
/* True when the link at `start` is written without a scheme and opens as http://. */
[[nodiscard]] bool markdown_bare_link_www(struct markdown_view view, size_t start);
#endif
