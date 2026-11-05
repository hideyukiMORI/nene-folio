#include "code_syntax.h"
#include "code_syntax_phase.h"
#include "code_token.h"
#include "rtf_text.h"
#include "text_buffer.h"
#include <stdlib.h>
#include <string.h>

struct code_syntax
{
    struct text_buffer *_Nonnull out;
    enum code_language language;
    enum code_syntax_phase phase;
    const char *_Nonnull text;
    size_t length;
    size_t cursor;
    size_t depth;
    char quote;
    size_t quote_width;
    bool multiline;
};

enum markdown_rtf_outcome code_syntax_create(enum code_language language,
                                             struct text_buffer *_Nonnull out,
                                             struct code_syntax *_Nullable *_Nonnull syntax)
{
    struct code_syntax *_Nullable made = calloc(1, sizeof *made);
    if (made == nullptr)
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    made->out = out;
    made->language = language;
    made->text = "";
    *syntax = made;
    return MARKDOWN_RTF_CONVERTED;
}

void code_syntax_destroy(struct code_syntax *_Nullable syntax)
{
    free(syntax);
}

static char peek(const struct code_syntax *_Nonnull syntax, size_t offset)
{
    return offset < syntax->length - syntax->cursor ? syntax->text[syntax->cursor + offset] : '\0';
}

static bool matches(const struct code_syntax *_Nonnull syntax, const char *_Nonnull pattern)
{
    size_t length = strlen(pattern);
    return length != 0 && length <= syntax->length - syntax->cursor &&
           memcmp(syntax->text + syntax->cursor, pattern, length) == 0;
}

static bool in_set(char character, const char *_Nonnull values)
{
    for (size_t at = 0; values[at] != '\0'; ++at)
        if (values[at] == character)
            return true;
    return false;
}

static const char *_Nonnull token_style(enum code_token token)
{
    switch (token)
    {
    case CODE_TOKEN_TEXT:
        return "";
    case CODE_TOKEN_KEYWORD:
        return "{\\cf7 ";
    case CODE_TOKEN_STRING:
        return "{\\cf8 ";
    case CODE_TOKEN_COMMENT:
        return "{\\cf9 ";
    case CODE_TOKEN_NUMBER:
        return "{\\cf10 ";
    }
    return "";
}

static void emit(struct code_syntax *_Nonnull syntax, enum code_token token, size_t start)
{
    if (token != CODE_TOKEN_TEXT)
        text_buffer_append_text(syntax->out, token_style(token));
    rtf_text_append(syntax->out, syntax->text + start, syntax->cursor - start);
    if (token != CODE_TOKEN_TEXT)
        text_buffer_append_text(syntax->out, "}");
}

static bool advance_comment(struct code_syntax *_Nonnull syntax)
{
    const char *close = code_language_comment_close(syntax->language);
    if (matches(syntax, close))
    {
        syntax->cursor += strlen(close);
        return --syntax->depth == 0;
    }
    const char *open = code_language_comment_open(syntax->language);
    if (code_language_feature(syntax->language, CODE_FEATURE_NESTED_COMMENT) &&
        matches(syntax, open))
    {
        ++syntax->depth;
        syntax->cursor += strlen(open);
        return false;
    }
    ++syntax->cursor;
    return false;
}

static void read_comment(struct code_syntax *_Nonnull syntax, size_t start)
{
    while (syntax->cursor < syntax->length && syntax->phase == CODE_SYNTAX_COMMENT)
        if (advance_comment(syntax))
            syntax->phase = CODE_SYNTAX_TEXT;
    emit(syntax, CODE_TOKEN_COMMENT, start);
}

static bool shell_comment_boundary(const struct code_syntax *_Nonnull syntax)
{
    if (syntax->language != CODE_LANGUAGE_SHELL || syntax->cursor == 0)
        return true;
    return in_set(syntax->text[syntax->cursor - 1], " \t;|&()");
}

static bool try_comment(struct code_syntax *_Nonnull syntax)
{
    size_t start = syntax->cursor;
    if (matches(syntax, code_language_line_comment(syntax->language)) &&
        shell_comment_boundary(syntax))
    {
        syntax->cursor = syntax->length;
        emit(syntax, CODE_TOKEN_COMMENT, start);
        return true;
    }
    const char *open = code_language_comment_open(syntax->language);
    if (!matches(syntax, open))
        return false;
    syntax->depth = 1;
    syntax->phase = CODE_SYNTAX_COMMENT;
    syntax->cursor += strlen(open);
    read_comment(syntax, start);
    return true;
}

static bool quote_end(const struct code_syntax *_Nonnull syntax)
{
    for (size_t at = 0; at < syntax->quote_width; ++at)
        if (peek(syntax, at) != syntax->quote)
            return false;
    return true;
}

static bool doubled_quote(const struct code_syntax *_Nonnull syntax)
{
    enum code_feature feature =
        syntax->quote == '\'' ? CODE_FEATURE_DOUBLED_SINGLE : CODE_FEATURE_DOUBLED_DOUBLE;
    return syntax->quote_width == 1 && peek(syntax, 1) == syntax->quote &&
           code_language_feature(syntax->language, feature);
}

static bool advance_quote(struct code_syntax *_Nonnull syntax)
{
    if (quote_end(syntax))
    {
        if (doubled_quote(syntax))
        {
            syntax->cursor += 2;
            return false;
        }
        syntax->cursor += syntax->quote_width;
        syntax->phase = CODE_SYNTAX_TEXT;
        return false;
    }
    char escape = code_language_escape(syntax->language, syntax->quote);
    if (escape != '\0' && peek(syntax, 0) == escape)
    {
        ++syntax->cursor;
        if (syntax->cursor == syntax->length)
            return true;
    }
    ++syntax->cursor;
    return false;
}

static void read_quote(struct code_syntax *_Nonnull syntax, size_t start)
{
    bool continued = false;
    while (syntax->cursor < syntax->length && syntax->phase == CODE_SYNTAX_QUOTED)
        continued = advance_quote(syntax);
    if (syntax->phase == CODE_SYNTAX_QUOTED && !syntax->multiline && !continued)
        syntax->phase = CODE_SYNTAX_TEXT;
    emit(syntax, CODE_TOKEN_STRING, start);
}

static bool only_space(const struct code_syntax *_Nonnull syntax, size_t offset)
{
    for (size_t at = syntax->cursor + offset; at < syntax->length; ++at)
        if (syntax->text[at] != ' ' && syntax->text[at] != '\t')
            return false;
    return true;
}

static bool try_here(struct code_syntax *_Nonnull syntax)
{
    if (!code_language_feature(syntax->language, CODE_FEATURE_HERE_STRING) ||
        peek(syntax, 0) != '@')
        return false;
    char quote = peek(syntax, 1);
    if ((quote != '\'' && quote != '"') || !only_space(syntax, 2))
        return false;
    syntax->quote = quote;
    syntax->phase = CODE_SYNTAX_HERE;
    size_t start = syntax->cursor;
    syntax->cursor = syntax->length;
    emit(syntax, CODE_TOKEN_STRING, start);
    return true;
}

static void read_here(struct code_syntax *_Nonnull syntax)
{
    if (peek(syntax, 0) == syntax->quote && peek(syntax, 1) == '@')
    {
        syntax->cursor = 2;
        syntax->phase = CODE_SYNTAX_TEXT;
    }
    else
        syntax->cursor = syntax->length;
    emit(syntax, CODE_TOKEN_STRING, 0);
}

static bool quoted(const struct code_syntax *_Nonnull syntax, char character)
{
    return character == '"' ||
           (character == '\'' &&
            code_language_feature(syntax->language, CODE_FEATURE_SINGLE_QUOTE)) ||
           (character == '`' && code_language_feature(syntax->language, CODE_FEATURE_BACKTICK));
}

static bool try_string(struct code_syntax *_Nonnull syntax)
{
    if (try_here(syntax))
        return true;
    char quote = peek(syntax, 0);
    if (!quoted(syntax, quote))
        return false;
    size_t start = syntax->cursor;
    syntax->quote = quote;
    syntax->quote_width = 1;
    if (code_language_feature(syntax->language, CODE_FEATURE_TRIPLE) && peek(syntax, 1) == quote &&
        peek(syntax, 2) == quote)
        syntax->quote_width = 3;
    syntax->multiline = syntax->quote_width == 3 || quote == '`' ||
                        code_language_feature(syntax->language, CODE_FEATURE_MULTILINE);
    syntax->phase = CODE_SYNTAX_QUOTED;
    syntax->cursor += syntax->quote_width;
    read_quote(syntax, start);
    return true;
}

static bool digit(char character)
{
    return character >= '0' && character <= '9';
}

static bool word_start(char character)
{
    return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
           character == '_' || character == '$' || (unsigned char)character >= 0x80;
}

static void read_word(struct code_syntax *_Nonnull syntax)
{
    size_t start = syntax->cursor++;
    while (word_start(peek(syntax, 0)) || digit(peek(syntax, 0)))
        ++syntax->cursor;
    enum code_token token =
        code_language_keyword(syntax->language, syntax->text + start, syntax->cursor - start)
            ? CODE_TOKEN_KEYWORD
            : CODE_TOKEN_TEXT;
    emit(syntax, token, start);
}

static bool number_part(const struct code_syntax *_Nonnull syntax)
{
    char character = peek(syntax, 0);
    if (character == '\0')
        return false;
    if (digit(character) || in_set(character, "._abcdefABCDEFxXoOpPnNuUlLjJmM"))
        return true;
    return (character == '+' || character == '-') && syntax->cursor > 0 &&
           in_set(syntax->text[syntax->cursor - 1], "eEpP") && digit(peek(syntax, 1));
}

static void read_number(struct code_syntax *_Nonnull syntax)
{
    size_t start = syntax->cursor++;
    while (number_part(syntax))
        ++syntax->cursor;
    emit(syntax, CODE_TOKEN_NUMBER, start);
}

static bool token_start(char character)
{
    return word_start(character) || digit(character) || in_set(character, "/\"'`#<@.-");
}

static void read_plain(struct code_syntax *_Nonnull syntax)
{
    size_t start = syntax->cursor++;
    char escape = code_language_escape(syntax->language, '"');
    if (syntax->text[start] == escape && syntax->cursor < syntax->length)
        ++syntax->cursor;
    while (syntax->cursor < syntax->length && !token_start(peek(syntax, 0)))
    {
        char current = syntax->text[syntax->cursor++];
        if (current == escape && syntax->cursor < syntax->length)
            ++syntax->cursor;
    }
    emit(syntax, CODE_TOKEN_TEXT, start);
}

static void read_text(struct code_syntax *_Nonnull syntax)
{
    if (try_comment(syntax) || try_string(syntax))
        return;
    char character = peek(syntax, 0);
    if (word_start(character))
    {
        read_word(syntax);
        return;
    }
    if (digit(character) || (character == '.' && digit(peek(syntax, 1))))
    {
        read_number(syntax);
        return;
    }
    read_plain(syntax);
}

void code_syntax_append(struct code_syntax *_Nonnull syntax, const char *_Nonnull text,
                        size_t length)
{
    if (syntax->language == CODE_LANGUAGE_PLAIN)
    {
        rtf_text_append(syntax->out, text, length);
        return;
    }
    syntax->text = text;
    syntax->length = length;
    syntax->cursor = 0;
    if (length == 0 && syntax->phase == CODE_SYNTAX_QUOTED && !syntax->multiline)
        syntax->phase = CODE_SYNTAX_TEXT;
    while (syntax->cursor < length)
    {
        switch (syntax->phase)
        {
        case CODE_SYNTAX_TEXT:
            read_text(syntax);
            break;
        case CODE_SYNTAX_COMMENT:
            read_comment(syntax, syntax->cursor);
            break;
        case CODE_SYNTAX_QUOTED:
            read_quote(syntax, syntax->cursor);
            break;
        case CODE_SYNTAX_HERE:
            read_here(syntax);
            break;
        }
    }
    syntax->text = "";
    syntax->length = syntax->cursor = 0;
}
