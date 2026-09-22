/* OS 同梱の GDI+（gdiplus.dll）のフラット API を C から呼ぶための宣言（ADR 0033 の決定 2）。
 *
 * 根拠: Windows SDK の gdiplus*.h は C++ 専用で、C からは 1 組も通らない（6 組を実測・
 * out/design/2026-09-23/icon-probe/ の §1-1）。gdiplusgpstubs.h が `class GpGraphics {};` の
 * ような C++ のクラスでできていて、`#ifdef __cplusplus` の C 用の枝がどのファイルにも無い。
 * そこで windows.h だけを読み、使う関数を SDK の gdiplusflat.h の署名の字面で宣言する。
 * gdiplus.lib の名前は extern "C"（無修飾）なので、C から素直に解決する（実測）。
 *
 * このヘッダは型を 1 つも定義しない（CNF-002）。GDI+ の対象は不完全型の宣言だけで受け、
 * 座標は float、Status / FillMode / SmoothingMode / PixelOffsetMode / MatrixOrder は
 * 使う値だけを constexpr の定数で持つ（typedef は置かない）。
 * GdiplusStartup へ渡す束だけは完全型が要るので gdiplus_startup_input.h に分けてある。 */
#ifndef NENEFOLIO_GDIPLUS_FLAT_H
#define NENEFOLIO_GDIPLUS_FLAT_H

#include "gdiplus_startup_input.h"

#include <windows.h>

/* GDI+ の不透明な対象。本体は gdiplus.dll の中にしかない（C-003 / C-007 と同じ形）。 */
struct GpGraphics;
struct GpPath;
struct GpBrush;
/* GdiplusStartup の第 3 引数（SDK の GdiplusStartupOutput）。背景スレッドを既定のままにするので
 * nullptr しか渡さない。定義が要らないので不完全型のまま置く。 */
struct gdiplus_startup_output;

/* Status（gdiplusenums.h）。使うのは Ok だけで、0 以外はまとめて「描かない」にする。 */
constexpr int gdiplus_ok = 0;
/* FillMode（gdiplusenums.h）。歯車の穴は Alternate、+ の重なりは Winding。 */
constexpr int gdiplus_fill_alternate = 0;
constexpr int gdiplus_fill_winding = 1;
/* SmoothingMode / PixelOffsetMode（gdiplusenums.h）。この 2 つで中間色が出る（実測）。 */
constexpr int gdiplus_smoothing_antialias = 4;
constexpr int gdiplus_pixel_offset_half = 4;
/* MatrixOrder（gdiplusenums.h）。箱への写像は Translate → Scale の順に Prepend で積む。 */
constexpr int gdiplus_matrix_prepend = 0;
/* GdiplusStartupInput::version の唯一の値。 */
constexpr UINT32 gdiplus_version_1 = 1;

int WINAPI GdiplusStartup(ULONG_PTR *token, const struct gdiplus_startup_input *input,
                          struct gdiplus_startup_output *output);
void WINAPI GdiplusShutdown(ULONG_PTR token);

int WINAPI GdipCreateFromHDC(HDC device, struct GpGraphics **graphics);
int WINAPI GdipDeleteGraphics(struct GpGraphics *graphics);
int WINAPI GdipSetSmoothingMode(struct GpGraphics *graphics, int mode);
int WINAPI GdipSetPixelOffsetMode(struct GpGraphics *graphics, int mode);
int WINAPI GdipTranslateWorldTransform(struct GpGraphics *graphics, float dx, float dy, int order);
int WINAPI GdipScaleWorldTransform(struct GpGraphics *graphics, float sx, float sy, int order);
int WINAPI GdipResetWorldTransform(struct GpGraphics *graphics);

int WINAPI GdipCreatePath(int fill_mode, struct GpPath **path);
int WINAPI GdipDeletePath(struct GpPath *path);
int WINAPI GdipAddPathLine(struct GpPath *path, float x1, float y1, float x2, float y2);
int WINAPI GdipAddPathArc(struct GpPath *path, float x, float y, float width, float height,
                          float start, float sweep);
int WINAPI GdipAddPathEllipse(struct GpPath *path, float x, float y, float width, float height);
int WINAPI GdipClosePathFigure(struct GpPath *path);

int WINAPI GdipCreateSolidFill(DWORD color, struct GpBrush **brush);
int WINAPI GdipDeleteBrush(struct GpBrush *brush);
int WINAPI GdipFillPath(struct GpGraphics *graphics, struct GpBrush *brush, struct GpPath *path);

#endif
