/* data/<カテゴリ>/index.json の台帳（FR-008）。そのカテゴリのノートの表示順を持つ。
 * 版 1 の形（キーの順序も固定。違えば MALFORMED であって既定値には落ちない・FR-015）:
 *   {"version": 1, "notes": ["名前", …]}
 * 名前はファイル名から拡張子 .md を除いたもの。規則と重複禁止は name_list に委ねる。 */
#ifndef NENEFOLIO_NOTE_LEDGER_H
#define NENEFOLIO_NOTE_LEDGER_H

#include "note_ledger_outcome.h"

#include <stddef.h>

struct note_ledger;
struct json_writer;
struct name_list;

[[nodiscard]] enum note_ledger_outcome
note_ledger_empty(struct note_ledger *_Nullable *_Nonnull out);
[[nodiscard]] enum note_ledger_outcome
note_ledger_parse(const char *_Nonnull text, size_t length,
                  struct note_ledger *_Nullable *_Nonnull out);
/* writer へ版 1 の文書を 1 つ書く。完了の判定は json_writer_finish で行う。 */
void note_ledger_write(const struct note_ledger *_Nonnull ledger,
                       struct json_writer *_Nonnull writer);
/* 走査結果と照合した新しい台帳を作る（FR-008）: ledger の順序で scanned にある名前を残し、
 * scanned にだけある名前を末尾に足す。ledger にだけある名前は落ちる。 */
[[nodiscard]] enum note_ledger_outcome
note_ledger_reconcile(const struct note_ledger *_Nonnull ledger,
                      const struct name_list *_Nonnull scanned,
                      struct note_ledger *_Nullable *_Nonnull out);
/* from 番目を to 番目へ移した、順序だけが違う新しい台帳を作る（FR-009）。
 * from と to は count 未満であること。from == to でも複製を返す。 */
[[nodiscard]] enum note_ledger_outcome
note_ledger_moved(const struct note_ledger *_Nonnull ledger, size_t from, size_t to,
                  struct note_ledger *_Nullable *_Nonnull out);
/* index の位置に name（終端付き）を挿した新しい台帳を作る（ADR 0008 の決定 3）。
 * index は count 以下で、count なら末尾に付く。
 * name が名前の規則に反する、または既にあるなら MALFORMED。 */
[[nodiscard]] enum note_ledger_outcome
note_ledger_inserted(const struct note_ledger *_Nonnull ledger, size_t index,
                     const char *_Nonnull name, struct note_ledger *_Nullable *_Nonnull out);
/* index 番目を除いた新しい台帳を作る（ADR 0008 の決定 3）。index は count 未満であること。 */
[[nodiscard]] enum note_ledger_outcome
note_ledger_removed(const struct note_ledger *_Nonnull ledger, size_t index,
                    struct note_ledger *_Nullable *_Nonnull out);
[[nodiscard]] size_t note_ledger_count(const struct note_ledger *_Nonnull ledger);
/* 終端付き。ledger が生きている間だけ有効。index は count 未満であること。 */
[[nodiscard]] const char *_Nonnull note_ledger_name(const struct note_ledger *_Nonnull ledger,
                                                    size_t index);
void note_ledger_destroy(struct note_ledger *_Nullable ledger);

#endif
