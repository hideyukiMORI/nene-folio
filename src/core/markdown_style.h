/* 行内の整形の状態。全メンバーが独立に妥当な完全型（C-003）。 */
#ifndef NENEFOLIO_MARKDOWN_STYLE_H
#define NENEFOLIO_MARKDOWN_STYLE_H

struct markdown_style
{
    bool bold;
    bool italic;
    char previous; /* 直前のバイト。行頭は空白 */
};

#endif
