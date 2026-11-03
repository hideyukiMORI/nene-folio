#ifndef NENEFOLIO_DIAGRAM_RENDER_H
#define NENEFOLIO_DIAGRAM_RENDER_H
#include "diagram_request.h"
#include "image_bitmap.h"
#include "image_work.h"
[[nodiscard]] enum image_outcome diagram_render_read(const struct diagram_request *_Nonnull request,
                                                     const struct image_work *_Nonnull work,
                                                     struct image_bitmap *_Nullable *_Nonnull out);
#endif
