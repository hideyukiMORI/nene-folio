/* 同梱の書体の登録を数から判定する純関数（ADR 0036 の決定 3）。Win32 の列挙は adapters の
 * font_bundle が行い、見つけた数と登録できた数だけをここへ渡す。確保失敗（NO_MEMORY）は数では
 * 表せないので adapters が直接返し、ここからは返らない。 */
#ifndef NENEFOLIO_FONT_BUNDLE_VERDICT_H
#define NENEFOLIO_FONT_BUNDLE_VERDICT_H

#include "font_bundle_outcome.h"

#include <stddef.h>

/* found は見つけた *.otf / *.ttf の数、registered はそのうち登録できた数（registered <= found）。
 * 0 件なら MISSING、1 つでも欠けたら PARTIAL、すべてなら READY。 */
[[nodiscard]] enum font_bundle_outcome font_bundle_verdict_of(size_t found, size_t registered);

#endif
