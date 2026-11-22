/* #1062 / ADR0120: 落とされたファイルを 1 つずつ images/ へ取り込み、入れる Markdown をためる
 * 一連。ファイルには触らない（触るのは image_port.adopt の先の adapter だけ）。どのノートの
 * どの表示で受けるかは folio_state が決め、ここは「この宛先へ、この一覧を」だけを進める。 */
#ifndef NENEFOLIO_IMAGE_ADOPTION_H
#define NENEFOLIO_IMAGE_ADOPTION_H
#include "image_adoption_start.h"
#include "image_adoption_step.h"
#include "image_adoption_summary.h"
#include <stddef.h>
struct image_adoption;
struct image_port;
struct note_identity;
struct storage_path;
/* 1 回に扱うファイルの上限（ADR0120 の決定 4）。超えた分は扱わず、数だけ覚える。 */
constexpr size_t image_adoption_limit = 64;
/* paths の先頭から image_adoption_limit 個までを写して持つ（呼んだ側の paths は借りない）。
 * destination（宛先のノート）は引き取る: STARTED でなければ、引き取らずに呼んだ側へ残す。
 * 結果は STARTED / NOTHING（count が 0）/ OUT_OF_MEMORY だけ。失敗は out を変えない。 */
[[nodiscard]] enum image_adoption_start
image_adoption_create(struct note_identity *_Nonnull destination,
                      struct storage_path *_Nonnull const *_Nonnull paths, size_t count,
                      struct image_adoption *_Nullable *_Nonnull out);
/* 次の 1 つを取り込む。取り込めたら、その画像の Markdown を（2 つ目からは改行で区切って）足す。 */
[[nodiscard]] enum image_adoption_step
image_adoption_advance(struct image_adoption *_Nonnull adoption,
                       const struct image_port *_Nonnull port);
/* 宛先のノートから離れた・その本文が入れ替わったときに呼ぶ: 残りを扱わずに終え、ためた Markdown は
 * もう入れられないものにする（image_adoption_landable が偽になる）。写した画像はそのまま。 */
void image_adoption_stop(struct image_adoption *_Nonnull adoption);
[[nodiscard]] bool image_adoption_landable(const struct image_adoption *_Nonnull adoption);
[[nodiscard]] struct image_adoption_summary
image_adoption_summary_of(const struct image_adoption *_Nonnull adoption);
[[nodiscard]] const struct note_identity *_Nonnull image_adoption_destination(
    const struct image_adoption *_Nonnull adoption);
/* 取り込んだ画像の Markdown（終端付き。1 つも無ければ空）。adoption が生きている間だけ有効。 */
[[nodiscard]] const char *_Nonnull image_adoption_text(
    const struct image_adoption *_Nonnull adoption);
void image_adoption_destroy(struct image_adoption *_Nullable adoption);
#endif
