/* ui_text_format の結果（C-005 / ADR 0030 の決定 3）。接頭辞は表の ID（UI_TEXT_）と
 * 重ならないように UI_TEXT_FORMAT_ にする。 */
#ifndef NENEFOLIO_UI_TEXT_FORMAT_OUTCOME_H
#define NENEFOLIO_UI_TEXT_FORMAT_OUTCOME_H

enum ui_text_format_outcome : unsigned char
{
    UI_TEXT_FORMAT_READY,
    /* 埋めた句が capacity に収まらない。out は空文字列で、呼び出し側は何も出さない */
    UI_TEXT_FORMAT_TOO_LONG
};

#endif
