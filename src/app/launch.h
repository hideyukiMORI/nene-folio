#ifndef NENEFOLIO_LAUNCH_H
#define NENEFOLIO_LAUNCH_H
#include "font_bundle.h"
struct utf8_text;
/* この起動に固有のもの。入口が 1 回作り、合成ルートの関数へ明示的に渡す（ARC-006）。
 * link は起動の引数にあった「開く」依頼のリンク（#1071 / ADR0118）で、無ければ nullptr。 */
struct launch
{
    enum font_bundle_outcome fonts;
    const struct utf8_text *_Nullable link;
};
#endif
