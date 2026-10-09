#include "allocation_probe.h"
#include "name_list.h"
#include "note_text.h"
#include "persisted_size_limit.h"
#include "rgb_color.h"
#include "unit_tests.h"
#include "utf16_text.h"
#include "utf8_text.h"

#include <stdlib.h>
#include <string.h>

static void verify_utf8_decode(void)
{
    uint32_t code_point = 0;
    require(utf8_text_decode("A", 1, &code_point) == 1 && code_point == 0x41, "ascii");
    require(utf8_text_decode("\xC3\xA9", 2, &code_point) == 2 && code_point == 0xE9, "2 bytes");
    require(utf8_text_decode("\xE6\x97\xA5", 3, &code_point) == 3 && code_point == 0x65E5,
            "3 bytes");
    require(utf8_text_decode("\xF0\x9F\x98\x80", 4, &code_point) == 4 && code_point == 0x1F600,
            "4 bytes");
    require(utf8_text_decode("", 0, &code_point) == 0, "empty");
    require(utf8_text_decode("\x80", 1, &code_point) == 0, "lone continuation");
    require(utf8_text_decode("\xC0\x80", 2, &code_point) == 0, "overlong 2 bytes");
    require(utf8_text_decode("\xE0\x80\x80", 3, &code_point) == 0, "overlong 3 bytes");
    require(utf8_text_decode("\xED\xA0\x80", 3, &code_point) == 0, "surrogate");
    require(utf8_text_decode("\xE6\x97", 2, &code_point) == 0, "truncated");
    require(utf8_text_decode("\xE6\x41\xA5", 3, &code_point) == 0, "bad continuation");
    require(utf8_text_decode("\xF4\x90\x80\x80", 4, &code_point) == 0, "above max");
    require(utf8_text_decode("\xF5\x80\x80\x80", 4, &code_point) == 0, "invalid lead");
}

static void verify_utf8_encode(void)
{
    char out[utf8_text_max_encoded];
    require(utf8_text_encode(0x41, out) == 1 && out[0] == 'A', "encode ascii");
    require(utf8_text_encode(0xE9, out) == 2 && memcmp(out, "\xC3\xA9", 2) == 0, "encode 2");
    require(utf8_text_encode(0x65E5, out) == 3 && memcmp(out, "\xE6\x97\xA5", 3) == 0, "encode 3");
    require(utf8_text_encode(0x1F600, out) == 4 && memcmp(out, "\xF0\x9F\x98\x80", 4) == 0,
            "encode 4");
    require(utf8_text_encode(0xD800, out) == 0, "encode surrogate");
    require(utf8_text_encode(0xFFFD, out) == 3 && memcmp(out, "\xEF\xBF\xBD", 3) == 0,
            "encode above surrogates");
    require(utf8_text_encode(0x110000, out) == 0, "encode above max");
}

static void verify_utf16_from_utf8(void)
{
    struct utf16_text *text = nullptr;
    const char *source = "\xE6\x97\xA5\xF0\x9F\x98\x80"
                         "A";
    require(utf16_text_create(source, strlen(source), &text) == UTF16_TEXT_CONVERTED, "utf16 ok");
    const char16_t *units = utf16_text_units(text);
    require(utf16_text_length(text) == 4, "utf16 length");
    require(units[0] == 0x65E5 && units[1] == 0xD83D && units[2] == 0xDE00 && units[3] == u'A' &&
                units[4] == u'\0',
            "utf16 units");
    utf16_text_destroy(text);
    require(utf16_text_create("\xC3", 1, &text) == UTF16_TEXT_INVALID_UTF8, "utf16 invalid");
    require(utf16_text_create("", 0, &text) == UTF16_TEXT_CONVERTED &&
                utf16_text_length(text) == 0 && utf16_text_units(text)[0] == u'\0',
            "utf16 empty");
    utf16_text_destroy(text);
    utf16_text_destroy(nullptr);
}

static void verify_utf8_from_utf16(void)
{
    struct utf8_text *text = nullptr;
    const char16_t source[] = {0x65E5, 0xD83D, 0xDE00, u'A'};
    require(utf8_text_create(source, 4, &text) == UTF8_TEXT_CONVERTED, "utf8 ok");
    require(utf8_text_length(text) == 8, "utf8 length");
    require(same_text(utf8_text_bytes(text), "\xE6\x97\xA5\xF0\x9F\x98\x80"
                                             "A"),
            "utf8 bytes");
    utf8_text_destroy(text);
    const char16_t lone_high[] = {0xD83D};
    require(utf8_text_create(lone_high, 1, &text) == UTF8_TEXT_INVALID_UTF16, "lone high");
    const char16_t lone_low[] = {0xDE00};
    require(utf8_text_create(lone_low, 1, &text) == UTF8_TEXT_INVALID_UTF16, "lone low");
    const char16_t bad_pair[] = {0xD83D, u'A'};
    require(utf8_text_create(bad_pair, 2, &text) == UTF8_TEXT_INVALID_UTF16, "bad pair");
    const char16_t high_then_bmp[] = {0xD83D, 0xFFFF};
    require(utf8_text_create(high_then_bmp, 2, &text) == UTF8_TEXT_INVALID_UTF16, "high then bmp");
    const char16_t trailing_high[] = {u'A', 0xD83D};
    require(utf8_text_create(trailing_high, 2, &text) == UTF8_TEXT_INVALID_UTF16, "trailing high");
    require(utf8_text_create(bad_pair, 0, &text) == UTF8_TEXT_CONVERTED &&
                utf8_text_length(text) == 0,
            "utf8 empty");
    utf8_text_destroy(text);
    utf8_text_destroy(nullptr);
}

/* 本文を作って改行の形を読む。 */
static enum line_ending ending_of(const char *_Nonnull source)
{
    struct note_text *text = nullptr;
    require(note_text_create(source, strlen(source), &text) == NOTE_TEXT_ACCEPTED, "note create");
    enum line_ending ending = note_text_line_ending(text);
    note_text_destroy(text);
    return ending;
}

static void verify_line_ending(void)
{
    require(ending_of("a\nb") == LINE_ENDING_LF, "lf body");
    require(ending_of("a\r\nb") == LINE_ENDING_CRLF, "crlf body");
    require(ending_of("no break at all") == LINE_ENDING_LF, "no break is lf");
    require(ending_of("") == LINE_ENDING_LF, "empty is lf");
    require(ending_of("a\r\nb\nc") == LINE_ENDING_CRLF, "the first break decides");
    require(ending_of("a\nb\r\nc") == LINE_ENDING_LF, "the first break decides both ways");
}

/* 編集後の本文を ending に揃え、できたバイト列を確かめる。 */
static void expect_folded(const char *_Nonnull edited, enum line_ending ending,
                          const char *_Nonnull expected)
{
    struct note_text *text = nullptr;
    require(note_text_from_editor(edited, strlen(edited), ending, &text) == NOTE_TEXT_ACCEPTED,
            "from editor");
    require(same_text(note_text_bytes(text), expected), "folded body");
    require(note_text_length(text) == strlen(expected), "folded length");
    note_text_destroy(text);
}

static void verify_from_editor(void)
{
    expect_folded("a\r\nb\rc\nd", LINE_ENDING_LF, "a\nb\nc\nd");
    expect_folded("a\r\nb\rc\nd", LINE_ENDING_CRLF, "a\r\nb\r\nc\r\nd");
    expect_folded("x\r\n", LINE_ENDING_LF, "x\n");
    expect_folded("x\n", LINE_ENDING_CRLF, "x\r\n");
    expect_folded("x", LINE_ENDING_LF, "x");
    expect_folded("x", LINE_ENDING_CRLF, "x");
    expect_folded("", LINE_ENDING_CRLF, "");
    expect_folded("\r", LINE_ENDING_LF, "\n");
    expect_folded("a\r", LINE_ENDING_CRLF, "a\r\n");
    expect_folded("\xEF\xBB\xBF"
                  "a\nb",
                  LINE_ENDING_LF, "a\nb");
    expect_folded("\xE6\x97\xA5\r\n\xE6\x9C\xAC", LINE_ENDING_LF, "\xE6\x97\xA5\n\xE6\x9C\xAC");
    struct note_text *text = nullptr;
    require(note_text_from_editor("\xC3", 1, LINE_ENDING_LF, &text) == NOTE_TEXT_INVALID_UTF8,
            "invalid utf8 from the editor");
    require(note_text_from_editor("a\xED\xA0\x80", 4, LINE_ENDING_LF, &text) ==
                NOTE_TEXT_INVALID_UTF8,
            "surrogate from the editor");
}

static void expect_first_line(const char *_Nonnull source, const char *_Nonnull expected,
                              const char *_Nonnull description)
{
    struct note_text *text = nullptr;
    require(note_text_create(source, strlen(source), &text) == NOTE_TEXT_ACCEPTED, description);
    size_t start = 99;
    size_t length = 99;
    note_text_first_line(text, &start, &length);
    require(length == strlen(expected), description);
    require(start + length <= note_text_length(text), description);
    require(memcmp(note_text_bytes(text) + start, expected, length) == 0, description);
    if (length == 0)
    {
        require(start == note_text_length(text), description);
    }
    note_text_destroy(text);
}

/* 履歴の一覧の見出し（ADR 0038 の決定 8・検証 3）。 */
static void verify_first_line(void)
{
    expect_first_line("", "", "empty body has no first line");
    expect_first_line("\n\r\n\r", "", "only line breaks");
    expect_first_line("  \t\n \r\n\t", "", "only blank lines");
    expect_first_line("single", "single", "one line without a trailing break");
    expect_first_line("first\nsecond", "first", "LF");
    expect_first_line("first\r\nsecond", "first", "CRLF");
    expect_first_line("first\rsecond", "first", "CR");
    expect_first_line("\n\nthird\n", "third", "leading empty lines (LF)");
    expect_first_line("\r\n\r\nthird\r\n", "third", "leading empty lines (CRLF)");
    expect_first_line("\r\rthird", "third", "leading empty lines (CR)");
    expect_first_line("  \n\t# heading \t\nbody", "# heading", "blank line and trimmed ends");
    expect_first_line("\xE6\x97\xA5\xE6\x9C\xAC\r\n", "\xE6\x97\xA5\xE6\x9C\xAC", "UTF-8 line");
}

static void verify_note_equals(void)
{
    struct note_text *one = nullptr;
    struct note_text *same = nullptr;
    struct note_text *other = nullptr;
    struct note_text *longer = nullptr;
    require(note_text_create("a\nb", 3, &one) == NOTE_TEXT_ACCEPTED, "one");
    require(note_text_create("a\nb", 3, &same) == NOTE_TEXT_ACCEPTED, "same");
    require(note_text_create("a\nc", 3, &other) == NOTE_TEXT_ACCEPTED, "other");
    require(note_text_create("a\nbb", 4, &longer) == NOTE_TEXT_ACCEPTED, "longer");
    require(note_text_equals(one, same), "equal bodies");
    require(!note_text_equals(one, other), "different bytes");
    require(!note_text_equals(one, longer), "different lengths");
    note_text_destroy(longer);
    note_text_destroy(other);
    note_text_destroy(same);
    note_text_destroy(one);
    note_text_destroy(nullptr);
}

static void verify_colors(void)
{
    struct rgb_color color = {0, 0, 0};
    require(rgb_color_parse_hex("#3D7EFF", 7, &color) == RGB_COLOR_PARSED && color.red == 0x3D &&
                color.green == 0x7E && color.blue == 0xFF,
            "parse upper");
    require(rgb_color_parse_hex("#3d7eff", 7, &color) == RGB_COLOR_PARSED && color.blue == 0xFF,
            "parse lower");
    require(rgb_color_parse_hex("3D7EFF", 6, &color) == RGB_COLOR_MALFORMED, "no hash");
    require(rgb_color_parse_hex("03D7EFF", 7, &color) == RGB_COLOR_MALFORMED, "wrong prefix");
    require(rgb_color_parse_hex("#GG0000", 7, &color) == RGB_COLOR_MALFORMED, "bad digit");
    require(rgb_color_parse_hex("#3D7EFF0", 8, &color) == RGB_COLOR_MALFORMED, "too long");
    require(rgb_color_parse_hex("#-12345", 7, &color) == RGB_COLOR_MALFORMED, "below digits");
    require(rgb_color_parse_hex("#::0000", 7, &color) == RGB_COLOR_MALFORMED, "between 9 and A");
    require(rgb_color_parse_hex("#zz0000", 7, &color) == RGB_COLOR_MALFORMED, "above f");
    require(rgb_color_parse_hex("#0G0000", 7, &color) == RGB_COLOR_MALFORMED, "bad low digit");
    require(rgb_color_parse_hex("#00GG00", 7, &color) == RGB_COLOR_MALFORMED, "bad green");
    require(rgb_color_parse_hex("#0000GG", 7, &color) == RGB_COLOR_MALFORMED, "bad blue");
    char hex[rgb_color_hex_length + 1] = {0};
    rgb_color_write_hex(color, hex);
    require(same_text(hex, "#3D7EFF"), "write hex");
}

static void verify_name_rules(struct name_list *list)
{
    require(name_list_append(list, "", 0) == NAME_LIST_INVALID_NAME, "empty name");
    require(name_list_append(list, "a/b", 3) == NAME_LIST_INVALID_NAME, "slash");
    require(name_list_append(list, "a\\b", 3) == NAME_LIST_INVALID_NAME, "backslash");
    require(name_list_append(list, "a:b", 3) == NAME_LIST_INVALID_NAME, "colon");
    require(name_list_append(list, ".", 1) == NAME_LIST_INVALID_NAME, "dot");
    require(name_list_append(list, "..", 2) == NAME_LIST_INVALID_NAME, "dot dot");
    require(name_list_append(list, "name.", 5) == NAME_LIST_INVALID_NAME, "trailing dot");
    require(name_list_append(list, "name ", 5) == NAME_LIST_INVALID_NAME, "trailing space");
    require(name_list_append(list, "a\tb", 3) == NAME_LIST_INVALID_NAME, "control");
    require(name_list_append(list, "\xC3", 1) == NAME_LIST_INVALID_NAME, "invalid utf8");
    char longest[name_list_max_length + 1];
    memset(longest, 'x', sizeof longest);
    require(name_list_append(list, longest, sizeof longest) == NAME_LIST_INVALID_NAME, "too long");
    require(name_list_append(list, longest, name_list_max_length) == NAME_LIST_ACCEPTED,
            "longest accepted");
}

static void verify_name_list(void)
{
    struct name_list *list = nullptr;
    require(name_list_create(&list) == NAME_LIST_ACCEPTED, "name list create");
    require(name_list_count(list) == 0, "empty count");
    require(name_list_append(list, "\xE4\xBB\x95\xE4\xBA\x8B", 6) == NAME_LIST_ACCEPTED,
            "japanese");
    require(name_list_append(list, "\xE4\xBB\x95\xE4\xBA\x8B", 6) == NAME_LIST_DUPLICATE,
            "duplicate");
    require(name_list_append(list, "a.b c", 5) == NAME_LIST_ACCEPTED, "inner dot and space");
    require(name_list_append(list, ".a", 2) == NAME_LIST_ACCEPTED, "leading dot");
    require(name_list_append(list, "..a", 3) == NAME_LIST_ACCEPTED, "leading dots");
    for (size_t index = 0; index < 8; ++index)
    {
        char name[2] = {(char)('A' + index), '\0'};
        require(name_list_append(list, name, 1) == NAME_LIST_ACCEPTED, "growth");
    }
    require(name_list_count(list) == 12, "count after growth");
    require(same_text(name_list_at(list, 1), "a.b c"), "at");
    require(name_list_contains(list, "H") && !name_list_contains(list, "I"), "contains");
    require(!name_list_contains(list, "a.b"), "prefix is not contained");
    verify_name_rules(list);
    name_list_destroy(list);
    name_list_destroy(nullptr);
}

void run_text_tests(void)
{
    verify_utf8_decode();
    verify_utf8_encode();
    verify_utf16_from_utf8();
    verify_utf8_from_utf16();
    verify_line_ending();
    verify_from_editor();
    verify_note_equals();
    verify_first_line();
    verify_colors();
    verify_name_list();
}

static char *_Nonnull size_bytes(void)
{
    char *bytes = malloc(persisted_size_limit + 4);
    if (bytes == nullptr)
    {
        require(false, "size boundary fixture allocated");
        exit(1);
    }
    memset(bytes, 'X', persisted_size_limit + 4);
    return bytes;
}

static void verify_create_size(void)
{
    char *bytes = size_bytes();
    struct note_text *text = nullptr;
    for (size_t length = persisted_size_limit - 1; length <= persisted_size_limit; ++length)
    {
        require(note_text_create(bytes, length, &text) == NOTE_TEXT_ACCEPTED &&
                    note_text_length(text) == length,
                "canonical limit minus one and limit accepted");
        note_text_destroy(text);
    }
    struct note_text *sentinel = nullptr;
    require(note_text_create("M", 1, &sentinel) == NOTE_TEXT_ACCEPTED, "size sentinel");
    text = sentinel;
    require(note_text_create(bytes, persisted_size_limit + 1, &text) == NOTE_TEXT_TOO_LARGE &&
                text == sentinel,
            "canonical limit plus one rejected; output unchanged");
    bytes[persisted_size_limit] = (char)0x80;
    require(note_text_create(bytes, persisted_size_limit + 1, &text) == NOTE_TEXT_INVALID_UTF8 &&
                text == sentinel,
            "malformed UTF8 precedes oversized create");
    require(note_text_from_editor(bytes, persisted_size_limit + 1, LINE_ENDING_CRLF, &text) ==
                    NOTE_TEXT_INVALID_UTF8 &&
                text == sentinel,
            "malformed UTF8 precedes oversized normalization");
    note_text_destroy(sentinel);
    free(bytes);
}

static void verify_multibyte_bom_size(void)
{
    char *bytes = size_bytes();
    for (size_t index = 0; index + 3 <= persisted_size_limit; index += 3)
    {
        memcpy(bytes + index, "\xE6\x97\xA5", 3);
    }
    struct note_text *text = nullptr;
    require(note_text_create(bytes, persisted_size_limit, &text) == NOTE_TEXT_ACCEPTED &&
                note_text_length(text) == persisted_size_limit,
            "multibyte UTF8 is measured in bytes, not characters");
    note_text_destroy(text);
    text = nullptr;
    require(note_text_create(bytes, persisted_size_limit + 1, &text) == NOTE_TEXT_TOO_LARGE &&
                text == nullptr,
            "multibyte limit plus byte rejected");
    memmove(bytes + 3, bytes, persisted_size_limit);
    memcpy(bytes, "\xEF\xBB\xBF", 3);
    require(note_text_create(bytes, persisted_size_limit + 3, &text) == NOTE_TEXT_ACCEPTED &&
                note_text_length(text) == persisted_size_limit,
            "BOM excluded from canonical create limit");
    note_text_destroy(text);
    require(note_text_from_editor(bytes, persisted_size_limit + 3, LINE_ENDING_LF, &text) ==
                    NOTE_TEXT_ACCEPTED &&
                note_text_length(text) == persisted_size_limit,
            "editor also excludes BOM from canonical limit");
    note_text_destroy(text);
    free(bytes);
}

static void verify_normalized_size(void)
{
    char *bytes = size_bytes();
    bytes[0] = '\r';
    bytes[1] = '\n';
    struct note_text *text = nullptr;
    require(note_text_from_editor(bytes, persisted_size_limit + 1, LINE_ENDING_LF, &text) ==
                    NOTE_TEXT_ACCEPTED &&
                note_text_length(text) == persisted_size_limit && note_text_bytes(text)[0] == '\n',
            "CRLF shrinking accepts raw bytes above limit");
    note_text_destroy(text);
    text = nullptr;
    require(note_text_from_editor(bytes, persisted_size_limit + 1, LINE_ENDING_CRLF, &text) ==
                    NOTE_TEXT_TOO_LARGE &&
                text == nullptr,
            "CRLF preserved remains above limit");
    bytes[1] = 'X';
    require(note_text_from_editor(bytes, persisted_size_limit, LINE_ENDING_CRLF, &text) ==
                    NOTE_TEXT_TOO_LARGE &&
                text == nullptr,
            "single CR expanding rejects raw within limit");
    require(note_text_from_editor(bytes, persisted_size_limit - 1, LINE_ENDING_CRLF, &text) ==
                    NOTE_TEXT_ACCEPTED &&
                note_text_length(text) == persisted_size_limit &&
                note_text_bytes(text)[0] == '\r' && note_text_bytes(text)[1] == '\n',
            "expansion exactly to limit accepted");
    note_text_destroy(text);
    memset(bytes, 'X', persisted_size_limit + 1);
    bytes[persisted_size_limit - 1] = '\n';
    require(note_text_from_editor(bytes, persisted_size_limit, LINE_ENDING_LF, &text) ==
                    NOTE_TEXT_ACCEPTED &&
                note_text_bytes(text)[persisted_size_limit - 1] == '\n',
            "terminal newline at limit preserved");
    note_text_destroy(text);
    text = nullptr;
    require(note_text_from_editor(bytes, persisted_size_limit + 1, LINE_ENDING_LF, &text) ==
                    NOTE_TEXT_TOO_LARGE &&
                text == nullptr,
            "editor canonical limit plus one rejected");
    free(bytes);
}

static void verify_oversize_before_allocation(void)
{
    struct note_text *check = nullptr;
    allocation_probe_fail_at(1);
    enum note_text_outcome active = note_text_create("M", 1, &check);
    allocation_probe_fail_at(0);
    note_text_destroy(check);
    if (active != NOTE_TEXT_OUT_OF_MEMORY)
    {
        return;
    }
    char *bytes = size_bytes();
    check = nullptr;
    allocation_probe_fail_at(1);
    require(note_text_create(bytes, persisted_size_limit + 1, &check) == NOTE_TEXT_TOO_LARGE &&
                note_text_create("M", 1, &check) == NOTE_TEXT_OUT_OF_MEMORY,
            "oversized create consumes no allocation point");
    allocation_probe_fail_at(0);
    allocation_probe_fail_at(1);
    require(note_text_from_editor(bytes, persisted_size_limit + 1, LINE_ENDING_LF, &check) ==
                    NOTE_TEXT_TOO_LARGE &&
                note_text_create("M", 1, &check) == NOTE_TEXT_OUT_OF_MEMORY,
            "oversized normalization consumes no allocation point");
    allocation_probe_fail_at(0);
    free(bytes);
}

void run_note_size_text_tests(void)
{
    verify_create_size();
    verify_multibyte_bom_size();
    verify_normalized_size();
    verify_oversize_before_allocation();
}

static void verify_nul_rejected(const char *_Nonnull bytes, size_t length)
{
    struct note_text *sentinel = nullptr;
    require(note_text_create("kept", 4, &sentinel) == NOTE_TEXT_ACCEPTED, "NUL sentinel owned");
    struct note_text *text = sentinel;
    require(note_text_create(bytes, length, &text) == NOTE_TEXT_EMBEDDED_NUL && text == sentinel,
            "NUL file body rejected without changing out");
    require(note_text_from_editor(bytes, length, LINE_ENDING_LF, &text) == NOTE_TEXT_EMBEDDED_NUL &&
                text == sentinel,
            "NUL editor body rejected before LF normalization");
    require(note_text_from_editor(bytes, length, LINE_ENDING_CRLF, &text) ==
                    NOTE_TEXT_EMBEDDED_NUL &&
                text == sentinel && same_text(note_text_bytes(sentinel), "kept"),
            "NUL editor body rejected before CRLF normalization");
    note_text_destroy(sentinel);
}

static void verify_nul_codec(void)
{
    const char16_t input[] = {u'A', u'\0', u'B'};
    struct utf8_text *narrow = nullptr;
    require(utf8_text_create(input, 3, &narrow) == UTF8_TEXT_CONVERTED &&
                utf8_text_length(narrow) == 3 && memcmp(utf8_text_bytes(narrow), "A\0B", 3) == 0,
            "generic UTF-16 to UTF-8 conversion still accepts NUL");
    struct utf16_text *wide = nullptr;
    require(utf16_text_create(utf8_text_bytes(narrow), 3, &wide) == UTF16_TEXT_CONVERTED &&
                utf16_text_length(wide) == 3 &&
                memcmp(utf16_text_units(wide), input, sizeof input) == 0,
            "generic UTF-8 to UTF-16 conversion still roundtrips NUL");
    utf16_text_destroy(wide);
    utf8_text_destroy(narrow);
}

void run_note_nul_text_tests(void)
{
    verify_nul_rejected("\0AB", 3);
    verify_nul_rejected("A\0B", 3);
    verify_nul_rejected("AB\0", 3);
    verify_nul_rejected("\0", 1);
    verify_nul_rejected("\xEF\xBB\xBF\0A", 5);
    struct note_text *text = nullptr;
    require(note_text_create("\0\xC3", 2, &text) == NOTE_TEXT_INVALID_UTF8 && text == nullptr &&
                note_text_from_editor("\0\xC3", 2, LINE_ENDING_LF, &text) ==
                    NOTE_TEXT_INVALID_UTF8 &&
                text == nullptr,
            "invalid UTF-8 keeps precedence over NUL");
    require(note_text_create("AB\0", 2, &text) == NOTE_TEXT_ACCEPTED && note_text_length(text) == 2,
            "the terminator outside the explicit length is not body data");
    note_text_destroy(text);
    expect_folded("", LINE_ENDING_LF, "");
    expect_folded("\xE6\x97\xA5\r\nB", LINE_ENDING_CRLF, "\xE6\x97\xA5\r\nB");
    verify_nul_codec();
}
