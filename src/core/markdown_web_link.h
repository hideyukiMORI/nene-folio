#ifndef NENEFOLIO_MARKDOWN_WEB_LINK_H
#define NENEFOLIO_MARKDOWN_WEB_LINK_H
#include "markdown_view.h"
#include "web_uri.h"
/* A borrowed destination span is parsed only here. Autolinks keep their literal bytes. */
[[nodiscard]] enum web_uri_outcome
markdown_web_link_destination(struct markdown_view view, size_t start, size_t end,
                              struct web_uri *_Nullable *_Nonnull out);
[[nodiscard]] enum web_uri_outcome
markdown_web_link_autolink(struct markdown_view view, size_t start, size_t end,
                           struct web_uri *_Nullable *_Nonnull out);
#endif
