/* #311 宣言長の算術境界。最大fitは測定buildの確保拒否でだけ観測し、巨大実確保をしない。 */
#include "allocation_probe.h"
#include "note_replace.h"
#include "regex_matches.h"
#include "replace_edit.h"
#include "replace_template.h"
#include "text_buffer.h"
#include "unit_tests.h"
#include "utf16_text.h"
#include "utf8_text.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* 正典buildではcoreの確保はprobeを通らない。巨大fit要求をそこへ送らない。 */
static bool probe_active(void)
{
    struct text_buffer *buffer = nullptr;
    allocation_probe_fail_at(1);
    enum text_buffer_outcome outcome = text_buffer_create(&buffer);
    bool active = outcome == TEXT_BUFFER_OUT_OF_MEMORY && allocation_probe_calls() == 1;
    allocation_probe_fail_at(0);
    text_buffer_destroy(buffer);
    return active;
}

static void verify_utf8_bounds(bool measured)
{
    struct utf8_text *kept = nullptr;
    require(utf8_text_create(u"kept", 4, &kept) == UTF8_TEXT_CONVERTED && kept != nullptr,
            "UTF-8 output sentinel is a real owned object");
    const size_t too_large[] = {(SIZE_MAX - 1) / 3 + 1, SIZE_MAX};
    for (size_t index = 0; index < sizeof too_large / sizeof too_large[0]; ++index)
    {
        struct utf8_text *out = kept;
        allocation_probe_fail_at(1);
        require(utf8_text_create(u"x", too_large[index], &out) == UTF8_TEXT_OUT_OF_MEMORY &&
                    out == kept && allocation_probe_calls() == 0,
                "UTF-8 overflow refuses before allocation or reading declared units");
        allocation_probe_fail_at(0);
    }
    if (measured)
    {
        struct utf8_text *out = kept;
        size_t fit = (SIZE_MAX - 1) / 3;
        allocation_probe_fail_at(1);
        require(utf8_text_create(u"x", fit, &out) == UTF8_TEXT_OUT_OF_MEMORY && out == kept &&
                    allocation_probe_calls() == 1 && allocation_probe_malloc_size() == fit * 3 + 1,
                "UTF-8 maximum fit reaches only the deliberately failing exact allocator");
        allocation_probe_fail_at(0);
    }
    require(same_text(utf8_text_bytes(kept), "kept"), "UTF-8 sentinel content survives refusal");
    utf8_text_destroy(kept);
}

static void verify_utf16_bounds(bool measured)
{
    struct utf16_text *kept = nullptr;
    require(utf16_text_create("kept", 4, &kept) == UTF16_TEXT_CONVERTED && kept != nullptr,
            "UTF-16 output sentinel is a real owned object");
    const size_t too_large[] = {SIZE_MAX / sizeof(char16_t), SIZE_MAX};
    for (size_t index = 0; index < sizeof too_large / sizeof too_large[0]; ++index)
    {
        struct utf16_text *out = kept;
        allocation_probe_fail_at(1);
        require(utf16_text_create("x", too_large[index], &out) == UTF16_TEXT_OUT_OF_MEMORY &&
                    out == kept && allocation_probe_calls() == 0,
                "UTF-16 overflow refuses before allocation or reading declared bytes");
        allocation_probe_fail_at(0);
    }
    if (measured)
    {
        struct utf16_text *out = kept;
        size_t fit = SIZE_MAX / sizeof(char16_t) - 1;
        allocation_probe_fail_at(1);
        require(utf16_text_create("x", fit, &out) == UTF16_TEXT_OUT_OF_MEMORY && out == kept &&
                    allocation_probe_calls() == 1 &&
                    allocation_probe_malloc_size() == (fit + 1) * sizeof(char16_t),
                "UTF-16 maximum fit reaches only the deliberately failing exact allocator");
        allocation_probe_fail_at(0);
    }
    require(utf16_text_length(kept) == 4 &&
                memcmp(utf16_text_units(kept), u"kept", 5 * sizeof(char16_t)) == 0,
            "UTF-16 sentinel content survives refusal");
    utf16_text_destroy(kept);
}

static struct text_buffer *_Nonnull buffer_for(const char *_Nonnull initial)
{
    struct text_buffer *buffer = nullptr;
    require(text_buffer_create(&buffer) == TEXT_BUFFER_ACCEPTED && buffer != nullptr,
            "buffer is genuinely constructed");
    text_buffer_append_text(buffer, initial);
    return buffer;
}

static void verify_buffer_overflow(void)
{
    struct text_buffer *buffer = buffer_for("abc");
    const char *kept = text_buffer_bytes(buffer);
    allocation_probe_fail_at(1);
    text_buffer_append(buffer, "x", SIZE_MAX - 3);
    require(text_buffer_finish(buffer) == TEXT_BUFFER_OUT_OF_MEMORY &&
                allocation_probe_calls() == 0 && text_buffer_bytes(buffer) == kept &&
                text_buffer_length(buffer) == 3 && same_text(kept, "abc"),
            "buffer overflow preserves owned storage and bytes before reading input");
    text_buffer_append_text(buffer, "later");
    text_buffer_append(buffer, "", 0);
    require(text_buffer_finish(buffer) == TEXT_BUFFER_OUT_OF_MEMORY &&
                allocation_probe_calls() == 0 && text_buffer_length(buffer) == 3 &&
                text_buffer_bytes(buffer) == kept && same_text(kept, "abc"),
            "overflow is sticky even for subsequent small and empty appends");
    allocation_probe_fail_at(0);
    text_buffer_destroy(buffer);
}

static void verify_buffer_growth(bool measured)
{
    char filled[300];
    memset(filled, 'a', sizeof filled);
    struct text_buffer *buffer = buffer_for("");
    const char *kept = text_buffer_bytes(buffer);
    allocation_probe_fail_at(0);
    text_buffer_append(buffer, filled, 255);
    require(text_buffer_finish(buffer) == TEXT_BUFFER_ACCEPTED &&
                text_buffer_bytes(buffer) == kept && text_buffer_length(buffer) == 255 &&
                allocation_probe_calls() == 0,
            "initial capacity includes the terminator and accepts exactly 255 bytes");
    text_buffer_append(buffer, "b", 1);
    require(text_buffer_finish(buffer) == TEXT_BUFFER_ACCEPTED &&
                text_buffer_length(buffer) == 256 && text_buffer_bytes(buffer)[255] == 'b' &&
                text_buffer_bytes(buffer)[256] == '\0' &&
                memcmp(text_buffer_bytes(buffer), filled, 255) == 0,
            "ordinary growth preserves all bytes and the terminator");
    if (measured)
    {
        require(allocation_probe_calls() == 1 && allocation_probe_realloc_size() == 513,
                "ordinary growth retains the canonical capacity doubling request");
    }
    allocation_probe_fail_at(0);
    text_buffer_destroy(buffer);
}

static void verify_buffer_failed_growth(void)
{
    struct text_buffer *buffer = buffer_for("abc");
    const char *kept = text_buffer_bytes(buffer);
    allocation_probe_fail_at(1);
    text_buffer_append(buffer, "x", SIZE_MAX - 4);
    require(text_buffer_finish(buffer) == TEXT_BUFFER_OUT_OF_MEMORY &&
                allocation_probe_calls() == 1 && allocation_probe_realloc_size() == SIZE_MAX &&
                text_buffer_bytes(buffer) == kept && text_buffer_length(buffer) == 3 &&
                same_text(kept, "abc"),
            "overflowing growth preference falls back to exact fitting requirement");
    text_buffer_append_text(buffer, "later");
    require(allocation_probe_calls() == 1 && text_buffer_bytes(buffer) == kept &&
                text_buffer_length(buffer) == 3,
            "failed exact growth is sticky and does not retry allocation");
    allocation_probe_fail_at(0);
    text_buffer_destroy(buffer);
}

static struct replace_template *_Nonnull replacement_for(const char16_t *_Nonnull units,
                                                         size_t length)
{
    struct replace_template *replacement = nullptr;
    require(replace_template_create(units, length, &replacement) == REPLACE_TEMPLATE_READY &&
                replacement != nullptr,
            "replacement template is genuinely constructed");
    return replacement;
}

static struct replace_edit *_Nonnull edit_sentinel(void)
{
    struct replace_edit *edit = nullptr;
    require(replace_edit_create((struct note_search_span){0, 1}, 1, &edit) == REPLACE_EDIT_READY &&
                edit != nullptr,
            "replacement output sentinel is a real owned edit");
    replace_edit_append(edit, u"k", 1);
    return edit;
}

static void expect_too_large(const struct note_replace_plan *_Nonnull plan)
{
    struct replace_edit *kept = edit_sentinel();
    struct replace_edit *out = kept;
    allocation_probe_fail_at(1);
    require(note_replace_build(plan, &out) == NOTE_REPLACE_TOO_LARGE && out == kept &&
                allocation_probe_calls() == 0 && replace_edit_length(kept) == 1 &&
                replace_edit_units(kept)[0] == u'k',
            "replacement limit rejects before allocation and preserves caller-owned output");
    allocation_probe_fail_at(0);
    replace_edit_destroy(kept);
}

static void verify_replace_exact_limit(void)
{
    char16_t body[2001];
    char16_t pattern[2001];
    for (size_t index = 0; index < 2000; ++index)
    {
        body[index] = u'a';
        pattern[index] = u'&';
    }
    pattern[2000] = u'x';
    body[2000] = u'a';
    struct regex_match match = {.whole = {0, 2000}, .groups = {}};
    struct regex_matches matches = {.items = &match, .capacity = 1, .count = 1};
    struct replace_template *exact = replacement_for(pattern, 2000);
    struct replace_template *over = replacement_for(pattern, 2001);
    const enum replace_scope scopes[] = {REPLACE_ONE, REPLACE_ALL, REPLACE_LINE_FIRST};
    for (size_t index = 0; index < sizeof scopes / sizeof scopes[0]; ++index)
    {
        struct note_replace_plan plan = {.text = body,
                                         .length = 2000,
                                         .matches = &matches,
                                         .replacement = exact,
                                         .scope = scopes[index],
                                         .anchor = {0, 0}};
        struct replace_edit *edit = nullptr;
        require(note_replace_build(&plan, &edit) == NOTE_REPLACE_READY && edit != nullptr &&
                    replace_edit_length(edit) == note_replace_limit,
                "all three scopes accept exactly four million output units");
        const char16_t *units = replace_edit_units(edit);
        size_t at = 0;
        while (at < note_replace_limit && units[at] == u'a')
        {
            at += 1;
        }
        require(at == note_replace_limit, "both passes produce every measured output unit");
        require(units[note_replace_limit] == u'\0', "limit output has one terminator");
        replace_edit_destroy(edit);
        plan.replacement = over;
        expect_too_large(&plan);
    }
    struct note_replace_plan surrounding = {.text = body,
                                            .length = 2001,
                                            .matches = &matches,
                                            .replacement = exact,
                                            .scope = REPLACE_ALL,
                                            .anchor = {0, 0}};
    match.whole = (struct note_search_span){1, 2001};
    expect_too_large(&surrounding);
    match.whole = (struct note_search_span){0, 2000};
    expect_too_large(&surrounding);
    replace_template_destroy(over);
    replace_template_destroy(exact);
}

/* Huge declared spans are rejected during measuring; their backing text is never read. */
static void verify_replace_declared_bounds(void)
{
    struct regex_match match = {.whole = {0, SIZE_MAX}, .groups = {}};
    struct regex_matches matches = {.items = &match, .capacity = 1, .count = 1};
    struct replace_template *whole = replacement_for(u"&x", 2);
    struct note_replace_plan plan = {.text = u"a",
                                     .length = SIZE_MAX,
                                     .matches = &matches,
                                     .replacement = whole,
                                     .scope = REPLACE_ONE,
                                     .anchor = {0, 0}};
    expect_too_large(&plan);
    match.whole = (struct note_search_span){SIZE_MAX, SIZE_MAX};
    plan.scope = REPLACE_ALL;
    expect_too_large(&plan);
    match.whole = (struct note_search_span){0, 1};
    expect_too_large(&plan);
    plan.scope = REPLACE_LINE_FIRST;
    expect_too_large(&plan);
    replace_template_destroy(whole);
}

static void verify_replace_accumulated_limit(void)
{
    struct regex_match items[2] = {{.whole = {0, 1}, .groups = {}},
                                   {.whole = {1, 2}, .groups = {}}};
    for (size_t index = 0; index < 2; ++index)
    {
        items[index].groups[0].present = true;
        items[index].groups[0].span = (struct note_search_span){0, note_replace_limit / 2 + 1};
    }
    struct regex_matches matches = {.items = items, .capacity = 2, .count = 2};
    struct replace_template *group = replacement_for(u"\\1", 2);
    struct note_replace_plan plan = {.text = u"aa",
                                     .length = note_replace_limit / 2 + 1,
                                     .matches = &matches,
                                     .replacement = group,
                                     .scope = REPLACE_ALL,
                                     .anchor = {0, 0}};
    expect_too_large(&plan);
    replace_template_destroy(group);
    struct replace_template *two_groups = replacement_for(u"\\1\\1", 4);
    plan.replacement = two_groups;
    plan.scope = REPLACE_ONE;
    matches.count = 1;
    expect_too_large(&plan);
    replace_template_destroy(two_groups);
}

void run_size_overflow_tests(void)
{
    bool measured = probe_active();
    printf("size overflow allocation probe: %s; maximum-fit checks: %d\n",
           measured ? "active" : "inactive (canonical)", measured ? 3 : 0);
    verify_utf8_bounds(measured);
    verify_utf16_bounds(measured);
    verify_buffer_overflow();
    verify_buffer_growth(measured);
    if (measured)
    {
        verify_buffer_failed_growth();
    }
    verify_replace_exact_limit();
    verify_replace_declared_bounds();
    verify_replace_accumulated_limit();
}
