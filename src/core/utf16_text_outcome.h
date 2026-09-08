/* utf16_text の変換結果（C-005）。 */
#ifndef NENEFOLIO_UTF16_TEXT_OUTCOME_H
#define NENEFOLIO_UTF16_TEXT_OUTCOME_H

enum utf16_text_outcome : unsigned char
{
    UTF16_TEXT_CONVERTED,
    UTF16_TEXT_INVALID_UTF8,
    UTF16_TEXT_OUT_OF_MEMORY
};

#endif
