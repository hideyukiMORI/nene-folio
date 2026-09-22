/* utf16_text_fill の結果（C-005 / ADR 0030 の決定 5）。utf16_text_outcome と統合しないのは、
 * utf16_text_create が決して返さない TOO_LONG を結果型に混ぜないため。 */
#ifndef NENEFOLIO_UTF16_TEXT_FILL_OUTCOME_H
#define NENEFOLIO_UTF16_TEXT_FILL_OUTCOME_H

enum utf16_text_fill_outcome : unsigned char
{
    UTF16_TEXT_FILL_READY,
    /* 終端まで入れると capacity を超える。out は触らず、呼び出し側は何も出さない */
    UTF16_TEXT_FILL_TOO_LONG,
    /* UTF-8 として読めない。out は触らない */
    UTF16_TEXT_FILL_MALFORMED
};

#endif
