#ifndef NENEFOLIO_IMAGE_PORT_H
#define NENEFOLIO_IMAGE_PORT_H
#include "diagram_request.h"
#include "image_adopt_outcome.h"
#include "image_asset.h"
#include "image_export_policy.h"
#include "image_outcome.h"
#include "image_phase.h"
#include "image_request.h"
#include <stdint.h>
struct image_adapter;
struct image_bitmap;
struct storage_path;
struct text_buffer;
struct image_port
{
    struct image_adapter *_Nonnull adapter;
    enum image_outcome (*_Nonnull request)(struct image_adapter *_Nonnull adapter,
                                           const struct image_request *_Nonnull request,
                                           uint64_t *_Nonnull ticket);
    enum image_outcome (*_Nonnull diagram)(struct image_adapter *_Nonnull adapter,
                                           const struct diagram_request *_Nonnull request,
                                           uint64_t *_Nonnull ticket);
    enum image_outcome (*_Nonnull poll)(struct image_adapter *_Nonnull adapter, uint64_t ticket,
                                        struct image_bitmap *_Nullable *_Nonnull out);
    enum image_phase (*_Nonnull phase)(struct image_adapter *_Nonnull adapter);
    void (*_Nonnull cancel)(struct image_adapter *_Nonnull adapter);
    enum image_outcome (*_Nonnull asset)(struct image_adapter *_Nonnull adapter,
                                         const struct image_request *_Nonnull request,
                                         enum image_export_policy policy,
                                         struct image_asset *_Nullable *_Nonnull out);
    /* #1062 / ADR0120: source（外のどこかのファイル）を、category の images/ へ取り込む。
     * COPIED か PRESENT なら、images/ からの相対パス（区切りは `/`）を leaf へ足す。
     * 既にあるファイルは上書きしない: 同じ名前で中身が同じならそれを指し、違えば番号を付けた名前で
     * 写す。ほかの結果では何も写さず、leaf も変えない。 */
    enum image_adopt_outcome (*_Nonnull adopt)(struct image_adapter *_Nonnull adapter,
                                               const char *_Nonnull category,
                                               const struct storage_path *_Nonnull source,
                                               struct text_buffer *_Nonnull leaf);
};
#endif
