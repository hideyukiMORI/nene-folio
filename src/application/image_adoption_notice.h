/* #1062 / ADR0120: 画像の投下について、利用者へ出す 1 行を選ぶ（どの文言を、どの数で）。
 * 文言そのものは core の ui_text が持ち、ここは ID と数だけを返す純粋な関数。窓は
 * ui_text_format へ渡して出すだけで、どの文にするかを決めない（ARC-011）。 */
#ifndef NENEFOLIO_IMAGE_ADOPTION_NOTICE_H
#define NENEFOLIO_IMAGE_ADOPTION_NOTICE_H
#include "image_adoption_landing.h"
#include "image_adoption_start.h"
#include "image_adoption_summary.h"
#include "ui_text.h"
#include <stddef.h>
struct image_adoption_notice
{
    enum ui_text id; /* UI_TEXT_EMPTY は「何も出さない」 */
    size_t n;        /* {n}: 取り込んだ数。進みの文では、扱う総数 */
    size_t k;        /* {k}: 取り込まなかった数。進みの文では、いま扱っている番号 */
    size_t offset;   /* {offset}: 1 回の上限 */
};
/* 始めなかった理由の 1 行。始まった・ファイルが無い・記憶が足りない、は UI_TEXT_EMPTY
 * （記憶不足は、窓がいつもの失敗の道で知らせる）。 */
[[nodiscard]] struct image_adoption_notice
image_adoption_notice_refused(enum image_adoption_start start);
/* 進んでいるあいだの 1 行（「k/n」）。 */
[[nodiscard]] struct image_adoption_notice
image_adoption_notice_progress(const struct image_adoption_summary *_Nonnull summary);
/* 終わった後の 1 行。landing は、ためた Markdown が本文へ入ったかどうか（窓が答える）。 */
[[nodiscard]] struct image_adoption_notice
image_adoption_notice_finished(const struct image_adoption_summary *_Nonnull summary,
                               enum image_adoption_landing landing);
#endif
