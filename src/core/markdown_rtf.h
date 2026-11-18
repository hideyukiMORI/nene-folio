/* Markdown source → the one UTF-8 RTF path (ADR0070). Supported syntax is documented
 * in docs/MARKDOWN.md. Editing retains the original source through markdown_rtf_plain. */
#ifndef NENEFOLIO_MARKDOWN_RTF_H
#define NENEFOLIO_MARKDOWN_RTF_H

#include "markdown_rtf_locale.h"
#include "markdown_rtf_outcome.h"
#include "rtf_palette.h"

#include <stddef.h>

struct markdown_rtf;
struct theme_palette;
struct markdown_images;
struct markdown_media;
[[nodiscard]] const struct markdown_media *_Nullable markdown_rtf_media(
    const struct markdown_rtf *_Nonnull rtf);
[[nodiscard]] const struct markdown_images *_Nullable markdown_rtf_images(
    const struct markdown_rtf *_Nonnull rtf);
struct note_text;

/* face は本文のフォント名（ui_font_face が答える ASCII。`{` `}` `\` `;` を含まない）で、
 * fonttbl の 0 に入る（ADR 0032 の決定 5）。コードの 1 は Consolas のまま。 */
/* 閲覧の文書は face に加えて、コードブロックのコピー操作の文言の言語を受ける（ADR0098）。
 * 配色は実効の theme_palette から受ける。色表は rtf の役割の10色だけで、コピー操作の札は
 * 言語ラベルの対を反転して使う（地が code_comment の 9、文字が code_background の 6）。
 * contrast が高コントラストのときは塗らず、本文の文字色と下線にする。 */
[[nodiscard]] enum markdown_rtf_outcome
markdown_rtf_create(const struct note_text *_Nonnull text,
                    const struct theme_palette *_Nonnull theme, struct markdown_rtf_locale locale,
                    struct markdown_rtf *_Nullable *_Nonnull out);
/* 何も選んでいないときの空の文書（頭と末尾だけ）。 */
[[nodiscard]] enum markdown_rtf_outcome
markdown_rtf_empty(const struct theme_palette *_Nonnull theme, const char *_Nonnull face,
                   struct markdown_rtf *_Nullable *_Nonnull out);
/* 編集用の RTF（#250）。Markdown を解釈せず、本文の UTF-8 をそのまま書く。
 * `\` `{` `}` だけを escape し、改行（LF・CRLF・孤立 CR のそれぞれ 1 つ）を `\par ` にする。
 * TAB・VT・ほかの制御文字は生のまま。末尾に `\par ` を 1 つ足して `}` で閉じる。
 * 頭（fonttbl・colortbl・段落の書式）は閲覧と同じで、本文の色は palette.text（\cf1）。 */
[[nodiscard]] enum markdown_rtf_outcome
markdown_rtf_plain(const struct note_text *_Nonnull text,
                   const struct theme_palette *_Nonnull theme, const char *_Nonnull face,
                   struct markdown_rtf *_Nullable *_Nonnull out);
/* 終端付きの RTF。rtf が生きている間だけ有効。 */
[[nodiscard]] const char *_Nonnull markdown_rtf_text(const struct markdown_rtf *_Nonnull rtf);
[[nodiscard]] size_t markdown_rtf_length(const struct markdown_rtf *_Nonnull rtf);
void markdown_rtf_destroy(struct markdown_rtf *_Nullable rtf);

#endif
