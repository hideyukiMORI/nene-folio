/* #311 宣言長の算術境界。最大fitは測定buildの確保拒否でだけ観測し、巨大実確保をしない。 */
#include "allocation_probe.h"
#include "index_filter.h"
#include "index_filter_build.h"
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

static void verify_edit_capacity_bounds(bool measured)
{
    struct replace_edit *kept = edit_sentinel();
    const size_t refused[] = {SIZE_MAX / sizeof(char16_t), SIZE_MAX};
    for (size_t index = 0; index < sizeof refused / sizeof refused[0]; ++index)
    {
        struct replace_edit *out = kept;
        allocation_probe_fail_at(1);
        require(replace_edit_create((struct note_search_span){0, 0}, refused[index], &out) ==
                        REPLACE_EDIT_OUT_OF_MEMORY &&
                    out == kept && allocation_probe_calls() == 0,
                "edit capacity overflow refuses before owner allocation and preserves out");
        allocation_probe_fail_at(0);
    }
    if (measured)
    {
        struct replace_edit *out = kept;
        allocation_probe_fail_at(1);
        require(replace_edit_create((struct note_search_span){0, 0},
                                    SIZE_MAX / sizeof(char16_t) - 1,
                                    &out) == REPLACE_EDIT_OUT_OF_MEMORY &&
                    out == kept && allocation_probe_calls() == 1,
                "maximum fitting edit capacity reaches only the refused small owner allocation");
        allocation_probe_fail_at(0);
    }
    require(replace_edit_length(kept) == 1 &&
                memcmp(replace_edit_units(kept), u"k", 2 * sizeof(char16_t)) == 0 &&
                replace_edit_span(kept).end == 1,
            "capacity refusal preserves the existing edit text, terminator and span");
    replace_edit_destroy(kept);
    struct replace_edit *empty = nullptr;
    require(replace_edit_create((struct note_search_span){2, 3}, 0, &empty) == REPLACE_EDIT_READY,
            "zero capacity still owns a terminated empty edit");
    replace_edit_append(empty, u"x", 1);
    require(replace_edit_length(empty) == 0 && replace_edit_units(empty)[0] == u'\0' &&
                replace_edit_span(empty).start == 2 && replace_edit_span(empty).end == 3,
            "zero capacity preserves its span and terminator when an append has no room");
    replace_edit_destroy(empty);
}

static void verify_template_capacity_bounds(void)
{
    struct replace_template *kept = replacement_for(u"kept", 4);
    /* Each private piece contains size_t; no copy of its layout or exact size is needed. */
    const size_t refused[] = {SIZE_MAX / sizeof(char16_t), SIZE_MAX / sizeof(size_t), SIZE_MAX};
    for (size_t index = 0; index < sizeof refused / sizeof refused[0]; ++index)
    {
        struct replace_template *out = kept;
        allocation_probe_fail_at(1);
        require(replace_template_create(u"x", refused[index], &out) ==
                        REPLACE_TEMPLATE_OUT_OF_MEMORY &&
                    out == kept && allocation_probe_calls() == 0,
                "literal or piece capacity overflow refuses before owner allocation or input read");
        allocation_probe_fail_at(0);
    }
    require(replace_template_count(kept) == 1 && replace_template_literal_length(kept, 0) == 4 &&
                memcmp(replace_template_literal(kept, 0), u"kept", 4 * sizeof(char16_t)) == 0,
            "template capacity refusal preserves every existing literal");
    replace_template_destroy(kept);
}

void run_replace_factory_size_tests(void)
{
    bool measured = probe_active();
    printf("replacement capacity probe: %s; edit maximum-fit checks: %d\n",
           measured ? "active" : "inactive (canonical)", measured ? 1 : 0);
    verify_edit_capacity_bounds(measured);
    verify_template_capacity_bounds();
    run_replace_preview_size_tests();
}

static struct index_filter_build *_Nonnull filter_sentinel(void)
{
    const struct index_filter_entry entry = {
        .ref = {.category = 3, .note = 7}, .name = "_", .body = "a", .length = 1};
    struct index_filter_build *filter = nullptr;
    size_t work = 65536;
    require(index_filter_build_create("a", 1, 1, &filter) == INDEX_FILTER_ACCEPTED &&
                index_filter_build_step(filter, &entry, &work) == INDEX_FILTER_STEP_ALL_DONE,
            "filter sentinel is a real owned nonempty set");
    return filter;
}

static void verify_filter_capacity_bounds(bool measured)
{
    struct index_filter_build *kept = filter_sentinel();
    const size_t refused[] = {SIZE_MAX / sizeof(size_t), SIZE_MAX};
    for (size_t index = 0; index < sizeof refused / sizeof refused[0]; ++index)
    {
        struct index_filter_build *out = kept;
        allocation_probe_fail_at(1);
        enum index_filter_outcome outcome = index_filter_build_create("a", 1, refused[index], &out);
        size_t calls = allocation_probe_calls();
        size_t array_bytes = allocation_probe_malloc_size();
        allocation_probe_fail_at(0);
        printf("filter capacity: count=%zu, result=%u, expected_calls=0, actual_calls=%zu, "
               "malloc_bytes=%zu\n",
               refused[index], (unsigned)outcome, calls, array_bytes);
        require(outcome == INDEX_FILTER_OUT_OF_MEMORY && out == kept && calls == 0 &&
                    array_bytes == 0,
                "filter capacity overflow refuses before owner allocation or entry access");
    }
    if (measured)
    {
        size_t count = SIZE_MAX / sizeof(size_t) - 1;
        struct index_filter_build *out = kept;
        allocation_probe_fail_at(1);
        enum index_filter_outcome outcome = index_filter_build_create("a", 1, count, &out);
        size_t calls = allocation_probe_calls();
        size_t array_bytes = allocation_probe_malloc_size();
        allocation_probe_fail_at(0);
        printf(
            "filter maximum fit: count=%zu, expected_calls=1, actual_calls=%zu, malloc_bytes=%zu\n",
            count, calls, array_bytes);
        require(outcome == INDEX_FILTER_OUT_OF_MEMORY && out == kept && calls == 1 &&
                    array_bytes == 0,
                "maximum fitting filter count reaches only the deliberately refused small owner");
    }
    require(index_filter_build_count(kept) == 1 && index_filter_build_ref(kept, 0).category == 3 &&
                index_filter_build_ref(kept, 0).note == 7,
            "capacity refusal preserves the actual nonempty filter and every queried ref");
    index_filter_build_destroy(kept);
}

static void verify_filter_result_priority(void)
{
    struct index_filter_build *kept = filter_sentinel();
    struct index_filter_build *out = kept;
    allocation_probe_fail_at(1);
    enum index_filter_outcome empty = index_filter_build_create("", 0, SIZE_MAX, &out);
    size_t empty_calls = allocation_probe_calls();
    allocation_probe_fail_at(0);
    require(empty == INDEX_FILTER_NO_TERM && out == kept && empty_calls == 0,
            "NO_TERM still precedes impossible count without allocation");
    allocation_probe_fail_at(1);
    enum index_filter_outcome malformed = index_filter_build_create("\xC0\xAF", 2, SIZE_MAX, &out);
    size_t malformed_calls = allocation_probe_calls();
    allocation_probe_fail_at(0);
    require(malformed == INDEX_FILTER_MALFORMED && out == kept && malformed_calls == 0 &&
                index_filter_build_count(kept) == 1,
            "MALFORMED still precedes impossible count and preserves the owned result");
    index_filter_build_destroy(kept);
}

void run_index_filter_size_tests(void)
{
    bool measured = probe_active();
    printf("filter size allocation probe: %s; maximum-fit checks: %d\n",
           measured ? "active" : "inactive (canonical)", measured ? 1 : 0);
    verify_filter_capacity_bounds(measured);
    verify_filter_result_priority();
}

void run_index_filter_size_allocation_tests(void)
{
    require(probe_active(), "filter size allocation selector requires active failure injection");
    run_index_filter_size_tests();
    run_filter_allocation_tests();
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
    run_replace_factory_size_tests();
    run_index_filter_size_tests();
}
