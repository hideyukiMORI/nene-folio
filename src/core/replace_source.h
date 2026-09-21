/* 下見が写し取るもの（ADR 0028 の決定 6）。本文・置換文字列・宛先（カテゴリ名とノート名）を
 * 束ねる。どれも呼び出しの間だけ借り、replace_preview_create が複製して所有する。
 * 宛先を**名前**で持つのは note_corpus と同じ理由で、並び替えでは何も動かないからである。
 * 無題の文書はノート名が空文字列で、同じカテゴリの中で 1 つしか無いので区別が付く。
 * 全メンバーが独立に妥当なので完全型で公開する（C-003 の例外）。 */
#ifndef NENEFOLIO_REPLACE_SOURCE_H
#define NENEFOLIO_REPLACE_SOURCE_H

#include <stddef.h>
#include <uchar.h>

struct replace_source
{
    const char16_t *_Nonnull text;
    size_t length;
    const char16_t *_Nonnull replacement;
    size_t replacement_length;
    const char *_Nonnull category; /* 終端付き UTF-8 */
    const char *_Nonnull note;     /* 終端付き UTF-8。無題は空文字列 */
};

#endif
