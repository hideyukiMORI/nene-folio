/* 置換の下見を頼むときに渡すもの（ADR 0028 の決定 6）。本文・パターン・置換文字列と
 * それぞれの長さを束ねる（C-012 の引数 4 つの上限）。どれも呼び出しの間だけ借りる。
 * 本文は RichEdit が今表示している平文で、段落区切りは CR 1 つ（ADR 0023 の決定 1）。
 * 全メンバーが独立に妥当なので完全型で公開する（C-003 の例外）。 */
#ifndef NENEFOLIO_REPLACE_REQUEST_H
#define NENEFOLIO_REPLACE_REQUEST_H

#include <stddef.h>
#include <uchar.h>

struct replace_request
{
    const char16_t *_Nonnull text;
    size_t length;
    const char16_t *_Nonnull pattern;
    size_t pattern_length;
    const char16_t *_Nonnull replacement;
    size_t replacement_length;
};

#endif
