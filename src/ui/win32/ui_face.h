/* 言語ごとの書体名を Win32 の API へ渡せる形で取り出す唯一の場所（ADR 0032 の決定 4）。
 * 値の正本は core の ui_font で、ここは UTF-16 へ広げて**実在を確かめる**だけである
 * （CreateFontW は無い face 名でも失敗せず、GDI が勝手に別の face を選ぶ。
 * 存在確認は EnumFontFamiliesExW でしかできない・実測）。
 * ドロワーの書体・名前入力面の書体・本文の RichEdit の既定書式の 3 つがここを通る。 */
#ifndef NENEFOLIO_UI_FACE_H
#define NENEFOLIO_UI_FACE_H

#include "folio_language.h"

#include <windows.h>

/* out は終端を含めて LF_FACESIZE 単位の入れ物。無い face は日本語の face に落とす。 */
void ui_face_for(enum folio_language language, wchar_t *_Nonnull out);

#endif
