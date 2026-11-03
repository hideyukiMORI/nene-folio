#include "code_language.h"
#include "ascii_fold.h"
#include <stdint.h>
#include <string.h>

#define FEATURE(name) (1U << CODE_FEATURE_##name)
static const struct
{
    const char *_Nonnull aliases;
    const char *_Nonnull keywords;
    const char *_Nonnull line;
    const char *_Nonnull opening;
    const char *_Nonnull closing;
    unsigned features;
    char escape;
    bool single_escape;
    bool fold_keywords;
} languages[] = {
    {"text plaintext txt", "", "", "", "", 0, 0, false, false},
    {"c",
     "alignas alignof auto bool break case char const constexpr continue default do double "
     "else enum extern false float for goto if inline int long nullptr register restrict "
     "return short signed sizeof static static_assert struct switch thread_local true typedef "
     "typeof typeof_unqual union unsigned void volatile while _Atomic _Bool _Complex "
     "_Generic _Imaginary _Noreturn _Static_assert _Thread_local NULL",
     "//", "/*", "*/", FEATURE(SINGLE_QUOTE), '\\', true, false},
    {"cpp c++ cc cxx",
     "alignas alignof and and_eq asm auto bitand bitor bool break case catch "
     "char char8_t char16_t char32_t class compl concept const consteval constexpr constinit "
     "const_cast continue co_await co_return co_yield decltype default delete do double "
     "dynamic_cast else enum explicit export extern false float for friend goto if inline "
     "int long mutable namespace new noexcept not not_eq nullptr operator or or_eq private "
     "protected public register reinterpret_cast requires return short signed sizeof static "
     "static_assert static_cast struct switch template this thread_local throw true try "
     "typedef typeid typename union unsigned using virtual void volatile wchar_t while xor xor_eq "
     "NULL",
     "//", "/*", "*/", FEATURE(SINGLE_QUOTE), '\\', true, false},
    {"javascript js",
     "async await break case catch class const continue debugger default delete do "
     "else export extends false finally for from function get if import in instanceof let new "
     "null of return set static super switch this throw true try typeof undefined var void while "
     "yield",
     "//", "/*", "*/", FEATURE(SINGLE_QUOTE) | FEATURE(BACKTICK), '\\', true, false},
    {"typescript ts",
     "abstract any as asserts async await bigint boolean break case catch class "
     "const constructor continue debugger declare default delete do else enum export extends "
     "false finally for from function get if implements import in infer instanceof interface "
     "is keyof let module namespace never new null number object of private protected public "
     "readonly require return satisfies set static string super switch symbol this throw true "
     "try type typeof undefined unique unknown var void while yield",
     "//", "/*", "*/", FEATURE(SINGLE_QUOTE) | FEATURE(BACKTICK), '\\', true, false},
    {"python py python3",
     "False None True and as assert async await break class continue def del "
     "elif else except finally for from global if import in is lambda nonlocal not or pass raise "
     "return try while with yield match case",
     "#", "", "", FEATURE(SINGLE_QUOTE) | FEATURE(TRIPLE), '\\', true, false},
    {"json", "true false null", "", "", "", 0, '\\', false, false},
    {"sh shell bash zsh",
     "if then else elif fi case esac for select while until do done in function "
     "time coproc return break continue export local readonly declare typeset unset shift true "
     "false",
     "#", "", "", FEATURE(SINGLE_QUOTE) | FEATURE(BACKTICK) | FEATURE(MULTILINE), '\\', false,
     false},
    {"powershell ps1 pwsh",
     "begin break catch class continue data define do dynamicparam else elseif "
     "end enum exit filter finally for foreach from function hidden if in inlinescript param "
     "process return static switch throw trap try until using var while workflow $true $false "
     "$null",
     "#", "<#", "#>",
     FEATURE(SINGLE_QUOTE) | FEATURE(MULTILINE) | FEATURE(DOUBLED_SINGLE) |
         FEATURE(DOUBLED_DOUBLE) | FEATURE(HERE_STRING),
     '`', false, true},
    {"sql",
     "all alter and any as asc begin between by case cast check column commit constraint "
     "create cross current_date current_time current_timestamp database default delete desc "
     "distinct drop else end except exists false fetch foreign from full grant group having "
     "in index inner insert intersect into is join key left like limit not null offset on or "
     "order outer primary references return returning right rollback select set table then "
     "true truncate union unique update using values view when where with",
     "--", "/*", "*/",
     FEATURE(SINGLE_QUOTE) | FEATURE(MULTILINE) | FEATURE(DOUBLED_SINGLE) |
         FEATURE(DOUBLED_DOUBLE) | FEATURE(NESTED_COMMENT),
     0, false, true},
};
#undef FEATURE

static size_t slot(enum code_language language)
{
    switch (language)
    {
    case CODE_LANGUAGE_PLAIN:
        return 0;
    case CODE_LANGUAGE_C:
        return 1;
    case CODE_LANGUAGE_CPP:
        return 2;
    case CODE_LANGUAGE_JAVASCRIPT:
        return 3;
    case CODE_LANGUAGE_TYPESCRIPT:
        return 4;
    case CODE_LANGUAGE_PYTHON:
        return 5;
    case CODE_LANGUAGE_JSON:
        return 6;
    case CODE_LANGUAGE_SHELL:
        return 7;
    case CODE_LANGUAGE_POWERSHELL:
        return 8;
    case CODE_LANGUAGE_SQL:
        return 9;
    }
    return 0;
}

static bool same_word(const char *_Nonnull word, size_t length, const char *_Nonnull candidate,
                      bool fold)
{
    for (size_t at = 0; at < length; ++at)
    {
        char left = fold ? ascii_fold_byte(word[at]) : word[at];
        char right = fold ? ascii_fold_byte(candidate[at]) : candidate[at];
        if (left != right)
            return false;
    }
    return true;
}

static bool listed(const char *_Nonnull word, size_t length, const char *_Nonnull list, bool fold)
{
    while (*list != '\0')
    {
        size_t count = 0;
        while (list[count] != '\0' && list[count] != ' ')
            ++count;
        if (count == length && same_word(word, length, list, fold))
            return true;
        list += count;
        if (*list == ' ')
            ++list;
    }
    return false;
}

struct markdown_span code_language_word(const char *_Nonnull info, size_t length)
{
    size_t start = 0;
    while (start < length && (info[start] == ' ' || info[start] == '\t'))
        ++start;
    size_t end = start;
    while (end < length && info[end] != ' ' && info[end] != '\t')
        ++end;
    return (struct markdown_span){.bytes = info + start, .length = end - start, .offset = start};
}

enum code_language code_language_parse(const char *_Nonnull info, size_t length)
{
    struct markdown_span word = code_language_word(info, length);
    static const enum code_language ids[] = {CODE_LANGUAGE_PLAIN,      CODE_LANGUAGE_C,
                                             CODE_LANGUAGE_CPP,        CODE_LANGUAGE_JAVASCRIPT,
                                             CODE_LANGUAGE_TYPESCRIPT, CODE_LANGUAGE_PYTHON,
                                             CODE_LANGUAGE_JSON,       CODE_LANGUAGE_SHELL,
                                             CODE_LANGUAGE_POWERSHELL, CODE_LANGUAGE_SQL};
    for (size_t at = 0; at < sizeof ids / sizeof ids[0]; ++at)
        if (listed(word.bytes, word.length, languages[slot(ids[at])].aliases, true))
            return ids[at];
    return CODE_LANGUAGE_PLAIN;
}

bool code_language_keyword(enum code_language language, const char *_Nonnull word, size_t length)
{
    size_t at = slot(language);
    return listed(word, length, languages[at].keywords, languages[at].fold_keywords);
}

const char *_Nonnull code_language_line_comment(enum code_language language)
{
    return languages[slot(language)].line;
}
const char *_Nonnull code_language_comment_open(enum code_language language)
{
    return languages[slot(language)].opening;
}
const char *_Nonnull code_language_comment_close(enum code_language language)
{
    return languages[slot(language)].closing;
}
bool code_language_feature(enum code_language language, enum code_feature feature)
{
    return (languages[slot(language)].features & (1U << feature)) != 0;
}
char code_language_escape(enum code_language language, char quote)
{
    size_t at = slot(language);
    return quote == '\'' && !languages[at].single_escape ? '\0' : languages[at].escape;
}
