/* 本文の改行の形。閉じた集合（C-002）。読んだ本文の最初の改行で決まり、書き戻すときに
 * 同じ形へ揃える（ADR 0006）。 */
#ifndef NENEFOLIO_LINE_ENDING_H
#define NENEFOLIO_LINE_ENDING_H

enum line_ending : unsigned char
{
    LINE_ENDING_LF,
    LINE_ENDING_CRLF
};

#endif
