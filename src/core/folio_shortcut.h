#ifndef NENEFOLIO_FOLIO_SHORTCUT_H
#define NENEFOLIO_FOLIO_SHORTCUT_H
#include "folio_command.h"
#include "folio_shortcut_action.h"
#include "folio_shortcut_key.h"
#include "folio_shortcut_modifiers.h"
#include "folio_shortcut_route.h"
#include "ui_text.h"
struct folio_shortcut;
[[nodiscard]] size_t folio_shortcut_count(void);
[[nodiscard]] const struct folio_shortcut *_Nonnull folio_shortcut_at(size_t index);
[[nodiscard]] enum folio_shortcut_route
folio_shortcut_route(const struct folio_shortcut *_Nonnull shortcut);
[[nodiscard]] enum folio_shortcut_key
folio_shortcut_key(const struct folio_shortcut *_Nonnull shortcut);
[[nodiscard]] enum folio_shortcut_modifiers
folio_shortcut_modifiers(const struct folio_shortcut *_Nonnull shortcut);
[[nodiscard]] enum folio_shortcut_action
folio_shortcut_action(const struct folio_shortcut *_Nonnull shortcut);
[[nodiscard]] enum folio_command
folio_shortcut_command(const struct folio_shortcut *_Nonnull shortcut);
[[nodiscard]] const char *_Nonnull folio_shortcut_label(
    const struct folio_shortcut *_Nonnull shortcut, enum folio_language language);
/* COMMANDの第一binding。無ければ第一Ex別名、どちらも無ければ空文字。 */
[[nodiscard]] const char *_Nonnull folio_shortcut_command_hint(enum folio_command command,
                                                               enum folio_language language);
[[nodiscard]] enum ui_text_format_outcome folio_shortcut_help(enum ui_text id,
                                                              enum folio_language language,
                                                              char *_Nonnull out, size_t capacity);
#endif
