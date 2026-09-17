/* data/settings.json の設定（ADR 0025）。効果を実装したキーだけがスキーマに入る。
 * 版 1 の形（キーの順序も固定。違えば MALFORMED であって既定値には落ちない・ARC-009）:
 *   {"version": 1, "number": false}
 * キーを足すときは必ず版を上げ、旧版からの移行は core の純関数で行う（決定 2）。
 * 版 1 しか無いあいだ移行の関数は書かない。版が 1 でなければ UNSUPPORTED_VERSION である。 */
#ifndef NENEFOLIO_FOLIO_SETTINGS_H
#define NENEFOLIO_FOLIO_SETTINGS_H

#include "folio_settings_outcome.h"

#include <stddef.h>

struct folio_settings;
struct json_writer;

/* ファイルが無いときに使う既定値（number = false・決定 4）。 */
[[nodiscard]] enum folio_settings_outcome
folio_settings_default(struct folio_settings *_Nullable *_Nonnull out);
[[nodiscard]] enum folio_settings_outcome
folio_settings_parse(const char *_Nonnull text, size_t length,
                     struct folio_settings *_Nullable *_Nonnull out);
/* number だけが違う新しい設定を作る（ARC-005）。同じ値でも複製を返す。
 * 「書く意味があるか」は application が決める（ARC-011 / 決定 5）。 */
[[nodiscard]] enum folio_settings_outcome
folio_settings_with_number(const struct folio_settings *_Nonnull settings, bool number,
                           struct folio_settings *_Nullable *_Nonnull out);
/* writer へ版 1 の文書を 1 つ書く。完了の判定は json_writer_finish で行う。 */
void folio_settings_write(const struct folio_settings *_Nonnull settings,
                          struct json_writer *_Nonnull writer);
/* 編集中の本文の左に原文の行番号を出すか（FR-021 / ADR 0026）。 */
[[nodiscard]] bool folio_settings_number(const struct folio_settings *_Nonnull settings);
void folio_settings_destroy(struct folio_settings *_Nullable settings);

#endif
