/* 同梱の書体の登録（ADR 0036 の決定 3）。exe と同じディレクトリの fonts\ にある *.otf / *.ttf を
 * AddFontResourceExW(FR_PRIVATE) で登録し、destroy で RemoveFontResourceExW する唯一の区画。
 * FR_PRIVATE なので OS にも他のプロセスにも入らない。所有者は exe の入口で、窓を作る前に登録し、
 * メッセージループのあとに解除する（ARC-004）。 */
#ifndef NENEFOLIO_FONT_BUNDLE_H
#define NENEFOLIO_FONT_BUNDLE_H

#include "font_bundle_outcome.h"

struct font_bundle;

/* READY / MISSING / PARTIAL では *out に解除の相手を渡す（MISSING なら中身は空）。
 * NO_MEMORY では登録した分を戻してから *out を nullptr にする。 */
[[nodiscard]] enum font_bundle_outcome
font_bundle_register(struct font_bundle *_Nullable *_Nonnull out);
void font_bundle_destroy(struct font_bundle *_Nullable bundle);

#endif
