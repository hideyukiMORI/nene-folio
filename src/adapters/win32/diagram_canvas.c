#include "diagram_canvas.h"
#include "diagram_native_shape.h"
#include "diagram_style.h"
#include "utf16_text.h"
#include <stdlib.h>
#include <string.h>
#include <windows.h>
constexpr int scale = 2;
constexpr size_t pixel_limit = 4 * 1024 * 1024;
struct diagram_canvas
{
    HDC _Nullable dc;
    HFONT _Nullable body_font;
    HFONT _Nullable caption_font;
    HGDIOBJ _Nullable previous_font;
    HBITMAP _Nullable bitmap;
    HGDIOBJ _Nullable previous_bitmap;
    unsigned char *_Nullable pixels;
    unsigned char *_Nullable reduced;
    struct diagram_palette palette;
    struct utf16_text *_Nullable labels[diagram_text_limit];
    struct diagram_label_metrics sizes[diagram_text_limit];
    size_t label_count;
};
static COLORREF color(struct rgb_color value)
{
    return RGB(value.red, value.green, value.blue);
}
static COLORREF ink(const struct diagram_canvas *_Nonnull canvas, enum diagram_ink value)
{
    return color(diagram_palette_ink(&canvas->palette, value));
}
static RECT rectangle(struct diagram_rect bounds)
{
    return (RECT){bounds.x * scale, bounds.y * scale, (bounds.x + bounds.width) * scale,
                  (bounds.y + bounds.height) * scale};
}
static enum image_outcome create_font(const char *_Nonnull face, int pixels,
                                      HFONT _Nullable *_Nonnull font)
{
    char16_t units[LF_FACESIZE];
    size_t count = 0;
    if (utf16_text_fill(face, units, LF_FACESIZE, &count) != UTF16_TEXT_FILL_READY)
        return IMAGE_UNSUPPORTED;
    *font = CreateFontW(-pixels * scale, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH,
                        (const wchar_t *)units);
    return *font == nullptr ? IMAGE_OUT_OF_MEMORY : IMAGE_READY;
}
static enum image_outcome prepare_fonts(struct diagram_canvas *_Nonnull canvas,
                                        const char *_Nonnull face)
{
    enum image_outcome result = create_font(face, diagram_body_font, &canvas->body_font);
    if (result != IMAGE_READY)
        return result;
    result = create_font(face, diagram_caption_font, &canvas->caption_font);
    if (result != IMAGE_READY)
        return result;
    canvas->previous_font = SelectObject(canvas->dc, canvas->body_font);
    if (canvas->previous_font == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    SetBkMode(canvas->dc, TRANSPARENT);
    return IMAGE_READY;
}
enum image_outcome diagram_canvas_create(const struct diagram_request *_Nonnull request,
                                         struct diagram_canvas *_Nullable *_Nonnull out)
{
    struct diagram_canvas *_Nullable canvas = calloc(1, sizeof *canvas);
    if (canvas == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    canvas->palette = request->palette;
    canvas->dc = CreateCompatibleDC(nullptr);
    enum image_outcome outcome =
        canvas->dc == nullptr ? IMAGE_OUT_OF_MEMORY : prepare_fonts(canvas, request->face);
    if (outcome != IMAGE_READY)
    {
        diagram_canvas_destroy(canvas);
        return outcome;
    }
    *out = canvas;
    return IMAGE_READY;
}
static enum image_outcome measure_role(struct diagram_canvas *_Nonnull canvas,
                                       const struct utf16_text *_Nonnull label, HFONT font,
                                       struct diagram_size *_Nonnull size)
{
    SelectObject(canvas->dc, font);
    size_t length = utf16_text_length(label);
    RECT bounds = {0, 0, diagram_text_wrap * scale, 0};
    if (length == 0)
    {
        TEXTMETRICW metric;
        if (!GetTextMetricsW(canvas->dc, &metric))
            return IMAGE_FAILED;
        *size = (struct diagram_size){0, (metric.tmHeight + scale - 1) / scale};
        return IMAGE_READY;
    }
    int height = DrawTextW(canvas->dc, (const wchar_t *)utf16_text_units(label), (int)length,
                           &bounds, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL);
    if (height <= 0)
        return IMAGE_FAILED;
    *size = (struct diagram_size){(bounds.right + scale - 1) / scale,
                                  (bounds.bottom + scale - 1) / scale};
    return IMAGE_READY;
}
static enum image_outcome measure_label(struct diagram_canvas *_Nonnull canvas,
                                        const struct diagram_model *_Nonnull model, size_t index)
{
    const char *source = diagram_model_text_at(model, index);
    if (utf16_text_create(source, strlen(source), &canvas->labels[index]) != UTF16_TEXT_CONVERTED)
        return IMAGE_OUT_OF_MEMORY;
    canvas->label_count = index + 1;
    const struct utf16_text *label = canvas->labels[index];
    enum image_outcome result =
        measure_role(canvas, label, canvas->body_font, &canvas->sizes[index].body);
    return result == IMAGE_READY
               ? measure_role(canvas, label, canvas->caption_font, &canvas->sizes[index].caption)
               : result;
}
enum image_outcome diagram_canvas_measure(struct diagram_canvas *_Nonnull canvas,
                                          const struct diagram_model *_Nonnull model,
                                          const struct image_work *_Nonnull work,
                                          struct diagram_metrics *_Nullable *_Nonnull out)
{
    size_t count = diagram_model_text_count(model);
    for (size_t index = 0; index < count; ++index)
    {
        if (work->cancelled(work))
            return IMAGE_CANCELLED;
        enum image_outcome result = measure_label(canvas, model, index);
        if (result != IMAGE_READY)
            return result;
    }
    enum diagram_outcome outcome = diagram_metrics_create(canvas->sizes, count, out);
    switch (outcome)
    {
    case DIAGRAM_ACCEPTED:
        return IMAGE_READY;
    case DIAGRAM_INVALID:
        return IMAGE_FAILED;
    case DIAGRAM_UNSUPPORTED:
        return IMAGE_UNSUPPORTED;
    case DIAGRAM_TOO_LARGE:
        return IMAGE_TOO_LARGE;
    case DIAGRAM_OUT_OF_MEMORY:
        return IMAGE_OUT_OF_MEMORY;
    }
    return IMAGE_FAILED;
}
static enum image_outcome prepare_bitmap(struct diagram_canvas *_Nonnull canvas,
                                         struct diagram_size size)
{
    if (size.width < 1 || size.height < 1 || size.width > (int)image_bitmap_edge_limit ||
        size.height > (int)image_bitmap_edge_limit ||
        (size_t)size.width * (size_t)size.height > pixel_limit)
        return IMAGE_TOO_LARGE;
    BITMAPINFO info = {.bmiHeader = {.biSize = sizeof(BITMAPINFOHEADER),
                                     .biWidth = size.width * scale,
                                     .biHeight = -size.height * scale,
                                     .biPlanes = 1,
                                     .biBitCount = 32,
                                     .biCompression = BI_RGB}};
    void *_Nullable bits = nullptr;
    canvas->bitmap = CreateDIBSection(canvas->dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (canvas->bitmap == nullptr || bits == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    canvas->pixels = bits;
    canvas->previous_bitmap = SelectObject(canvas->dc, canvas->bitmap);
    canvas->reduced = malloc((size_t)size.width * (size_t)size.height * 4);
    if (canvas->reduced == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    RECT background = {0, 0, size.width * scale, size.height * scale};
    SetDCBrushColor(canvas->dc, color(canvas->palette.background));
    FillRect(canvas->dc, &background, (HBRUSH)GetStockObject(DC_BRUSH));
    return IMAGE_READY;
}
static DWORD stroke_style(enum diagram_stroke stroke)
{
    switch (stroke)
    {
    case DIAGRAM_SOLID:
    case DIAGRAM_THICK:
        return PS_SOLID;
    case DIAGRAM_DASHED:
        return PS_USERSTYLE;
    case DIAGRAM_INVISIBLE:
        return PS_NULL;
    }
    return PS_SOLID;
}
static HPEN _Nullable create_pen(const struct diagram_canvas *_Nonnull canvas,
                                 const struct diagram_primitive *_Nonnull primitive)
{
    LOGBRUSH brush = {.lbStyle = BS_SOLID, .lbColor = ink(canvas, primitive->ink)};
    DWORD width =
        (DWORD)(primitive->stroke == DIAGRAM_THICK ? diagram_thick_width : diagram_stroke_width) *
        scale;
    DWORD dashes[2] = {diagram_dash_length * scale, diagram_dash_gap * scale};
    bool dashed = primitive->stroke == DIAGRAM_DASHED;
    return ExtCreatePen(PS_GEOMETRIC | PS_ENDCAP_FLAT | PS_JOIN_ROUND |
                            stroke_style(primitive->stroke),
                        width, &brush, dashed ? 2 : 0, dashed ? dashes : nullptr);
}
static void draw_path(struct diagram_canvas *_Nonnull canvas,
                      const struct diagram_primitive *_Nonnull primitive)
{
    POINT points[8];
    for (size_t index = 0; index < primitive->count; ++index)
        points[index] =
            (POINT){primitive->points[index].x * scale, primitive->points[index].y * scale};
    if (primitive->count < 2)
        return;
    Polyline(canvas->dc, points, (int)primitive->count);
    SelectObject(canvas->dc, GetStockObject(DC_BRUSH));
    SetDCBrushColor(canvas->dc, ink(canvas, primitive->ink));
    diagram_native_shape_arrows(canvas->dc, primitive, scale);
}
static struct diagram_size select_text_role(struct diagram_canvas *_Nonnull canvas,
                                            const struct diagram_primitive *_Nonnull primitive)
{
    switch (primitive->role)
    {
    case DIAGRAM_TEXT_BODY:
        SelectObject(canvas->dc, canvas->body_font);
        return canvas->sizes[primitive->text].body;
    case DIAGRAM_TEXT_CAPTION:
        SelectObject(canvas->dc, canvas->caption_font);
        return canvas->sizes[primitive->text].caption;
    }
    return (struct diagram_size){0, 0};
}
static UINT text_alignment(enum diagram_text_alignment alignment)
{
    switch (alignment)
    {
    case DIAGRAM_TEXT_CENTER:
        return DT_CENTER;
    case DIAGRAM_TEXT_LEFT:
        return DT_LEFT;
    }
    return DT_LEFT;
}
static int text_left(const struct diagram_primitive *_Nonnull primitive, RECT bounds, int width)
{
    switch (primitive->alignment)
    {
    case DIAGRAM_TEXT_CENTER:
        return bounds.left + ((bounds.right - bounds.left) - width) / 2;
    case DIAGRAM_TEXT_LEFT:
        return bounds.left + primitive->text_inset * scale;
    }
    return bounds.left;
}
static enum image_outcome draw_text(struct diagram_canvas *_Nonnull canvas,
                                    const struct diagram_primitive *_Nonnull primitive)
{
    if (primitive->text >= canvas->label_count)
        return IMAGE_FAILED;
    RECT bounds = rectangle(primitive->bounds);
    if (primitive->fill != DIAGRAM_INK_NONE)
    {
        SetDCBrushColor(canvas->dc, ink(canvas, primitive->fill));
        FillRect(canvas->dc, &bounds, (HBRUSH)GetStockObject(DC_BRUSH));
    }
    const struct utf16_text *label = canvas->labels[primitive->text];
    size_t length = utf16_text_length(label);
    if (length == 0)
        return IMAGE_READY;
    SetTextColor(canvas->dc, ink(canvas, primitive->ink));
    struct diagram_size size = select_text_role(canvas, primitive);
    int height = size.height * scale;
    bounds.top += ((bounds.bottom - bounds.top) - height) / 2;
    bounds.left = text_left(primitive, bounds, size.width * scale);
    bounds.right = bounds.left + size.width * scale;
    return DrawTextW(canvas->dc, (const wchar_t *)utf16_text_units(label), (int)length, &bounds,
                     text_alignment(primitive->alignment) | DT_WORDBREAK | DT_NOPREFIX |
                         DT_EDITCONTROL) > 0
               ? IMAGE_READY
               : IMAGE_FAILED;
}
static enum image_outcome draw_primitive(struct diagram_canvas *_Nonnull canvas,
                                         const struct diagram_primitive *_Nonnull primitive)
{
    if (primitive->kind == DIAGRAM_PRIMITIVE_TEXT)
        return draw_text(canvas, primitive);
    HPEN _Nullable pen = create_pen(canvas, primitive);
    if (pen == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    HGDIOBJ previous_pen = SelectObject(canvas->dc, pen);
    HGDIOBJ previous_brush = SelectObject(
        canvas->dc, GetStockObject(primitive->fill == DIAGRAM_INK_NONE ? HOLLOW_BRUSH : DC_BRUSH));
    SetDCBrushColor(canvas->dc, ink(canvas, primitive->fill));
    switch (primitive->kind)
    {
    case DIAGRAM_PRIMITIVE_PATH:
        draw_path(canvas, primitive);
        break;
    case DIAGRAM_PRIMITIVE_SHAPE:
        diagram_native_shape_draw(canvas->dc, primitive, rectangle(primitive->bounds), scale);
        break;
    case DIAGRAM_PRIMITIVE_TEXT:
        break;
    }
    SelectObject(canvas->dc, previous_brush);
    SelectObject(canvas->dc, previous_pen);
    DeleteObject(pen);
    return IMAGE_READY;
}
static void reduce_row(struct diagram_canvas *_Nonnull canvas, struct diagram_size size, int row)
{
    size_t source_stride = (size_t)size.width * scale * 4;
    const unsigned char *first = canvas->pixels + (size_t)row * scale * source_stride;
    const unsigned char *second = first + source_stride;
    unsigned char *target = canvas->reduced + (size_t)row * (size_t)size.width * 4;
    for (int x = 0; x < size.width; ++x)
    {
        size_t source = (size_t)x * scale * 4;
        size_t destination = (size_t)x * 4;
        for (size_t part = 0; part < 3; ++part)
            target[destination + part] =
                (unsigned char)((first[source + part] + first[source + 4 + part] +
                                 second[source + part] + second[source + 4 + part] + 2) /
                                4);
        target[destination + 3] = 255;
    }
}
enum image_outcome diagram_canvas_draw(struct diagram_canvas *_Nonnull canvas,
                                       const struct diagram_scene *_Nonnull scene,
                                       const struct image_work *_Nonnull work,
                                       struct image_bitmap *_Nullable *_Nonnull out)
{
    struct diagram_size size = diagram_scene_size(scene);
    enum image_outcome result = prepare_bitmap(canvas, size);
    if (result != IMAGE_READY)
        return result;
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        if (work->cancelled(work))
            return IMAGE_CANCELLED;
        result = draw_primitive(canvas, diagram_scene_at(scene, index));
        if (result != IMAGE_READY)
            return result;
    }
    GdiFlush();
    for (int row = 0; row < size.height; ++row)
    {
        if (row % 128 == 0 && work->cancelled(work))
            return IMAGE_CANCELLED;
        reduce_row(canvas, size, row);
    }
    return image_bitmap_create(
        canvas->reduced, (struct image_size){(uint32_t)size.width, (uint32_t)size.height}, out);
}
void diagram_canvas_destroy(struct diagram_canvas *_Nullable canvas)
{
    if (canvas == nullptr)
        return;
    for (size_t index = 0; index < canvas->label_count; ++index)
        utf16_text_destroy(canvas->labels[index]);
    if (canvas->previous_bitmap != nullptr)
        SelectObject(canvas->dc, canvas->previous_bitmap);
    if (canvas->bitmap != nullptr)
        DeleteObject(canvas->bitmap);
    if (canvas->previous_font != nullptr)
        SelectObject(canvas->dc, canvas->previous_font);
    if (canvas->body_font != nullptr)
        DeleteObject(canvas->body_font);
    if (canvas->caption_font != nullptr)
        DeleteObject(canvas->caption_font);
    if (canvas->dc != nullptr)
        DeleteDC(canvas->dc);
    free(canvas->reduced);
    free(canvas);
}
