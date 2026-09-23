#include "icon_paint.h"

#include "gdiplus_flat.h"

#include <stddef.h>
#include <windows.h>

/* 形は画面案 C（Issue #88 で hide が選んだ「面で塗る・角丸の板」）の SVG を 24 の viewBox の
 * まま写したもの。相対座標は実行時ではなくここで絶対へ畳んである（ADR 0033 の決定 3）。
 * 画面案の値は 1 つも変えていない（歯車の外周が中心から +0.05 / +0.10 単位ずれているのも
 * 画面案そのものの性質）。突き合わせは out/design/2026-09-23/icon-ui-probe/geometry.py。 */
constexpr float icon_view_box = 24.0f;

/* 写しの規則 (1): <path> の M/L/l は絶対座標の点列にして、続く 2 点ごとに線を足し、
 * z は図形を閉じる（最後の点から最初の点への線は GDI+ が引く）。
 * 閉じる: M8.1 6.7L12 10.6l3.9-3.9 1.4 1.4-3.9 3.9 3.9 3.9-1.4 1.4-3.9-3.9-3.9 3.9-1.4-1.4
 *         3.9-3.9-3.9-3.9z（12 点）。腕の太さは 1.4 * sqrt(2) = 1.98 単位（決定 7）。 */
static const struct
{
    float x;
    float y;
} close_points[] = {
    {8.1f, 6.7f},   {12.0f, 10.6f}, {15.9f, 6.7f}, {17.3f, 8.1f}, {13.4f, 12.0f}, {17.3f, 15.9f},
    {15.9f, 17.3f}, {12.0f, 13.4f}, {8.1f, 17.3f}, {6.7f, 15.9f}, {10.6f, 12.0f}, {6.7f, 8.1f},
};

/* 歯車の外周（32 点）。fill-rule="evenodd" なので穴は FillMode Alternate で抜く。 */
static const struct
{
    float x;
    float y;
} settings_points[] = {
    {10.6f, 3.0f},  {13.4f, 3.0f},  {13.9f, 5.2f},  {15.5f, 5.9f},  {17.4f, 4.7f},  {19.4f, 6.7f},
    {18.2f, 8.6f},  {18.9f, 10.2f}, {21.1f, 10.7f}, {21.1f, 13.5f}, {18.9f, 14.0f}, {18.2f, 15.6f},
    {19.4f, 17.5f}, {17.4f, 19.5f}, {15.5f, 18.3f}, {13.9f, 19.0f}, {13.4f, 21.2f}, {10.6f, 21.2f},
    {10.1f, 19.0f}, {8.5f, 18.3f},  {6.6f, 19.5f},  {4.6f, 17.5f},  {5.8f, 15.6f},  {5.1f, 14.0f},
    {3.0f, 13.4f},  {3.0f, 10.6f},  {5.2f, 10.1f},  {5.9f, 8.5f},   {4.7f, 6.6f},   {6.7f, 4.6f},
    {8.6f, 5.8f},   {10.2f, 5.1f},
};

/* 写しの規則 (3): 歯車の穴 M12 9a3 3 0 100 6a3 3 0 000-6z は中心 (12,12)・半径 3 の円。 */
constexpr float settings_hole_origin = 9.0f;
constexpr float settings_hole_size = 6.0f;

/* 警告の印（ADR 0035 の補正 13）。外周の三角・縦棒の穴・点の穴の 3 図形を同じパスに入れ、
 * 歯車と同じく Alternate で穴を抜く（穴には箱の地が透ける）。値は補正 13 の目安のまま。 */
static const struct
{
    float x;
    float y;
} warning_points[] = {
    {12.0f, 2.5f},
    {22.5f, 20.5f},
    {1.5f, 20.5f},
};

/* 「!」の縦棒の穴 x 10.9〜13.1 / y 8〜14.5 を時計回りの 4 点で持つ。 */
static const struct
{
    float x;
    float y;
} warning_stem_points[] = {
    {10.9f, 8.0f},
    {13.1f, 8.0f},
    {13.1f, 14.5f},
    {10.9f, 14.5f},
};

/* 「!」の点の穴は中心 (12, 17.6)・半径 1.35 の円。 */
constexpr float warning_dot_left = 12.0f - 1.35f;
constexpr float warning_dot_top = 17.6f - 1.35f;
constexpr float warning_dot_size = 2.0f * 1.35f;

/* 写しの規則 (2): <rect x y width height rx> の表。引数を 4 つに収めるため（C-012）、
 * 写しは 1 本の関数に保って添字で選ぶ。 */
constexpr size_t round_rect_fold_bar = 0;
constexpr size_t round_rect_fold_stem = 1;
constexpr size_t round_rect_selection = 2;
static const struct
{
    float x;
    float y;
    float width;
    float height;
    float radius;
} round_rects[] = {
    /* 折畳の横棒 <rect x="6" y="11" width="12" height="2" rx="1"/>。
     * 高さ 2 単位なので箱が 20px 未満だと塗りが灰色になる（決定 6 で箱を 24px に揃えた）。 */
    [round_rect_fold_bar] = {6.0f, 11.0f, 12.0f, 2.0f, 1.0f},
    /* + の縦棒 <rect x="11" y="6" width="2" height="12" rx="1"/>。横棒と重ねるので Winding。 */
    [round_rect_fold_stem] = {11.0f, 6.0f, 2.0f, 12.0f, 1.0f},
    /* 選択の印 <rect x="8" y="8" width="8" height="8" rx="2"/>。ink は箱の左端から 8 単位。 */
    [round_rect_selection] = {8.0f, 8.0f, 8.0f, 8.0f, 2.0f},
};

/* 角丸の 1 つの角ぶんの四分円。左上 180 度 -> 右上 270 度 -> 右下 0 度 -> 左下 90 度の順に
 * 90 度ずつ足すと、弧と弧のあいだの直線は GDI+ が継ぐ。 */
constexpr float quarter_turn = 90.0f;

/* 最初の失敗を覚える。描くのをやめる判断は 1 か所だけで行う。 */
static int first_failure(int status, int step)
{
    return status != gdiplus_ok ? status : step;
}

/* 点の表を線でつないで閉じる。表は 2 つとも絶対座標なので畳み直さない。 */
static int add_close_path(struct GpPath *_Nonnull path)
{
    constexpr size_t count = sizeof close_points / sizeof close_points[0];
    int status = gdiplus_ok;
    for (size_t index = 0; index + 1 < count; ++index)
    {
        status = first_failure(
            status, GdipAddPathLine(path, close_points[index].x, close_points[index].y,
                                    close_points[index + 1].x, close_points[index + 1].y));
    }
    return first_failure(status, GdipClosePathFigure(path));
}

/* 歯車は外周の 32 点と、中心の円の穴の 2 図形。穴は Alternate で抜ける。 */
static int add_settings_path(struct GpPath *_Nonnull path)
{
    constexpr size_t count = sizeof settings_points / sizeof settings_points[0];
    int status = gdiplus_ok;
    for (size_t index = 0; index + 1 < count; ++index)
    {
        status = first_failure(
            status, GdipAddPathLine(path, settings_points[index].x, settings_points[index].y,
                                    settings_points[index + 1].x, settings_points[index + 1].y));
    }
    status = first_failure(status, GdipClosePathFigure(path));
    return first_failure(status,
                         GdipAddPathEllipse(path, settings_hole_origin, settings_hole_origin,
                                            settings_hole_size, settings_hole_size));
}

/* 警告の印は外周の三角・縦棒の穴・点の穴の 3 図形。穴は Alternate で抜ける。 */
static int add_warning_path(struct GpPath *_Nonnull path)
{
    constexpr size_t outline = sizeof warning_points / sizeof warning_points[0];
    constexpr size_t stem = sizeof warning_stem_points / sizeof warning_stem_points[0];
    int status = gdiplus_ok;
    for (size_t index = 0; index + 1 < outline; ++index)
    {
        status = first_failure(
            status, GdipAddPathLine(path, warning_points[index].x, warning_points[index].y,
                                    warning_points[index + 1].x, warning_points[index + 1].y));
    }
    status = first_failure(status, GdipClosePathFigure(path));
    for (size_t index = 0; index + 1 < stem; ++index)
    {
        status = first_failure(status, GdipAddPathLine(path, warning_stem_points[index].x,
                                                       warning_stem_points[index].y,
                                                       warning_stem_points[index + 1].x,
                                                       warning_stem_points[index + 1].y));
    }
    status = first_failure(status, GdipClosePathFigure(path));
    return first_failure(status, GdipAddPathEllipse(path, warning_dot_left, warning_dot_top,
                                                    warning_dot_size, warning_dot_size));
}

/* 写しの規則 (2)。h == 2rx の板では縦の直線の長さが 0 になり、両端が半円の板になる。 */
static int add_round_rect(struct GpPath *_Nonnull path, size_t which)
{
    float diameter = round_rects[which].radius * 2.0f;
    float left = round_rects[which].x;
    float top = round_rects[which].y;
    float right = left + round_rects[which].width - diameter;
    float bottom = top + round_rects[which].height - diameter;
    const struct
    {
        float x;
        float y;
        float start;
    } corners[] = {
        {left, top, 180.0f}, {right, top, 270.0f}, {right, bottom, 0.0f}, {left, bottom, 90.0f}};
    int status = gdiplus_ok;
    for (size_t index = 0; index < sizeof corners / sizeof corners[0]; ++index)
    {
        status =
            first_failure(status, GdipAddPathArc(path, corners[index].x, corners[index].y, diameter,
                                                 diameter, corners[index].start, quarter_turn));
    }
    return first_failure(status, GdipClosePathFigure(path));
}

/* + は横棒と縦棒の 2 枚。SVG の既定の nonzero と同じく Winding で重なりを埋める。 */
static int add_fold_expand_path(struct GpPath *_Nonnull path)
{
    int status = add_round_rect(path, round_rect_fold_bar);
    return first_failure(status, add_round_rect(path, round_rect_fold_stem));
}

/* 形ごとに割る（全値の switch・default は書かない・C-002）。 */
static int build_path(struct GpPath *_Nonnull path, enum icon_paint_kind kind)
{
    switch (kind)
    {
    case ICON_PAINT_CLOSE:
        return add_close_path(path);
    case ICON_PAINT_SETTINGS:
        return add_settings_path(path);
    case ICON_PAINT_FOLD_COLLAPSE:
        return add_round_rect(path, round_rect_fold_bar);
    case ICON_PAINT_FOLD_EXPAND:
        return add_fold_expand_path(path);
    case ICON_PAINT_SELECTION:
        return add_round_rect(path, round_rect_selection);
    case ICON_PAINT_WARNING:
        return add_warning_path(path);
    }
    return gdiplus_ok;
}

/* FillMode はパスを作るときの引数だけで決める（二重に持たない）。 */
static int fill_mode_for(enum icon_paint_kind kind)
{
    switch (kind)
    {
    case ICON_PAINT_CLOSE:
    case ICON_PAINT_FOLD_COLLAPSE:
    case ICON_PAINT_FOLD_EXPAND:
    case ICON_PAINT_SELECTION:
        return gdiplus_fill_winding;
    case ICON_PAINT_SETTINGS:
    case ICON_PAINT_WARNING:
        return gdiplus_fill_alternate;
    }
    return gdiplus_fill_winding;
}

/* COLORREF は 0x00BBGGRR、GDI+ は 0xAARRGGBB。そのまま渡すと赤と青が入れ替わる。 */
static DWORD to_argb(COLORREF color)
{
    return 0xFF000000u | ((DWORD)GetRValue(color) << 16) | ((DWORD)GetGValue(color) << 8) |
           (DWORD)GetBValue(color);
}

/* 写しの規則 (4): 24 の viewBox を箱へ写して塗る。倍率は箱の幅から 1 行で出る。 */
static void fill_in_box(struct GpGraphics *_Nonnull graphics, struct GpBrush *_Nonnull brush,
                        struct GpPath *_Nonnull path, RECT bounds)
{
    float ratio = (float)(bounds.right - bounds.left) / icon_view_box;
    int status = GdipResetWorldTransform(graphics);
    status = first_failure(status,
                           GdipTranslateWorldTransform(graphics, (float)bounds.left,
                                                       (float)bounds.top, gdiplus_matrix_prepend));
    status = first_failure(status,
                           GdipScaleWorldTransform(graphics, ratio, ratio, gdiplus_matrix_prepend));
    if (status == gdiplus_ok)
    {
        GdipFillPath(graphics, brush, path);
    }
    GdipResetWorldTransform(graphics);
}

/* パスと刷毛を作って塗る。どこかが 0 以外を返したら何も描かずに片づける。 */
static void paint_shape(struct GpGraphics *_Nonnull graphics, RECT bounds,
                        enum icon_paint_kind kind, COLORREF color)
{
    struct GpPath *_Nullable path = nullptr;
    struct GpBrush *_Nullable brush = nullptr;
    int status = GdipCreatePath(fill_mode_for(kind), &path);
    if (path != nullptr)
    {
        status = first_failure(status, build_path(path, kind));
        status = first_failure(status, GdipCreateSolidFill(to_argb(color), &brush));
        if (status == gdiplus_ok && brush != nullptr)
        {
            fill_in_box(graphics, brush, path, bounds);
        }
    }
    if (brush != nullptr)
    {
        GdipDeleteBrush(brush);
    }
    if (path != nullptr)
    {
        GdipDeletePath(path);
    }
}

void icon_paint_fill(HDC device, RECT bounds, enum icon_paint_kind kind, COLORREF color)
{
    /* graphics は描くたびに作って捨てる（1 回 0.022ms・使い回しの利得は 0.006ms）。
     * SetViewportOrgEx とクリップは HDC から引き継ぐので、呼ぶ側は client 座標のままでよい。 */
    struct GpGraphics *_Nullable graphics = nullptr;
    if (GdipCreateFromHDC(device, &graphics) != gdiplus_ok || graphics == nullptr)
    {
        return;
    }
    int status = GdipSetSmoothingMode(graphics, gdiplus_smoothing_antialias);
    status = first_failure(status, GdipSetPixelOffsetMode(graphics, gdiplus_pixel_offset_half));
    if (status == gdiplus_ok)
    {
        paint_shape(graphics, bounds, kind, color);
    }
    GdipDeleteGraphics(graphics);
}
