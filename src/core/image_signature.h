/* #1062 / ADR0120: 取り込める画像かを、中身の先頭で確かめる（拡張子は信じない）。純粋な関数。
 * 表示のときの復号（WIC）が受ける入れ物と同じ 2 つ: PNG と JPEG（ADR0085）。 */
#ifndef NENEFOLIO_IMAGE_SIGNATURE_H
#define NENEFOLIO_IMAGE_SIGNATURE_H
#include <stddef.h>
/* bytes が PNG か JPEG の署名で始まるか。短すぎれば false。 */
[[nodiscard]] bool image_signature_supported(const unsigned char *_Nonnull bytes, size_t length);
#endif
