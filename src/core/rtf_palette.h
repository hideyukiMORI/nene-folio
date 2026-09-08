/* 右ペインの RTF に使う色の束。テーマごとの値の正本はここ 1 つ（ARC-001・ADR 0005）。
 * 全メンバーが独立に妥当な色なので完全型で公開する（C-003）。 */
#ifndef NENEFOLIO_RTF_PALETTE_H
#define NENEFOLIO_RTF_PALETTE_H

#include "folio_theme.h"
#include "rgb_color.h"

struct rtf_palette
{
    struct rgb_color text;            /* 本文 */
    struct rgb_color heading;         /* 見出し */
    struct rgb_color muted;           /* 引用の文字 */
    struct rgb_color link;            /* リンクとインラインコードの文字 */
    struct rgb_color code_text;       /* コードブロックの文字 */
    struct rgb_color code_background; /* コードブロックの地 */
};

[[nodiscard]] struct rtf_palette rtf_palette_for(enum folio_theme theme);

#endif
