#ifndef NENEFOLIO_DIAGRAM_TEXT_H
#define NENEFOLIO_DIAGRAM_TEXT_H
#include <stddef.h>
/* A borrowed UTF-8 slice; parsing never alters the source allocation. */
struct diagram_text
{
    const char *_Nonnull bytes;
    size_t length;
};
[[nodiscard]] struct diagram_text diagram_text_trim(struct diagram_text text);
[[nodiscard]] struct diagram_text diagram_text_skip(struct diagram_text text, size_t count);
[[nodiscard]] bool diagram_text_equal(struct diagram_text text, const char *_Nonnull word);
[[nodiscard]] bool diagram_text_starts(struct diagram_text text, const char *_Nonnull word);
[[nodiscard]] bool diagram_text_character_in(char value, const char *_Nonnull characters);
[[nodiscard]] struct diagram_text diagram_text_word(struct diagram_text *_Nonnull text);
[[nodiscard]] struct diagram_text diagram_text_identifier(struct diagram_text *_Nonnull text);
#endif
