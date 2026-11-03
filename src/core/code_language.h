#ifndef NENEFOLIO_CODE_LANGUAGE_H
#define NENEFOLIO_CODE_LANGUAGE_H
#include "code_feature.h"
#include "markdown_span.h"
#include <stddef.h>
enum code_language : unsigned char
{
    CODE_LANGUAGE_PLAIN,
    CODE_LANGUAGE_C,
    CODE_LANGUAGE_CPP,
    CODE_LANGUAGE_JAVASCRIPT,
    CODE_LANGUAGE_TYPESCRIPT,
    CODE_LANGUAGE_PYTHON,
    CODE_LANGUAGE_JSON,
    CODE_LANGUAGE_SHELL,
    CODE_LANGUAGE_POWERSHELL,
    CODE_LANGUAGE_SQL
};
/* Borrowed first info word; offset is relative to info. */
[[nodiscard]] struct markdown_span code_language_word(const char *_Nonnull info, size_t length);
[[nodiscard]] enum code_language code_language_parse(const char *_Nonnull info, size_t length);
[[nodiscard]] bool code_language_keyword(enum code_language language, const char *_Nonnull word,
                                         size_t length);
[[nodiscard]] const char *_Nonnull code_language_line_comment(enum code_language language);
[[nodiscard]] const char *_Nonnull code_language_comment_open(enum code_language language);
[[nodiscard]] const char *_Nonnull code_language_comment_close(enum code_language language);
[[nodiscard]] bool code_language_feature(enum code_language language, enum code_feature feature);
[[nodiscard]] char code_language_escape(enum code_language language, char quote);
#endif
