#ifndef NENEFOLIO_WEB_URI_H
#define NENEFOLIO_WEB_URI_H
#include "web_uri_outcome.h"
#include <stddef.h>
struct web_uri;
constexpr size_t web_uri_byte_limit = 8192;
/* Absolute HTTP(S), no userinfo, controls, whitespace, quotes, backslash or angle brackets.
 * Owns the exact UTF-8 destination. Failure leaves out unchanged. */
[[nodiscard]] enum web_uri_outcome web_uri_create(const char *_Nonnull bytes, size_t length,
                                                  struct web_uri *_Nullable *_Nonnull out);
[[nodiscard]] const char *_Nonnull web_uri_bytes(const struct web_uri *_Nonnull uri);
[[nodiscard]] size_t web_uri_length(const struct web_uri *_Nonnull uri);
void web_uri_destroy(struct web_uri *_Nullable uri);
#endif
