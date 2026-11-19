/* #1071 / ADR0118: 指定したノートを開いて箇所を選ぶ依頼。外から来た文字列（リンク、後に依頼の
 * ファイル）がこの型になる唯一の入口で、ここを通った値だけが application へ渡る（ARC-008）。
 * できるのは「開く」だけ。書く・作る・消す動作はこの型に無い。 */
#ifndef NENEFOLIO_OPEN_REQUEST_H
#define NENEFOLIO_OPEN_REQUEST_H
#include "open_request_lines.h"
#include "open_request_outcome.h"
#include <stddef.h>
struct category_name;
struct note_name;
struct open_request;
/* リンク全体の上限（UTF-8 のバイト数）。コマンドラインと Shell の上限の内側に置く。 */
constexpr size_t open_request_byte_limit = 2048;
/* 探す文の上限（% を戻した後の UTF-8 のバイト数）。 */
constexpr size_t open_request_text_limit = 1024;
/* 行の番号の上限。 */
constexpr size_t open_request_line_limit = 9999999;
/* `nenefolio://open?category=…&note=…[&lines=N|N-M][&text=…]` を読む。
 * スキームと `open` は ASCII の大小を問わない。`//` と `open` の後の `/` は省ける。
 * 値は % で符号化した UTF-8。`+` は空白にしない。値に制御文字は置けない。
 * category と note は必須で、既存の名前の規則（category_name / note_name）を通す。
 * 知らないキーは読み飛ばす。知っているキーの重複、`#`、空の区切りは不正。空の text は無いのと同じ。
 * 失敗では out を変えない。 */
[[nodiscard]] enum open_request_outcome
open_request_create(const char *_Nonnull bytes, size_t length,
                    struct open_request *_Nullable *_Nonnull out);
[[nodiscard]] const struct category_name *_Nonnull open_request_category(
    const struct open_request *_Nonnull request);
[[nodiscard]] const struct note_name *_Nonnull open_request_note(
    const struct open_request *_Nonnull request);
/* lines があれば true で out へ入れる。無ければ false で out は変えない。 */
[[nodiscard]] bool open_request_lines(const struct open_request *_Nonnull request,
                                      struct open_request_lines *_Nonnull out);
/* 探す文（終端付きの UTF-8）。無ければ空文字列で、長さは 0。request が生きている間だけ有効。 */
[[nodiscard]] const char *_Nonnull open_request_text(const struct open_request *_Nonnull request);
[[nodiscard]] size_t open_request_text_length(const struct open_request *_Nonnull request);
void open_request_destroy(struct open_request *_Nullable request);
#endif
