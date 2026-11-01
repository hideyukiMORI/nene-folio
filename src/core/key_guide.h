#ifndef NENEFOLIO_KEY_GUIDE_H
#define NENEFOLIO_KEY_GUIDE_H

#include "folio_language.h"
#include "ui_text.h"
#include <stddef.h>

/* 不変の説明catalog。実キーはshortcut/commandから参照し、操作を所有しない。 */
struct key_guide;
constexpr size_t key_guide_capacity = 96;
[[nodiscard]] size_t key_guide_count(void);
[[nodiscard]] const struct key_guide *_Nonnull key_guide_at(size_t index);
[[nodiscard]] enum ui_text key_guide_group(const struct key_guide *_Nonnull row);
[[nodiscard]] const char *_Nonnull key_guide_description(const struct key_guide *_Nonnull row,
                                                         enum folio_language language);
[[nodiscard]] enum ui_text_format_outcome key_guide_key(const struct key_guide *_Nonnull row,
                                                        enum folio_language language,
                                                        char *_Nonnull out, size_t capacity);
[[nodiscard]] enum ui_text_format_outcome key_guide_name(const struct key_guide *_Nonnull row,
                                                         enum folio_language language,
                                                         char *_Nonnull out, size_t capacity);

#endif
