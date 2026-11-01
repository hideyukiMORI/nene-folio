#ifndef NENEFOLIO_NOTE_MINIMAP_H
#define NENEFOLIO_NOTE_MINIMAP_H
#include "folio_palette.h"
#include "note_minimap_outcome.h"
#include "note_minimap_paint.h"

struct note_minimap;
struct note_pane;
[[nodiscard]] enum note_minimap_outcome
note_minimap_create(struct note_minimap *_Nullable *_Nonnull out);
void note_minimap_destroy(struct note_minimap *_Nullable map);
/* 入力をまとめるtimer待ち。完成済みの同じ本文の像は残す。 */
void note_minimap_invalidate(struct note_minimap *_Nonnull map);
/* 本文投入/非表示では別文書の像を残さない。 */
void note_minimap_clear(struct note_minimap *_Nonnull map);
void note_minimap_begin(struct note_minimap *_Nonnull map);
[[nodiscard]] bool note_minimap_pending(const struct note_minimap *_Nonnull map);
/* 一回16sample以下。完成または失敗でtrueを返し、pendingを下ろす。 */
[[nodiscard]] bool note_minimap_step(struct note_minimap *_Nonnull map,
                                     struct note_pane *_Nonnull pane);
/* 呼出元は本文READY/可視を確認する。いずれも本文/選択を変更しない。 */
[[nodiscard]] bool note_minimap_viewport(HWND _Nonnull pane,
                                         struct note_minimap_view *_Nonnull out);
[[nodiscard]] int note_minimap_limit(struct note_minimap_view view);
[[nodiscard]] RECT note_minimap_track(const struct note_minimap_paint *_Nonnull paint);
[[nodiscard]] RECT note_minimap_thumb(const struct note_minimap_paint *_Nonnull paint);
/* pointerは主窓clientのy。offsetはthumb内の掴んだ位置。 */
[[nodiscard]] int note_minimap_drag(const struct note_minimap_paint *_Nonnull paint, int y,
                                    int offset);
[[nodiscard]] int note_minimap_jump(const struct note_minimap_paint *_Nonnull paint, int y);
void note_minimap_draw(const struct note_minimap *_Nonnull map, HDC _Nonnull device,
                       const struct folio_palette *_Nonnull palette,
                       const struct note_minimap_paint *_Nonnull paint);
#endif
