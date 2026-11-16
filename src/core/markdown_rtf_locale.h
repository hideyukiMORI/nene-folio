/* 閲覧の RTF が言語から受け取る値（ADR 0032 の決定 5・ADR0098）。face は fonttbl の 0 に入る
 * 本文の書体名（ASCII。`{` `}` `\` `;` を含まない）、language はコードブロックのコピー操作の
 * 文言を引く言語。core は「いまの言語」を持たないので、どちらも呼び出し側が渡す（ARC-005）。
 * 全メンバーが独立に妥当なので完全型で公開する（C-003 の例外）。 */
#ifndef NENEFOLIO_MARKDOWN_RTF_LOCALE_H
#define NENEFOLIO_MARKDOWN_RTF_LOCALE_H
#include "folio_language.h"
struct markdown_rtf_locale
{
    const char *_Nonnull face;
    enum folio_language language;
};
#endif
