/* 言語ごとの UI の書体の face 名（ADR 0032 の決定 4・ADR 0036 の決定 4）。
 * ui（ドロワー・名前入力面・RichEdit の既定書式）と core（markdown_rtf の fonttbl）が
 * 同じ表から引く。face 名は文言ではないので ui_text には置かない（C-018）。
 * 主窓の自前描画（パンくず・札・ヘルプ・状態行・番号帯・EDIT）は Consolas のままで、
 * ここは通らない。表は 2 つで、同梱の face は ui_font.c、OS の退避の face は ui_font_fallback.c
 * （#42 で差し替えた。CNF-009 が表ごとに網羅を守る）。 */
#ifndef NENEFOLIO_UI_FONT_H
#define NENEFOLIO_UI_FONT_H

#include "folio_language.h"

/* ASCII の終端付き。RTF の fonttbl にも Win32 の LOGFONT にもそのまま入る
 * （`{` `}` `\` `;` を含まないことは単体が固定する）。表の外の値へは落ちない。 */

/* 同梱の face（fonts/ の Noto Sans JP / Noto Sans SC。English は Noto Sans JP のラテン文字）。 */
[[nodiscard]] const char *_Nonnull ui_font_face(enum folio_language language);

/* OS の退避の face（同梱が登録できなかったときに使う）。 */
[[nodiscard]] const char *_Nonnull ui_font_fallback_face(enum folio_language language);

/* 3 段の選択（ADR 0036 の決定 4）: 同梱が在ればそれ、無ければその言語の退避、
 * それも無ければ日本語の退避。実在は ui が確かめて渡す（core は OS を読まない・ARC-007）。 */
[[nodiscard]] const char *_Nonnull ui_font_pick(enum folio_language language, bool bundled_present,
                                                bool fallback_present);

#endif
