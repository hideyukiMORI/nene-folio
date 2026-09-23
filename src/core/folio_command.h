/* Ex・操作パレット・既存ショートカットが共有する安定した操作 ID と登録表（ADR 0016）。
 * 表示名と Ex 別名は同じ静的な登録表から読み、UI は ID だけを実行する。
 * command 引数にはこの列挙型の値だけを渡す。文字入力は parse で検証し、整数から作らない。 */
#ifndef NENEFOLIO_FOLIO_COMMAND_H
#define NENEFOLIO_FOLIO_COMMAND_H

#include "ex_substitute.h"
#include "folio_argument_kind.h"
#include "folio_language.h"
#include "folio_option.h"

#include <stddef.h>

enum folio_command : unsigned char
{
    FOLIO_COMMAND_SAVE,
    FOLIO_COMMAND_QUIT,
    FOLIO_COMMAND_SAVE_QUIT,
    FOLIO_COMMAND_FORCE_QUIT,
    FOLIO_COMMAND_HELP,
    FOLIO_COMMAND_EDIT,
    FOLIO_COMMAND_VIEW,
    FOLIO_COMMAND_NEW,
    FOLIO_COMMAND_SAVE_AS,
    FOLIO_COMMAND_RENAME,
    FOLIO_COMMAND_FIND,
    FOLIO_COMMAND_SET,
    FOLIO_COMMAND_TOGGLE_NUMBER,
    FOLIO_COMMAND_REPLACE,      /* 置換の欄を開く（ADR 0028 の決定 8(a)） */
    FOLIO_COMMAND_SUBSTITUTE,   /* `:%s/…/…/[g]` を欄を開かず直接適用する（決定 8(b)） */
    FOLIO_COMMAND_SETTINGS,     /* 設定画面を開く（ADR 0031 の決定 7・8(b)） */
    FOLIO_COMMAND_DISCARD_EDITS /* 編集を破棄して保存済みの本文に戻す（ADR 0016 の補正 4） */
};

[[nodiscard]] size_t folio_command_count(void);
/* index は 0 <= index < folio_command_count()。 */
[[nodiscard]] enum folio_command folio_command_at(size_t index);
/* 日本語の表示名。#38 で同じ ID の翻訳表へ移す。 */
[[nodiscard]] const char *_Nonnull folio_command_label(enum folio_command command,
                                                       enum folio_language language);
/* GUI専用操作は0個。未実装という意味ではない（ADR 0018）。 */
[[nodiscard]] size_t folio_command_alias_count(enum folio_command command);
/* index は 0 <= index < folio_command_alias_count(command)。 */
[[nodiscard]] const char *_Nonnull folio_command_alias(enum folio_command command, size_t index);
/* 登録表の別名を解釈。名前引数を許す操作は閉じた集合で決め、その開始位置を返す。無しならlength。
 * 名前の途中/末尾の空白は保持し、名前型で検証する（ADR0020）。 */
[[nodiscard]] bool folio_command_parse(const char *_Nonnull text, size_t length,
                                       enum folio_command *_Nonnull out, size_t *_Nonnull argument);
/* パレットの絞り込み。空なら全件。表示名または Ex 別名の部分一致。 */
[[nodiscard]] bool folio_command_matches(enum folio_command command, const char *_Nonnull text,
                                         size_t length, enum folio_language language);
/* 操作パレットと「操作」メニューに出す操作か（ADR 0016 の決定 1 / ADR 0018 の決定 2 の補正）。
 * 引数を渡せない面なので、語を要る Ex の文法（`:set`）だけが偽になる。
 * 新しい操作を足すたびに閉じた switch が決めさせる。 */
[[nodiscard]] bool folio_command_listed(enum folio_command command);
/* パレットの箱の高さを決める、出す操作の数（ADR 0026 の補正 9）。
 * 登録表の総数ではないので、出さない操作を足しても箱に空の行が増えない。 */
[[nodiscard]] size_t folio_command_listed_count(void);
/* `:set` の語を解く（ADR 0026 の決定 8）。空白で切った 1 語の完全一致だけを受ける。
 * 語なし・未知の語・余計な語があれば false で out は触らない。 */
[[nodiscard]] bool folio_command_parse_option(const char *_Nonnull text, size_t length,
                                              enum folio_option *_Nonnull out);
/* `:%s` の後ろを解く（ADR 0028 の決定 8(b)）。text は folio_command_parse が返した argument の
 * 位置から渡す。区切りは `/` だけで、`\` の次の 1 文字は区切りにならない（`\/` はそのまま残す）。
 * 旗は `g` だけ。区切りの不足・空のパターン・知らない旗は false で out は触らない。 */
[[nodiscard]] bool folio_command_parse_substitute(const char *_Nonnull text, size_t length,
                                                  struct ex_substitute *_Nonnull out);

#endif
