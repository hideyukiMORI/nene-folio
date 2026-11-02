#include "allocation_probe.h"
#include "appearance_snapshot.h"
#include "code_language.h"
#include "code_syntax.h"
#include "markdown_rtf.h"
#include "note_text.h"
#include "text_buffer.h"
#include "theme_palette.h"
#include "unit_tests.h"
#include <stdio.h>
#include <string.h>

static void append_lines(struct code_syntax *_Nonnull syntax, struct text_buffer *_Nonnull output,
                         const char *_Nonnull source)
{
    const char *at = source;
    for (;;)
    {
        const char *end = strchr(at, '\n');
        size_t count = end == nullptr ? strlen(at) : (size_t)(end - at);
        code_syntax_append(syntax, at, count);
        if (end == nullptr)
            return;
        text_buffer_append_text(output, "\n");
        at = end + 1;
    }
}

static void expect(const char *_Nonnull language, const char *_Nonnull source,
                   const char *_Nonnull expected)
{
    struct text_buffer *output = nullptr;
    struct code_syntax *syntax = nullptr;
    require(text_buffer_create(&output) == TEXT_BUFFER_ACCEPTED, "syntax output");
    enum code_language kind = code_language_parse(language, strlen(language));
    require(code_syntax_create(kind, output, &syntax) == MARKDOWN_RTF_CONVERTED, "syntax owner");
    append_lines(syntax, output, source);
    code_syntax_destroy(syntax);
    require(text_buffer_finish(output) == TEXT_BUFFER_ACCEPTED, "syntax complete");
    if (!same_text(text_buffer_bytes(output), expected))
        fprintf(stderr, "syntax %s\nsource: %s\nexpected: %s\nactual: %s\n", language, source,
                expected, text_buffer_bytes(output));
    require(same_text(text_buffer_bytes(output), expected), "language token boundaries");
    text_buffer_destroy(output);
}

static void aliases(void)
{
    static const struct
    {
        const char *name;
        enum code_language language;
    } cases[] = {{"", CODE_LANGUAGE_PLAIN},
                 {"unknown", CODE_LANGUAGE_PLAIN},
                 {"text", CODE_LANGUAGE_PLAIN},
                 {"txt", CODE_LANGUAGE_PLAIN},
                 {"plaintext", CODE_LANGUAGE_PLAIN},
                 {"c", CODE_LANGUAGE_C},
                 {"cpp", CODE_LANGUAGE_CPP},
                 {"c++", CODE_LANGUAGE_CPP},
                 {"cc", CODE_LANGUAGE_CPP},
                 {"cxx", CODE_LANGUAGE_CPP},
                 {"javascript", CODE_LANGUAGE_JAVASCRIPT},
                 {"js", CODE_LANGUAGE_JAVASCRIPT},
                 {"typescript", CODE_LANGUAGE_TYPESCRIPT},
                 {"ts", CODE_LANGUAGE_TYPESCRIPT},
                 {"python", CODE_LANGUAGE_PYTHON},
                 {"py", CODE_LANGUAGE_PYTHON},
                 {"python3", CODE_LANGUAGE_PYTHON},
                 {"json", CODE_LANGUAGE_JSON},
                 {"sh", CODE_LANGUAGE_SHELL},
                 {"shell", CODE_LANGUAGE_SHELL},
                 {"bash", CODE_LANGUAGE_SHELL},
                 {"zsh", CODE_LANGUAGE_SHELL},
                 {"powershell", CODE_LANGUAGE_POWERSHELL},
                 {"ps1", CODE_LANGUAGE_POWERSHELL},
                 {"pwsh", CODE_LANGUAGE_POWERSHELL},
                 {"sql", CODE_LANGUAGE_SQL},
                 {" \tPyThOn extra", CODE_LANGUAGE_PYTHON},
                 {"C++\tmore", CODE_LANGUAGE_CPP},
                 {"typescript2", CODE_LANGUAGE_PLAIN},
                 {"\t \t", CODE_LANGUAGE_PLAIN}};
    for (size_t at = 0; at < sizeof cases / sizeof cases[0]; ++at)
        require(code_language_parse(cases[at].name, strlen(cases[at].name)) == cases[at].language,
                "language alias and info boundary");
    require(code_language_parse("pythonX", 6) == CODE_LANGUAGE_PYTHON,
            "info parser uses the borrowed length");
    require(code_language_parse("py\0thon", 7) == CODE_LANGUAGE_PLAIN,
            "embedded NUL cannot hide an unknown language suffix");
}

static void basic_languages(void)
{
    expect("c", "int value = 0x2A; // comment",
           "{\\cf7 int} value = {\\cf10 0x2A}; {\\cf9 // comment}");
    expect("cpp", "constexpr auto value = nullptr;",
           "{\\cf7 constexpr} {\\cf7 auto} value = {\\cf7 nullptr};");
    expect("js", "const url = \"https://x\"; true",
           "{\\cf7 const} url = {\\cf8 \"https://x\"}; {\\cf7 true}");
    expect("ts", "interface Box { value: number }",
           "{\\cf7 interface} Box \\{ value: {\\cf7 number} \\}");
    expect("python", "def f(): return True # hi",
           "{\\cf7 def} f(): {\\cf7 return} {\\cf7 True} {\\cf9 # hi}");
    expect("json", "{\"a\": -1.2e+3, \"b\": null}",
           "\\{{\\cf8 \"a\"}: -{\\cf10 1.2e+3}, {\\cf8 \"b\"}: {\\cf7 null}\\}");
    expect("sh", "if true; then echo 'ok'; fi # end",
           "{\\cf7 if} {\\cf7 true}; {\\cf7 then} echo {\\cf8 'ok'}; {\\cf7 fi} {\\cf9 # end}");
    expect("pwsh", "IF ($true) { return 42 } # done",
           "{\\cf7 IF} ({\\cf7 $true}) \\{ {\\cf7 return} {\\cf10 42} \\} {\\cf9 # done}");
    expect("sql", "SELECT * FROM t WHERE x = 'it''s';",
           "{\\cf7 SELECT} * {\\cf7 FROM} t {\\cf7 WHERE} x = {\\cf8 'it''s'};");
    expect("other", "int {日本語} \\path", "int \\{日本語\\} \\\\path");
    expect("c", "integer int2 _int 日本語int", "integer int2 _int 日本語int");
    expect("python", "true True FALSE False", "true {\\cf7 True} FALSE {\\cf7 False}");
    expect("json", "// # /* hi */", "// # /* hi */");
}

static void continued_tokens(void)
{
    expect("c", "/* \"first\nsecond */ int", "{\\cf9 /* \"first}\n{\\cf9 second */} {\\cf7 int}");
    expect("c", "\"/* not comment */\" // \"quote",
           "{\\cf8 \"/* not comment */\"} {\\cf9 // \"quote}");
    expect("c", "\"open\nint", "{\\cf8 \"open}\n{\\cf7 int}");
    expect("c", "\"a\\\nend\" int", "{\\cf8 \"a\\\\}\n{\\cf8 end\"} {\\cf7 int}");
    expect("c", "\"a\\\n\nint", "{\\cf8 \"a\\\\}\n\n{\\cf7 int}");
    expect("python", "'''first\n\n#text''' return",
           "{\\cf8 '''first}\n\n{\\cf8 #text'''} {\\cf7 return}");
    expect("python", "\"\"\"a\"b\"\"\" True", "{\\cf8 \"\"\"a\"b\"\"\"} {\\cf7 True}");
    expect("js", "`a ${x}\n// b` const", "{\\cf8 `a $\\{x\\}}\n{\\cf8 // b`} {\\cf7 const}");
    expect("bash", "'one\n#two' if", "{\\cf8 'one}\n{\\cf8 #two'} {\\cf7 if}");
    expect("bash", "echo foo#bar \\#literal #comment",
           "echo foo#bar \\\\#literal {\\cf9 #comment}");
    expect("pwsh", "<# a\n# b #> if", "{\\cf9 <# a}\n{\\cf9 # b #>} {\\cf7 if}");
    expect("pwsh", "@'\n\"#text\n'@; return",
           "{\\cf8 @'}\n{\\cf8 \"#text}\n{\\cf8 '@}; {\\cf7 return}");
    expect("pwsh", "@\"\n\n' here\n\"@", "{\\cf8 @\"}\n\n{\\cf8 ' here}\n{\\cf8 \"@}");
    expect("pwsh", "\"a`\"b\" 'it''s'", "{\\cf8 \"a`\"b\"} {\\cf8 'it''s'}");
    expect("sql", "/* outer /* nested */ end */ SELECT",
           "{\\cf9 /* outer /* nested */ end */} {\\cf7 SELECT}");
    expect("sql", "\"some\"\"name\" 'x\ny'", "{\\cf8 \"some\"\"name\"} {\\cf8 'x}\n{\\cf8 y'}");
}

static void colors(void)
{
    struct theme_palette base = *folio_theme_palette(FOLIO_THEME_LIGHT);
    struct theme_palette changed = base;
    require(theme_palette_equal(&base, &changed), "equal syntax palette");
    struct rgb_color *roles[] = {&changed.rtf.code_keyword, &changed.rtf.code_string,
                                 &changed.rtf.code_comment, &changed.rtf.code_number};
    for (size_t at = 0; at < 4; ++at)
    {
        *roles[at] = (struct rgb_color){0, 0, 0};
        require(!theme_palette_equal(&base, &changed),
                "syntax role participates in palette equality");
        changed = base;
    }
}

static void markdown_case(const char *_Nonnull source, const char *_Nonnull present,
                          const char *_Nonnull absent)
{
    struct note_text *text = nullptr;
    struct markdown_rtf *rtf = nullptr;
    require(note_text_create(source, strlen(source), &text) == NOTE_TEXT_ACCEPTED, "fence source");
    require(markdown_rtf_create(text, rtf_palette_for(FOLIO_THEME_LIGHT), "Consolas", &rtf) ==
                MARKDOWN_RTF_CONVERTED,
            "fence syntax conversion");
    require(strstr(markdown_rtf_text(rtf), present) != nullptr, "fence recognizes language");
    require(strstr(markdown_rtf_text(rtf), absent) == nullptr, "fence does not leak syntax");
    require(same_text(note_text_bytes(text), source), "render preserves original Markdown");
    markdown_rtf_destroy(rtf);
    require(markdown_rtf_plain(text, rtf_palette_for(FOLIO_THEME_LIGHT), "Consolas", &rtf) ==
                MARKDOWN_RTF_CONVERTED,
            "plain conversion after syntax");
    require(strstr(markdown_rtf_text(rtf), "{\\cf7 ") == nullptr, "editing stays plain");
    markdown_rtf_destroy(rtf);
    note_text_destroy(text);
}

static void fence_boundaries(void)
{
    markdown_case("```c\n/* open\n```\n```c\nint\n```\nint\n", "{\\cf7 int}", "{\\cf9 int}");
    markdown_case("> ```PY extra\n> def f():\n>   return 1\n> ```\nreturn\n", "{\\cf7 def}",
                  "{\\cf7 extra}");
    markdown_case("- ~~~c\n  int\n  ~~~\n\nint\n", "{\\cf7 int}", "{\\cf7 ~}");
    markdown_case("```sql\nSELECT", "{\\cf7 SELECT}", "{\\cf9 SELECT}");
    markdown_case("```unknown\nint\n```\n`int`\n\nint", "int", "{\\cf7 ");
    markdown_case("```\nint\n```", "int", "{\\cf7 ");
    markdown_case("> ```c\n> /* open\n\n```c\nint\n```", "{\\cf7 int}", "{\\cf9 int}");
}

static void large_input(void)
{
    char source[65537];
    const size_t length = sizeof source - 1;
    memset(source, 'x', length);
    source[length] = '\0';
    expect("c", source, source);
    memset(source, '/', length);
    source[1] = '*';
    struct text_buffer *output = nullptr;
    struct code_syntax *syntax = nullptr;
    require(text_buffer_create(&output) == TEXT_BUFFER_ACCEPTED, "large output");
    require(code_syntax_create(CODE_LANGUAGE_C, output, &syntax) == MARKDOWN_RTF_CONVERTED,
            "large syntax");
    code_syntax_append(syntax, source, length);
    require(text_buffer_finish(output) == TEXT_BUFFER_ACCEPTED, "large comment complete");
    require(strlen(text_buffer_bytes(output)) == length + 7 &&
                memcmp(text_buffer_bytes(output) + 6, source, length) == 0,
            "long unterminated comment is bounded and intact");
    code_syntax_destroy(syntax);
    text_buffer_destroy(output);
}

void run_code_syntax_tests(void)
{
    aliases();
    basic_languages();
    continued_tokens();
    colors();
    fence_boundaries();
    large_input();
    code_syntax_destroy(nullptr);
    puts("code syntax tests passed");
}
