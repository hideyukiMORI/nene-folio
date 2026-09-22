/* 閉じる・設定・折畳・選択の印を、既存の DC へ GDI+ で塗る 1 本（ADR 0033 の決定 3 / 5）。
 * 形は 24 の viewBox の点の表で icon_paint.c が持ち、bounds へ等倍で写す。
 * 描画関数なので結果を返さない。GDI+ が失敗したら何も描かずに戻る（現行の draw_* と同じ）。 */
#ifndef NENEFOLIO_ICON_PAINT_H
#define NENEFOLIO_ICON_PAINT_H

#include "icon_paint_kind.h"

#include <windows.h>

void icon_paint_fill(HDC device, RECT bounds, enum icon_paint_kind kind, COLORREF color);

#endif
