#include "name_list.h"
#include "rgb_color.h"
#include "unit_tests.h"
#include "utf16_text.h"
#include "utf8_text.h"

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
    verify_colors();
    verify_name_list();
}
