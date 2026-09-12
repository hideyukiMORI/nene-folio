/* Ex・操作パレット・既存ショートカットが共有する安定した操作 ID と登録表（ADR 0016）。
 * 表示名と Ex 別名は同じ静的な登録表から読み、UI は ID だけを実行する。 */
#ifndef NENEFOLIO_FOLIO_COMMAND_H
#define NENEFOLIO_FOLIO_COMMAND_H

#include <stddef.h>

enum folio_command : unsigned char
{
    FOLIO_COMMAND_SAVE,
    FOLIO_COMMAND_QUIT,
    FOLIO_COMMAND_SAVE_QUIT,
    FOLIO_COMMAND_FORCE_QUIT,
    FOLIO_COMMAND_HELP
};

[[nodiscard]] size_t folio_command_count(void);
/* index は 0 <= index < folio_command_count()。 */
[[nodiscard]] enum folio_command folio_command_at(size_t index);
/* 日本語の表示名。#38 で同じ ID の翻訳表へ移す。 */
[[nodiscard]] const char *_Nonnull folio_command_label(enum folio_command command);
[[nodiscard]] size_t folio_command_alias_count(enum folio_command command);
/* index は 0 <= index < folio_command_alias_count(command)。 */
[[nodiscard]] const char *_Nonnull folio_command_alias(enum folio_command command, size_t index);
/* 先頭の : は省略可。前後の ASCII 空白を除き、登録済み別名との完全一致だけを受ける。 */
[[nodiscard]] bool folio_command_parse(const char *_Nonnull text, size_t length,
                                       enum folio_command *_Nonnull out);
/* パレットの絞り込み。空なら全件。表示名または Ex 別名の部分一致。 */
[[nodiscard]] bool folio_command_matches(enum folio_command command, const char *_Nonnull text,
                                         size_t length);

#endif
