#ifndef NENEFOLIO_FOLIO_MENU_ACTION_H
#define NENEFOLIO_FOLIO_MENU_ACTION_H
#include "folio_command.h"
#include "folio_menu_action_kind.h"
#include "folio_menu_context.h"
#include "folio_menu_group.h"
struct folio_menu_action;
[[nodiscard]] size_t folio_menu_action_count(enum folio_menu_context context);
[[nodiscard]] const struct folio_menu_action *_Nonnull folio_menu_action_at(
    enum folio_menu_context context, size_t index);
[[nodiscard]] enum folio_menu_action_kind
folio_menu_action_kind(const struct folio_menu_action *_Nonnull action);
/* COMMAND variantだけで呼ぶ。descriptorは静的で変更も確保も不要。 */
[[nodiscard]] enum folio_command
folio_menu_action_command(const struct folio_menu_action *_Nonnull action);
[[nodiscard]] enum folio_menu_group
folio_menu_action_group(const struct folio_menu_action *_Nonnull action);
[[nodiscard]] char folio_menu_action_access_key(const struct folio_menu_action *_Nonnull action);
[[nodiscard]] enum ui_text_format_outcome
folio_menu_action_caption(const struct folio_menu_action *_Nonnull action,
                          enum folio_language language, char *_Nonnull out, size_t capacity);
/* キー、コロン付きEx名、またはこのメニュー内のアクセスキー案内。 */
[[nodiscard]] enum ui_text_format_outcome
folio_menu_action_hint(const struct folio_menu_action *_Nonnull action,
                       enum folio_language language, char *_Nonnull out, size_t capacity);
#endif
