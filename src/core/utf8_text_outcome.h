/* utf8_text の変換結果（C-005）。 */
#ifndef NENEFOLIO_UTF8_TEXT_OUTCOME_H
#define NENEFOLIO_UTF8_TEXT_OUTCOME_H

enum utf8_text_outcome : unsigned char
{
    UTF8_TEXT_CONVERTED,
    UTF8_TEXT_INVALID_UTF16,
    UTF8_TEXT_OUT_OF_MEMORY
};

#endif
