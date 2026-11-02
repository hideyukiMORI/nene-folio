#ifndef NENEFOLIO_CODE_SYNTAX_H
#define NENEFOLIO_CODE_SYNTAX_H
#include "code_language.h"
#include "markdown_rtf_outcome.h"
struct code_syntax;
struct text_buffer;
/* Short-lived lexer. The output belongs to the enclosing Markdown conversion. */
[[nodiscard]] enum markdown_rtf_outcome
code_syntax_create(enum code_language language, struct text_buffer *_Nonnull out,
                   struct code_syntax *_Nullable *_Nonnull syntax);
/* The validated UTF-8 line is borrowed only during this call. */
void code_syntax_append(struct code_syntax *_Nonnull syntax, const char *_Nonnull text,
                        size_t length);
void code_syntax_destroy(struct code_syntax *_Nullable syntax);
#endif
