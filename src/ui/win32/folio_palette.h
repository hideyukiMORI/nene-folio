/* UI が自前で描く部分の色の束。テーマごとの値の正本はここ 1 つ（ARC-001・ADR 0005）。
 * 全メンバーが独立に妥当な色なので完全型で公開する（C-003）。RTF の色は core の rtf_palette。 */
#ifndef NENEFOLIO_FOLIO_PALETTE_H
#define NENEFOLIO_FOLIO_PALETTE_H

#include "folio_theme.h"

#include <windows.h>

struct folio_palette
{
    COLORREF window;              /* 窓とドロワーの地 */
    COLORREF pane;                /* 右ペインの地 */
    COLORREF header_text;         /* ドロワーの頭・右ペインの頭の薄い文字 */
    COLORREF category_text;       /* カテゴリ名 */
    COLORREF note_text;           /* ノート名 */
    COLORREF selected_background; /* 選択中のノート行の面 */
    COLORREF selected_text;       /* 選択中のノート名 */
    COLORREF current_text;        /* 右ペインの頭の、いま見ているノート名 */
    COLORREF editor_text;         /* 編集モードの本文 */
    COLORREF chip_background;     /* 有効な側の札の地 */
    COLORREF chip_text;           /* 有効な側の札の文字 */
    COLORREF border;              /* 窓の縁と区切り記号 */
};

[[nodiscard]] struct folio_palette folio_palette_for(enum folio_theme theme);

#endif
