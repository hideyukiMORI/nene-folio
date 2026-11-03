#ifndef NENEFOLIO_DIAGRAM_NATIVE_SHAPE_H
#define NENEFOLIO_DIAGRAM_NATIVE_SHAPE_H
#include "diagram_primitive.h"
#include <windows.h>
/* The canvas owns GDI resources; bounds are device pixels, radius is logical. */
void diagram_native_shape_draw(HDC _Nonnull dc, const struct diagram_primitive *_Nonnull primitive,
                               RECT bounds, int scale);
void diagram_native_shape_arrows(HDC _Nonnull dc,
                                 const struct diagram_primitive *_Nonnull primitive, int scale);
#endif
