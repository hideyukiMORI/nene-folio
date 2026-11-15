#include "allocation_probe.h"
#include "index_filter.h"
#include "index_filter_build.h"
#include "note_excerpt.h"
#include "note_metadata.h"
#include "note_text.h"
#include "unit_tests.h"
#include "utf8_text.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct index_filter_query query(const char *_Nonnull term)
{
    return (struct index_filter_query){
        .term = term, .length = strlen(term), .mode = TAG_MATCH_ALL, .scope = INDEX_MATCH_NAMES};
}

static struct index_filter_entry named(size_t index, const char *_Nonnull name,
                                       const char *_Nullable category, bool favorite)
{
    return (struct index_filter_entry){
        .ref = {0, index}, .name = name, .category_name = category, .favorite = favorite};
}

static void drain(struct index_filter_build *_Nonnull build,
                  const struct index_filter_entry *_Nonnull entries, size_t count, size_t budget)
{
    for (size_t index = 0; index < count; ++index)
    {
        enum index_filter_step step = INDEX_FILTER_STEP_MORE;
        size_t calls = 0;
        while (step == INDEX_FILTER_STEP_MORE)
        {
            size_t zero = 0;
            require(index_filter_build_step(build, &entries[index], &zero) ==
                        INDEX_FILTER_STEP_MORE,
                    "zero work cannot advance a live entry or its final projection");
            size_t work = budget;
            step = index_filter_build_step(build, &entries[index], &work);
            require(work < budget && ++calls < 100000,
                    "positive work progresses and bounded fixture terminates");
        }
        require((index + 1 == count) == (step == INDEX_FILTER_STEP_ALL_DONE),
                "last entry includes projection completion before ALL_DONE");
    }
    require(index_filter_build_readiness(build) == INDEX_FILTER_READY, "complete names only");
}

static struct index_filter *_Nonnull filtered(const struct index_filter_query *_Nonnull request,
                                              const struct index_filter_entry *_Nonnull entries,
                                              size_t count, size_t budget)
{
    struct index_filter_build *build = nullptr;
    require(index_filter_build_create(request, count, &build) == INDEX_FILTER_ACCEPTED,
            "name query accepted including empty query");
    drain(build, entries, count, budget);
    struct index_filter *result = nullptr;
    require(index_filter_finish(&build, &result) == INDEX_FILTER_FINISH_ACCEPTED &&
                build == nullptr,
            "immutable names take the only sealed builder");
    return result;
}

static void expect_order(const struct index_filter *_Nonnull result, const size_t *_Nonnull notes,
                         size_t count)
{
    require(index_filter_ranked_count(result) == count, "ranked count exact");
    for (size_t index = 0; index < count; ++index)
    {
        struct index_ranked_note row = index_filter_ranked(result, index);
        require(row.ref.category == 0 && row.ref.note == notes[index],
                "independent golden ranking preserves ledger tie order");
        require(index_filter_note(result, row.ref) && index_filter_category(result, 0),
                "ranked projection does not reorder binary-search membership");
    }
}

static void rank_budgets(const struct index_filter_entry *_Nonnull entries,
                         const size_t *_Nonnull expected)
{
    const size_t budgets[] = {1, 2, 7, 65536};
    struct index_filter_query request = query("abc");
    for (size_t at = 0; at < sizeof budgets / sizeof budgets[0]; ++at)
    {
        struct index_filter *result = filtered(&request, entries, 10, budgets[at]);
        expect_order(result, expected, 9);
        require(index_filter_ranked(result, 0).kind == INDEX_MATCH_NAME_EXACT &&
                    index_filter_ranked(result, 4).kind == INDEX_MATCH_NAME_SUBSEQUENCE &&
                    index_filter_ranked(result, 8).kind == INDEX_MATCH_CATEGORY_SUBSEQUENCE,
                "kind is the matching side and best tier");
        index_filter_destroy(result);
    }
}

static void rankings(void)
{
    const struct index_filter_entry entries[] = {
        named(0, "axbyc", "none", true),    named(1, "abc--", "none", false),
        named(2, "other", "abc", false),    named(3, "ABC", "none", false),
        named(4, "--abc--", "none", false), named(5, "abc", "none", true),
        named(6, "x", "abcx", false),       named(7, "y", "xabc", false),
        named(8, "z", "axbyc", false),      named(9, "acb", nullptr, false),
    };
    const size_t manual[] = {3, 5, 1, 4, 0, 2, 6, 7, 8};
    const size_t favored[] = {5, 0, 3, 1, 4, 2, 6, 7, 8};
    rank_budgets(entries, manual);
    struct index_filter_query request = query("abc");
    request.favorites_first = true;
    struct index_filter *result = filtered(&request, entries, 10, 3);
    expect_order(result, favored, 9);
    index_filter_destroy(result);
    request.favorites_only = true;
    result = filtered(&request, entries, 10, 3);
    expect_order(result, favored, 2);
    index_filter_destroy(result);
}

static struct note_text *_Nonnull source(const char *_Nonnull bytes, size_t length)
{
    struct note_text *text = nullptr;
    require(note_text_create(bytes, length, &text) == NOTE_TEXT_ACCEPTED, "valid excerpt source");
    return text;
}

static void scopes_and_tags(void)
{
    const char *bytes = "---\ntags: [red]\n---\nbody";
    struct note_text *body = source(bytes, strlen(bytes));
    const struct note_tags *tags = note_metadata_tags(note_text_metadata(body));
    struct index_filter_entry entries[] = {
        named(0, "alpha", "A", false), named(1, "alpha", "B", true), named(2, "alpha", "C", false)};
    entries[1].tags = tags;
    struct index_filter_query request = query("");
    struct index_filter *result = filtered(&request, entries, 3, 1);
    const size_t all[] = {0, 1, 2};
    expect_order(result, all, 3);
    require(index_filter_ranked(result, 0).kind == INDEX_MATCH_ALL,
            "empty names do not require readable bodies");
    index_filter_destroy(result);
    request.tags = tags;
    result = filtered(&request, entries, 3, 1);
    const size_t tagged[] = {1};
    expect_order(result, tagged, 1);
    index_filter_destroy(result);
    request = query("alpha");
    request.scope = INDEX_MATCH_NAME_BODY;
    result = filtered(&request, entries, 3, 2);
    require(index_filter_count(result) == 0 && index_filter_ranked_count(result) == 0,
            "legacy name-body still requires a readable body and has no rank projection");
    index_filter_destroy(result);
    note_text_destroy(body);
}

static uint32_t oracle_fold(uint32_t point)
{
    return point >= 'A' && point <= 'Z' ? point + ('a' - 'A') : point;
}

/* Independent fixed-size scalar oracle: enumerate the three possible ordered pairs.
 * It neither calls a string matcher nor decodes the production UTF-8 source. */
static int oracle_tier(const uint32_t *_Nonnull values, uint32_t first, uint32_t second)
{
    bool a = oracle_fold(values[0]) == oracle_fold(first);
    bool b = oracle_fold(values[1]) == oracle_fold(first);
    bool c = oracle_fold(values[1]) == oracle_fold(second);
    bool d = oracle_fold(values[2]) == oracle_fold(second);
    if (a && c)
        return 0;
    if (b && d)
        return 1;
    if (a && d)
        return 2;
    return -1;
}

static void encode_points(const uint32_t *_Nonnull points, size_t count, char *_Nonnull bytes)
{
    size_t length = 0;
    for (size_t index = 0; index < count; ++index)
        length += utf8_text_encode(points[index], bytes + length);
    bytes[length] = '\0';
}

static void expect_scalar_order(const struct index_filter *_Nonnull result,
                                const uint32_t (*_Nonnull values)[3],
                                const uint32_t *_Nonnull wanted)
{
    size_t expected[216];
    size_t count = 0;
    for (int tier = 0; tier < 3; ++tier)
        for (size_t at = 0; at < 216; ++at)
            if (oracle_tier(values[at], wanted[0], wanted[1]) == tier)
                expected[count++] = at;
    require(index_filter_count(result) == count, "scalar oracle membership exact");
    expect_order(result, expected, count);
}

static void scalar_oracle(void)
{
    const uint32_t alphabet[] = {'A', 'b', 0xe9, 0xc3, 0xa9, 0x1f600};
    uint32_t values[216][3];
    char names[216][16];
    struct index_filter_entry entries[216];
    for (size_t at = 0; at < 216; ++at)
    {
        values[at][0] = alphabet[at / 36];
        values[at][1] = alphabet[at / 6 % 6];
        values[at][2] = alphabet[at % 6];
        encode_points(values[at], 3, names[at]);
        entries[at] = named(at, names[at], nullptr, false);
    }
    for (size_t pair = 0; pair < 36; ++pair)
    {
        uint32_t wanted[] = {alphabet[pair / 6], alphabet[pair % 6]};
        char bytes[9];
        encode_points(wanted, 2, bytes);
        struct index_filter_query request = query(bytes);
        struct index_filter *result = filtered(&request, entries, 216, pair % 7 + 1);
        expect_scalar_order(result, values, wanted);
        index_filter_destroy(result);
    }
}

static void byte_subsequence_counterexamples(void)
{
    const struct index_filter_entry crossed[] = {named(0, "Ã©", nullptr, false),
                                                 named(1, "😁a🌀", nullptr, false)};
    const char *terms[] = {"é", "😀"};
    for (size_t at = 0; at < 2; ++at)
    {
        struct index_filter_query request = query(terms[at]);
        struct index_filter *result = filtered(&request, crossed, 2, 1);
        require(index_filter_count(result) == 0,
                "UTF-8 byte subsequences cannot impersonate scalars");
        index_filter_destroy(result);
    }
}

static void capped_and_removed(void)
{
    struct index_filter_entry entries[205];
    for (size_t index = 0; index < 205; ++index)
        entries[index] = named(index, "same", nullptr, index == 204);
    struct index_filter_query request = query("same");
    request.favorites_first = true;
    struct index_filter *result = filtered(&request, entries, 205, 1);
    require(index_filter_count(result) == 205 && index_filter_ranked_count(result) == 200 &&
                index_filter_ranked(result, 0).ref.note == 204 &&
                index_filter_ranked(result, 199).ref.note == 198,
            "all evaluated and global top 200");
    struct index_filter *removed = nullptr;
    require(index_filter_removed(result, (struct note_ref){0, 0}, &removed) ==
                INDEX_FILTER_ACCEPTED,
            "remove can promote a formerly hidden ranked note");
    require(index_filter_count(removed) == 204 && index_filter_ranked_count(removed) == 200 &&
                index_filter_ranked(removed, 0).ref.note == 203 &&
                index_filter_ranked(removed, 199).ref.note == 198 &&
                index_filter_ranked(result, 0).ref.note == 204,
            "removed preserves rank policy and original; old note 199 fills the vacated rank");
    index_filter_destroy(removed);
    index_filter_destroy(result);
}

static void empty_and_sealed(void)
{
    struct index_filter_query request = query("");
    const struct index_filter_entry entry = named(0, "one", nullptr, false);
    struct index_filter *empty = filtered(&request, &entry, 0, 1);
    require(index_filter_count(empty) == 0 && index_filter_ranked_count(empty) == 0,
            "empty ledger is a finished empty name result");
    index_filter_destroy(empty);
    char *term = malloc(4);
    require(term != nullptr, "owned query fixture");
    if (term == nullptr)
        return;
    memcpy(term, "one", 4);
    request = query(term);
    struct index_filter_build *build = nullptr;
    require(index_filter_build_create(&request, 1, &build) == INDEX_FILTER_ACCEPTED,
            "borrowed query");
    drain(build, &entry, 1, 1);
    free(term);
    size_t zero = 0;
    require(index_filter_build_step(build, &entry, &zero) == INDEX_FILTER_STEP_ALL_DONE,
            "sealed name projection never reuses the destroyed query");
    struct index_filter *result = nullptr;
    require(index_filter_finish(&build, &result) == INDEX_FILTER_FINISH_ACCEPTED,
            "sealed ownership");
    struct index_filter *removed = nullptr;
    require(index_filter_removed(result, entry.ref, &removed) == INDEX_FILTER_ACCEPTED &&
                index_filter_ranked_count(removed) == 0 && index_filter_count(removed) == 0,
            "removing the only ranked note produces empty sealed results");
    index_filter_destroy(removed);
    index_filter_destroy(result);
    request = query("");
    allocation_probe_fail_at(1);
    enum index_filter_outcome oversized = index_filter_build_create(&request, SIZE_MAX, &build);
    size_t calls = allocation_probe_calls();
    allocation_probe_fail_at(0);
    require(oversized == INDEX_FILTER_OUT_OF_MEMORY && build == nullptr && calls == 0,
            "unrepresentable name capacity is refused before allocation");
}

static void excerpt_basics(void)
{
    const char *bytes = "\xef\xbb\xbf\xef\xbb\xbf"
                        "a\r\n\n設計\rZ\n";
    struct note_text *text = source(bytes, strlen(bytes));
    struct note_excerpt *excerpt = nullptr;
    require(note_excerpt_create(text, &excerpt) == NOTE_EXCERPT_READY, "create owned excerpt");
    require(same_text(note_excerpt_bytes(excerpt), bytes + 3) &&
                note_excerpt_line_count(excerpt) == 5,
            "source FEFF and mixed newline bytes are never reinterpreted");
    const size_t offsets[] = {0, 6, 7, 14, 16};
    const size_t lengths[] = {4, 0, 6, 1, 0};
    for (size_t index = 0; index < 5; ++index)
    {
        struct note_excerpt_line line = note_excerpt_line(excerpt, index);
        require(line.offset == offsets[index] && line.length == lengths[index],
                "independent raw byte line spans exclude CRLF LF and CR");
    }
    note_text_destroy(text);
    require(note_excerpt_extent(excerpt) == NOTE_EXCERPT_FULL &&
                note_excerpt_length(excerpt) == strlen(bytes) - 3,
            "excerpt survives its input");
    note_excerpt_destroy(excerpt);
    text = source("", 0);
    require(note_excerpt_create(text, &excerpt) == NOTE_EXCERPT_READY &&
                note_excerpt_line_count(excerpt) == 1 && note_excerpt_line(excerpt, 0).length == 0,
            "empty source remains a readable empty line");
    note_excerpt_destroy(excerpt);
    note_text_destroy(text);
}

static void excerpt_limits(void)
{
    char bytes[16400];
    memset(bytes, 'x', sizeof bytes);
    memcpy(bytes + 16382, "😀", 4);
    struct note_text *text = source(bytes, 16386);
    struct note_excerpt *excerpt = nullptr;
    require(note_excerpt_create(text, &excerpt) == NOTE_EXCERPT_READY &&
                note_excerpt_length(excerpt) == 16382 &&
                note_excerpt_extent(excerpt) == NOTE_EXCERPT_BYTE_LIMIT,
            "byte limit stops before a four-byte scalar");
    note_excerpt_destroy(excerpt);
    note_text_destroy(text);
    bytes[16383] = '\r';
    bytes[16384] = '\n';
    memset(bytes, 'x', 16383);
    text = source(bytes, 16385);
    require(note_excerpt_create(text, &excerpt) == NOTE_EXCERPT_READY &&
                note_excerpt_length(excerpt) == 16383,
            "byte limit cannot split CRLF");
    note_excerpt_destroy(excerpt);
    note_text_destroy(text);
    memset(bytes, '\n', 80);
    text = source(bytes, 80);
    require(note_excerpt_create(text, &excerpt) == NOTE_EXCERPT_READY &&
                note_excerpt_line_count(excerpt) == 80 && note_excerpt_length(excerpt) == 79 &&
                note_excerpt_extent(excerpt) == NOTE_EXCERPT_LINE_LIMIT,
            "exactly 80 logical lines");
    note_excerpt_destroy(excerpt);
    note_text_destroy(text);
    memset(bytes, 'x', 16384);
    text = source(bytes, 16384);
    require(note_excerpt_create(text, &excerpt) == NOTE_EXCERPT_READY &&
                note_excerpt_length(excerpt) == 16384 &&
                note_excerpt_extent(excerpt) == NOTE_EXCERPT_FULL,
            "an exact byte-limit document is not reported truncated");
    note_excerpt_destroy(excerpt);
    note_text_destroy(text);
}

static void removed_allocation_boundaries(struct index_filter *_Nonnull original)
{
    size_t failures = 0;
    for (size_t nth = 1; nth < 16; ++nth)
    {
        struct index_filter *result = original;
        allocation_probe_fail_at(nth);
        enum index_filter_outcome made =
            index_filter_removed(original, (struct note_ref){0, 99}, &result);
        allocation_probe_fail_at(0);
        if (made == INDEX_FILTER_ACCEPTED)
        {
            require(index_filter_ranked_count(result) == 1, "removed success retains names");
            index_filter_destroy(result);
            break;
        }
        require(made == INDEX_FILTER_OUT_OF_MEMORY && result == original &&
                    index_filter_ranked_count(original) == 1,
                "removed allocation failures preserve both the original and output");
        failures += 1;
    }
    require(failures == 0 || failures == 5,
            "all five removed construction/wrapper boundaries covered");
    printf("discovery: %zu removed allocation failures\n", failures);
}

static void allocation_boundaries(void)
{
    struct index_filter_query request = query("設計");
    const struct index_filter_entry entry = named(0, "設 計", "work", true);
    struct index_filter_build *sentinel = nullptr;
    require(index_filter_build_create(&request, 1, &sentinel) == INDEX_FILTER_ACCEPTED,
            "sentinel build");
    size_t failures = 0;
    for (size_t nth = 1; nth < 16; ++nth)
    {
        struct index_filter_build *result = sentinel;
        allocation_probe_fail_at(nth);
        enum index_filter_outcome made = index_filter_build_create(&request, 1, &result);
        allocation_probe_fail_at(0);
        if (made == INDEX_FILTER_ACCEPTED)
        {
            index_filter_build_destroy(result);
            break;
        }
        require(made == INDEX_FILTER_OUT_OF_MEMORY && result == sentinel,
                "create failure preserves out");
        failures += 1;
    }
    drain(sentinel, &entry, 1, 1);
    struct index_filter *finished = nullptr;
    allocation_probe_fail_at(1);
    enum index_filter_finish_outcome finish = index_filter_finish(&sentinel, &finished);
    allocation_probe_fail_at(0);
    if (finish == INDEX_FILTER_FINISH_OUT_OF_MEMORY)
    {
        require(sentinel != nullptr && finished == nullptr, "finish OOM keeps ready owner");
        require(index_filter_finish(&sentinel, &finished) == INDEX_FILTER_FINISH_ACCEPTED,
                "ready finish retries without entry or query access");
    }
    require((failures == 0 || failures == 5) && finished != nullptr,
            "all five name construction boundaries covered");
    removed_allocation_boundaries(finished);
    index_filter_destroy(finished);
    index_filter_build_destroy(sentinel);
    printf("discovery: %zu builder allocation failures\n", failures);
}

static void excerpt_allocation_boundaries(void)
{
    struct note_text *text = source("source", 6);
    struct note_excerpt *old = nullptr;
    require(note_excerpt_create(text, &old) == NOTE_EXCERPT_READY, "sentinel excerpt");
    size_t excerpt_failures = 0;
    for (size_t nth = 1; nth < 4; ++nth)
    {
        struct note_excerpt *result = old;
        allocation_probe_fail_at(nth);
        enum note_excerpt_outcome made = note_excerpt_create(text, &result);
        allocation_probe_fail_at(0);
        if (made == NOTE_EXCERPT_READY)
        {
            note_excerpt_destroy(result);
            break;
        }
        require(result == old, "excerpt allocation failure preserves out");
        excerpt_failures += 1;
    }
    require(excerpt_failures == 0 || excerpt_failures == 2,
            "both excerpt allocation boundaries covered");
    printf("discovery: %zu excerpt allocation failures\n", excerpt_failures);
    note_excerpt_destroy(old);
    note_text_destroy(text);
}

void run_discovery_tests(void)
{
    rankings();
    scopes_and_tags();
    scalar_oracle();
    byte_subsequence_counterexamples();
    capped_and_removed();
    empty_and_sealed();
    excerpt_basics();
    excerpt_limits();
    allocation_boundaries();
    excerpt_allocation_boundaries();
}
