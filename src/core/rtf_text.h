#ifndef NENEFOLIO_RTF_TEXT_H
#define NENEFOLIO_RTF_TEXT_H
#include <stddef.h>
struct text_buffer;
void rtf_text_append(struct text_buffer *_Nonnull out, const char *_Nonnull text, size_t length);
void rtf_text_number(struct text_buffer *_Nonnull out, size_t value);
#endif
