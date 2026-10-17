/* UI が自前で描く部分の色の束。core の theme_palette から COLORREF へ写す（ARC-001・ADR0049）。
 * 全メンバーが独立に妥当な色なので完全型で公開する（C-003）。RTF の色は core の rtf_palette。 */
#ifndef NENEFOLIO_FOLIO_PALETTE_H
#define NENEFOLIO_FOLIO_PALETTE_H

#include "theme_palette.h"

#include <windows.h>

struct folio_palette
{
    enum folio_contrast contrast;
    COLORREF window;                /* 窓とドロワーの地 */
    COLORREF pane;                  /* 右ペインの地 */
    COLORREF header_text;           /* ドロワーの頭・右ペインの頭の薄い文字 */
    COLORREF category_text;         /* カテゴリ名 */
    COLORREF note_text;             /* ノート名 */
    COLORREF selected_background;   /* 選択中のノート行の面 */
    COLORREF selected_text;         /* 選択中のノート名 */
    COLORREF current_text;          /* 右ペインの頭の、いま見ているノート名 */
    COLORREF editor_text;           /* 編集モードの本文 */
    COLORREF chip_background;       /* 有効な側の札の地 */
    COLORREF chip_text;             /* 有効な側の札の文字 */
    COLORREF border;                /* 窓の縁と区切り記号 */
    COLORREF breadcrumb_background; /* Powerlineのカテゴリ面 */
    COLORREF breadcrumb_text;       /* カテゴリ面の文字 */
    COLORREF gutter_text;           /* 編集中の本文の左に出す原文の行番号（ADR 0026 の決定 7） */
    COLORREF dialog_surface;        /* 設定カードの不透明な面 */
    COLORREF dialog_border;         /* カードと選択肢の輪郭 */
    COLORREF dialog_control;        /* 未採用の選択肢の面 */
    COLORREF dialog_secondary;      /* 読める濃さを保った版と操作案内 */
    COLORREF dialog_backdrop;       /* 背後操作を遮る暗幕 */
    BYTE dialog_backdrop_alpha;     /* 暗幕だけの不透明度 */
};

[[nodiscard]] struct folio_palette folio_palette_from(const struct theme_palette *_Nonnull palette);
/* カテゴリ色に対し、相対輝度のコントラスト比が高い白か黒を返す（ADR 0017）。 */
[[nodiscard]] COLORREF folio_palette_ink(COLORREF background);

#endif
