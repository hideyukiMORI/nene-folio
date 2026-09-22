/* data/settings.json の設定（ADR 0025・ADR 0031 の決定 1）。
 * 効果を実装したキーだけがスキーマに入る。
 * 版 3 の形（キーの順序も固定。違えば MALFORMED で既定値には落ちない・ARC-009）:
 *   {"version": 3, "number": false, "theme": "system", "language": "ja"}
 * 版 1 の形 {"version": 1, "number": <bool>} と版 2 の形
 * {"version": 2, "number": <bool>, "theme": "…"} も読めて、足りないキーに既定値を埋める（移行）。
 * キーを足すときは必ず版を上げ、旧版からの移行は core の純関数で行う（決定 2）。
 * 書くのは常に最新の版で、版 1・版 2 のファイルは次に書いた時点で版 3 になる。 */
#ifndef NENEFOLIO_FOLIO_SETTINGS_H
#define NENEFOLIO_FOLIO_SETTINGS_H

#include "folio_language.h"
#include "folio_settings_outcome.h"
#include "folio_theme_choice.h"

#include <stddef.h>

struct folio_settings;
struct json_writer;

/* ファイルが無いときに使う既定値（number = false・theme = system・language = ja）。 */
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
/* theme だけが違う新しい設定を作る（ADR 0031 の決定 1）。with_number と同じ流儀。 */
[[nodiscard]] enum folio_settings_outcome
folio_settings_with_theme(const struct folio_settings *_Nonnull settings,
                          enum folio_theme_choice theme,
                          struct folio_settings *_Nullable *_Nonnull out);
/* language だけが違う新しい設定を作る（ADR 0032 の決定 1）。with_theme と同じ流儀。 */
[[nodiscard]] enum folio_settings_outcome
folio_settings_with_language(const struct folio_settings *_Nonnull settings,
                             enum folio_language language,
                             struct folio_settings *_Nullable *_Nonnull out);
/* writer へ版 3 の文書を 1 つ書く。完了の判定は json_writer_finish で行う。 */
void folio_settings_write(const struct folio_settings *_Nonnull settings,
                          struct json_writer *_Nonnull writer);
/* 編集中の本文の左に原文の行番号を出すか（FR-021 / ADR 0026）。 */
[[nodiscard]] bool folio_settings_number(const struct folio_settings *_Nonnull settings);
/* 利用者が選んだテーマ（ADR 0031 の決定 1）。描画に使う値は folio_theme_resolve が決める。 */
[[nodiscard]] enum folio_theme_choice
folio_settings_theme(const struct folio_settings *_Nonnull settings);
/* 利用者が選んだ表示言語（ADR 0032 の決定 1）。UI へ渡すのは application である。 */
[[nodiscard]] enum folio_language
folio_settings_language(const struct folio_settings *_Nonnull settings);
void folio_settings_destroy(struct folio_settings *_Nullable settings);

#endif
