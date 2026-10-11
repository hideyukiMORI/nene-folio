/* md → RTF のサブセット変換（FR-005 / ADR 0002）。純関数でUTF-8 RTFを作る。
 * RichEditの入力境界はUTF-8を明示する（#236）。対応する記法: 見出し（#〜######）・強調（**太字**
 * __太字__ *斜体* _斜体_）・インラインコード（`…`）・ コードブロック（``` … ```）・箇条書き（- *
 * +）・番号付き（1.）・リンク（[文字](url) は下線の文字だけ）・引用（>）
 * それ以外の行は段落で、連続する行は 1 つの段落に繋ぐ。入れ子のリスト・表・画像・HTML は扱わない。
 */
#ifndef NENEFOLIO_MARKDOWN_RTF_H
#define NENEFOLIO_MARKDOWN_RTF_H

#include "markdown_rtf_outcome.h"
#include "rtf_palette.h"

#include <stddef.h>

struct markdown_rtf;
struct note_text;

/* face は本文のフォント名（ui_font_face が答える ASCII。`{` `}` `\` `;` を含まない）で、
 * fonttbl の 0 に入る（ADR 0032 の決定 5）。コードの 1 は Consolas のまま。 */
[[nodiscard]] enum markdown_rtf_outcome
markdown_rtf_create(const struct note_text *_Nonnull text, struct rtf_palette palette,
                    const char *_Nonnull face, struct markdown_rtf *_Nullable *_Nonnull out);
/* 何も選んでいないときの空の文書（頭と末尾だけ）。 */
[[nodiscard]] enum markdown_rtf_outcome
markdown_rtf_empty(struct rtf_palette palette, const char *_Nonnull face,
                   struct markdown_rtf *_Nullable *_Nonnull out);
/* 編集用の RTF（#250）。Markdown を解釈せず、本文の UTF-8 をそのまま書く。
 * `\` `{` `}` だけを escape し、改行（LF・CRLF・孤立 CR のそれぞれ 1 つ）を `\par ` にする。
 * TAB・VT・ほかの制御文字は生のまま。末尾に `\par ` を 1 つ足して `}` で閉じる。
 * 頭（fonttbl・colortbl・段落の書式）は閲覧と同じで、本文の色は palette.text（\cf1）。 */
[[nodiscard]] enum markdown_rtf_outcome
markdown_rtf_plain(const struct note_text *_Nonnull text, struct rtf_palette palette,
                   const char *_Nonnull face, struct markdown_rtf *_Nullable *_Nonnull out);
/* 終端付きの RTF。rtf が生きている間だけ有効。 */
[[nodiscard]] const char *_Nonnull markdown_rtf_text(const struct markdown_rtf *_Nonnull rtf);
[[nodiscard]] size_t markdown_rtf_length(const struct markdown_rtf *_Nonnull rtf);
void markdown_rtf_destroy(struct markdown_rtf *_Nullable rtf);

#endif
