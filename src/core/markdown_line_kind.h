/* markdown_rtf が行を分類する閉じた集合（C-002）。 */
#ifndef NENEFOLIO_MARKDOWN_LINE_KIND_H
#define NENEFOLIO_MARKDOWN_LINE_KIND_H

enum markdown_line_kind : unsigned char
{
    MARKDOWN_LINE_BLANK,
    MARKDOWN_LINE_HEADING, /* #〜###### と空白 */
    MARKDOWN_LINE_FENCE,   /* ``` で始まる */
    MARKDOWN_LINE_QUOTE,   /* > */
    MARKDOWN_LINE_BULLET,  /* - * + と空白 */
    MARKDOWN_LINE_ORDERED, /* 数字 . 空白 */
    MARKDOWN_LINE_TEXT
};

#endif
