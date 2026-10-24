/* 折返し一回の入力。文字の所有者は呼出し元のUTF16バッファのまま。 */
#ifndef NENEFOLIO_TEXT_WRAP_H
#define NENEFOLIO_TEXT_WRAP_H

#include <uchar.h>
#include <windows.h>

struct text_wrap
{
    HDC _Nonnull device;
    const char16_t *_Nonnull units;
    int count;
    RECT bounds;
    int tracking;
    bool paint;
    int *_Nullable maximum_line_height;
};

#endif
