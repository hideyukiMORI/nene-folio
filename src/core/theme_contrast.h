/* 線の色を全テーマで隣接色 3:1 以上に保つ正典の1本（ADR0060の決定3・WCAG 2.1 1.4.11）。
 * 役割の既定トークンが足りないテーマでは、決まった順の別の文字系トークンへ機械的に切り替える。 */
#ifndef NENEFOLIO_THEME_CONTRAST_H
#define NENEFOLIO_THEME_CONTRAST_H

#include "rgb_color.h"
#include "theme_line.h"

struct theme_palette;

/* 状態・境界・focusの線が隣接色に対して保つ比。 */
constexpr double theme_contrast_line_minimum = 3.0;

/* WCAG 2.1 の相対輝度によるコントラスト比（1〜21）。引数の順は問わない。 */
[[nodiscard]] double theme_contrast_ratio(struct rgb_color first, struct rgb_color second);

/* 役割の線の色。既定トークン、次に current / category / editor / selected / note / header
 * の文字色の順で、 役割の地すべてに 3:1
 * 以上の最初の色を返す。どれも届かなければ最も比の高い色を返す。 */
[[nodiscard]] struct rgb_color theme_contrast_line(const struct theme_palette *_Nonnull palette,
                                                   enum theme_line line);

#endif
