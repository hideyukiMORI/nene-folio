#ifndef NENEFOLIO_MARKDOWN_MEDIA_H
#define NENEFOLIO_MARKDOWN_MEDIA_H
#include "markdown_images.h"
#include "markdown_line.h"
#include "markdown_media_kind.h"
#include "note_text.h"
struct markdown_media;
constexpr size_t markdown_media_display_limit = 256;
[[nodiscard]] enum image_outcome
markdown_media_create(const char *_Nonnull source, struct markdown_media *_Nullable *_Nonnull out);
/* Display slots and export image references are distinct collections of the same traversal. */
[[nodiscard]] enum image_outcome markdown_media_image(struct markdown_media *_Nonnull media,
                                                      struct markdown_view view,
                                                      struct note_search_span destination,
                                                      size_t *_Nonnull slot);
[[nodiscard]] enum image_outcome markdown_media_begin_diagram(struct markdown_media *_Nonnull media,
                                                              size_t *_Nonnull slot);
void markdown_media_diagram_line(struct markdown_media *_Nonnull media, size_t slot,
                                 struct markdown_line line);
[[nodiscard]] enum image_outcome markdown_media_end_diagram(struct markdown_media *_Nonnull media,
                                                            size_t slot);
/* コピー用のコード原文（ADR0098）。表示slotとは別の番号で、個数の上限値と原文の総byteを共有する。
 * bytes は結合後の長さ。上限を超えるブロックは TOO_LARGE で、表示だけを従来どおり行う。 */
[[nodiscard]] enum image_outcome markdown_media_begin_code(struct markdown_media *_Nonnull media,
                                                           size_t bytes, size_t *_Nonnull slot);
/* 描画と同じ行（字下げとcontainer接頭辞を除いたもの）を LF で結ぶ。末尾に改行を足さない。
 * slot は begin_code が返した番号。 */
void markdown_media_code_line(struct markdown_media *_Nonnull media, size_t slot,
                              struct markdown_line line);
[[nodiscard]] enum image_outcome markdown_media_end_code(struct markdown_media *_Nonnull media,
                                                         size_t slot);
[[nodiscard]] size_t markdown_media_code_count(const struct markdown_media *_Nonnull media);
/* 終端付き。media が生きている間だけ有効。slot は code_count 未満。 */
[[nodiscard]] const char *_Nonnull markdown_media_code_bytes(
    const struct markdown_media *_Nonnull media, size_t slot);
[[nodiscard]] size_t markdown_media_code_length(const struct markdown_media *_Nonnull media,
                                                size_t slot);
[[nodiscard]] size_t markdown_media_count(const struct markdown_media *_Nonnull media);
[[nodiscard]] bool markdown_media_overflow(const struct markdown_media *_Nonnull media);
[[nodiscard]] const struct markdown_images *_Nonnull markdown_media_images(
    const struct markdown_media *_Nonnull media);
[[nodiscard]] enum markdown_media_kind
markdown_media_kind(const struct markdown_media *_Nonnull media, size_t slot);
[[nodiscard]] enum image_outcome markdown_media_status(const struct markdown_media *_Nonnull media,
                                                       size_t slot);
[[nodiscard]] size_t markdown_media_image_index(const struct markdown_media *_Nonnull media,
                                                size_t slot);
[[nodiscard]] const struct note_text *_Nonnull markdown_media_diagram(
    const struct markdown_media *_Nonnull media, size_t slot);
void markdown_media_placeholder(struct text_buffer *_Nonnull output, size_t index);
void markdown_media_destroy(struct markdown_media *_Nullable media);
#endif
