/* 言語ごとの UI の書体の face 名（ADR 0032 の決定 4）。
 * ui（ドロワー・名前入力面・RichEdit の既定書式）と core（markdown_rtf の fonttbl）が
 * 同じ 1 つの表から引く。face 名は文言ではないので ui_text には置かない（C-018）。
 * 主窓の自前描画（パンくず・札・ヘルプ・状態行・番号帯・EDIT）は Consolas のままで、
 * ここは通らない。#42 が同梱フォントを入れたら ui_font.c の表だけを差し替える。 */
#ifndef NENEFOLIO_UI_FONT_H
#define NENEFOLIO_UI_FONT_H

#include "folio_language.h"

/* ASCII の終端付き。RTF の fonttbl にも Win32 の LOGFONT にもそのまま入る
 * （`{` `}` `\` `;` を含まないことは単体が固定する）。表の外の値へは落ちない。 */
[[nodiscard]] const char *_Nonnull ui_font_face(enum folio_language language);

#endif
