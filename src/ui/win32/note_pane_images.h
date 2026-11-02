#ifndef NENEFOLIO_NOTE_PANE_IMAGES_H
#define NENEFOLIO_NOTE_PANE_IMAGES_H
#include "image_bitmap.h"
#include <tom.h>
#include <windows.h>
struct note_pane_images;
[[nodiscard]] enum image_outcome
note_pane_images_create(HWND _Nonnull window, ITextDocument *_Nonnull document, size_t count,
                        struct note_pane_images *_Nullable *_Nonnull out);
[[nodiscard]] bool note_pane_images_next(const struct note_pane_images *_Nonnull images,
                                         HWND _Nonnull window, size_t *_Nonnull index);
[[nodiscard]] enum image_outcome
note_pane_images_insert(struct note_pane_images *_Nonnull images, ITextDocument *_Nonnull document,
                        size_t index, const struct image_bitmap *_Nonnull bitmap);
void note_pane_images_finish(struct note_pane_images *_Nonnull images, size_t index);
void note_pane_images_reflow(struct note_pane_images *_Nonnull images);
[[nodiscard]] size_t note_pane_images_count(const struct note_pane_images *_Nonnull images);
[[nodiscard]] bool note_pane_images_bounds(const struct note_pane_images *_Nonnull images,
                                           ITextRange *_Nonnull range, size_t index,
                                           RECT *_Nonnull out);
[[nodiscard]] const struct image_bitmap *_Nullable note_pane_images_resize_next(
    struct note_pane_images *_Nonnull images, size_t *_Nonnull index);
void note_pane_images_destroy(struct note_pane_images *_Nullable images);
#endif
