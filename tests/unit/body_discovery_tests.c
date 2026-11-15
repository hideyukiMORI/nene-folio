#include "allocation_probe.h"
#include "discovery_target.h"
#include "index_filter.h"
#include "index_filter_build.h"
#include "note_excerpt.h"
#include "note_identity.h"
#include "note_metadata.h"
#include "note_source.h"
#include "note_text.h"
#include "unit_tests.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct index_filter_query body_query(const char *_Nonnull term)
{
    return (struct index_filter_query){
        .term = term, .length = strlen(term), .scope = INDEX_MATCH_BODY, .mode = TAG_MATCH_ALL};
}

static struct index_filter_entry body_entry(size_t at, const char *_Nullable bytes)
{
    return (struct index_filter_entry){.ref = {0, at},
                                       .name = "name-only-needle",
                                       .category_name = "needle",
                                       .body = bytes,
                                       .length = bytes == nullptr ? 0 : strlen(bytes)};
}

static struct note_text *_Nonnull text_of(const char *_Nonnull bytes)
{
    struct note_text *text = nullptr;
    require(note_text_create(bytes, strlen(bytes), &text) == NOTE_TEXT_ACCEPTED,
            "valid body fixture");
    return text;
}

static struct index_filter *_Nonnull build_body(const struct index_filter_query *_Nonnull query,
                                                const struct index_filter_entry *_Nonnull entries,
                                                size_t count, size_t budget)
{
    struct index_filter_build *build = nullptr;
    require(index_filter_build_create(query, count, &build) == INDEX_FILTER_ACCEPTED,
            "body scope accepted");
    for (size_t at = 0; at < count; ++at)
    {
        enum index_filter_step step = INDEX_FILTER_STEP_MORE;
        size_t calls = 0;
        while (step == INDEX_FILTER_STEP_MORE)
        {
            size_t zero = 0;
            require(index_filter_build_step(build, &entries[at], &zero) == INDEX_FILTER_STEP_MORE,
                    "zero work cannot skip body matching or coordinate measurement");
            size_t work = budget;
            step = index_filter_build_step(build, &entries[at], &work);
            require(work < budget && ++calls < 200000, "bounded body fixture makes progress");
        }
        require((step == INDEX_FILTER_STEP_ALL_DONE) == (at + 1 == count),
                "body coordinates and final projection complete before ALL_DONE");
    }
    struct index_filter *result = nullptr;
    require(index_filter_finish(&build, &result) == INDEX_FILTER_FINISH_ACCEPTED &&
                build == nullptr,
            "body result takes the sole sealed owner");
    return result;
}

static struct note_source_match match_of(const char *_Nonnull source, const char *_Nonnull term,
                                         size_t budget)
{
    struct index_filter_query query = body_query(term);
    struct index_filter_entry entry = body_entry(0, source);
    struct index_filter *result = build_body(&query, &entry, 1, budget);
    require(index_filter_count(result) == 1 && index_filter_ranked_count(result) == 1,
            "one body match expected");
    struct index_ranked_note row = index_filter_ranked(result, 0);
    require(row.kind == INDEX_MATCH_BODY_FIRST, "body result has its own first-occurrence meaning");
    index_filter_destroy(result);
    return row.match;
}

static void fixed_positions(void)
{
    const char *bytes = "head\r\n日本語😀\rX\nabAB終";
    const size_t budgets[] = {1, 2, 7, 65536};
    for (size_t at = 0; at < sizeof budgets / sizeof budgets[0]; ++at)
    {
        struct note_source_match match = match_of(bytes, "AB", budgets[at]);
        require(match.bytes.start == 22 && match.bytes.end == 24 && match.native.start == 13 &&
                    match.native.end == 15 && match.line == 4 && match.line_start == 22 &&
                    match.context_start == 0 && match.context_line == 1,
                "golden byte, UTF16 and mixed-newline positions select only the first occurrence");
        struct note_text *text = text_of(bytes);
        struct note_excerpt *excerpt = nullptr;
        require(note_excerpt_create_match(text, &match, &excerpt) == NOTE_EXCERPT_READY,
                "matched source window created");
        struct note_search_span selected = note_excerpt_selection(excerpt);
        require(selected.start == 16 && selected.end == 18 &&
                    note_excerpt_first_line(excerpt) == 1 && note_excerpt_line_count(excerpt) == 4,
                "preview CRLF positions differ deliberately from RichEdit paragraph positions");
        char snippet[note_excerpt_snippet_capacity];
        struct note_snippet_clip clip = note_excerpt_snippet(text, &match, snippet);
        require(!clip.leading && !clip.trailing && same_text(snippet, "abAB終"),
                "single-line snippet excludes preceding paragraphs");
        note_text_destroy(text);
        require(same_text(note_excerpt_bytes(excerpt), bytes), "excerpt owns unchanged raw bytes");
        note_excerpt_destroy(excerpt);
    }
    require(note_source_units(bytes, strlen(bytes), NOTE_SOURCE_PARAGRAPH) == 18 &&
                note_source_units(bytes, strlen(bytes), NOTE_SOURCE_CRLF) == 21,
            "shared source unit projections include supplementary scalars");
}

/* Independent naive byte oracle, with no matcher calls or production UTF-8 decoder. */
static unsigned char oracle_fold(unsigned char byte)
{
    return byte >= 'A' && byte <= 'Z' ? (unsigned char)(byte + ('a' - 'A')) : byte;
}

static size_t oracle_first(const char *_Nonnull source, const char *_Nonnull query)
{
    size_t size = strlen(source), wanted = strlen(query);
    for (size_t at = 0; at + wanted <= size; ++at)
    {
        size_t same = 0;
        while (same < wanted && oracle_fold((unsigned char)source[at + same]) ==
                                    oracle_fold((unsigned char)query[same]))
            ++same;
        if (same == wanted)
            return at;
    }
    return size;
}

static size_t oracle_units(const char *_Nonnull source, size_t end)
{
    size_t units = 0;
    for (size_t at = 0; at < end; ++at)
    {
        unsigned char byte = (unsigned char)source[at];
        bool continuation = (byte & 0xc0) == 0x80;
        bool paired_lf = byte == '\n' && at > 0 && source[at - 1] == '\r';
        if (!continuation && !paired_lf)
            units += byte >= 0xf0 ? 2 : 1;
    }
    return units;
}

static void oracle_position(const char *_Nonnull source,
                            const struct note_source_match *_Nonnull match)
{
    size_t line = 1, start = 0;
    for (size_t at = 0; at < match->bytes.start; ++at)
    {
        if (source[at] == '\r' || (source[at] == '\n' && (at == 0 || source[at - 1] != '\r')))
            ++line;
        if (source[at] == '\r' || source[at] == '\n')
            start = at + 1;
    }
    require(match->native.start == oracle_units(source, match->bytes.start) &&
                match->native.end == oracle_units(source, match->bytes.end) &&
                match->line == line && match->line_start == start && match->context_line == 1 &&
                match->context_start == 0,
            "independent byte accounting agrees for every coordinate");
}

static void oracle_case(const char *_Nonnull bytes, const char *_Nonnull term, size_t budget)
{
    struct index_filter_query query = body_query(term);
    struct index_filter_entry entry = body_entry(0, bytes);
    struct index_filter *result = build_body(&query, &entry, 1, budget);
    size_t first = oracle_first(bytes, term);
    bool found = first < strlen(bytes);
    require(index_filter_count(result) == (found ? 1U : 0U), "independent first-match oracle");
    if (found)
    {
        struct note_source_match match = index_filter_ranked(result, 0).match;
        require(match.bytes.start == first && match.bytes.end == first + strlen(term),
                "first original byte range exact");
        oracle_position(bytes, &match);
    }
    index_filter_destroy(result);
}

static void exhaustive_small_sources(void)
{
    const char *atoms[] = {"a", "B", "日", "😀", "\r", "\n", "\r\n"};
    const char *terms[] = {"a", "b", "日", "😀", "ab", "日😀"};
    size_t cases = 0;
    for (size_t a = 0; a < 7; ++a)
        for (size_t b = 0; b < 7; ++b)
            for (size_t c = 0; c < 7; ++c)
            {
                char bytes[40];
                (void)snprintf(bytes, sizeof bytes, "%s%s%s", atoms[a], atoms[b], atoms[c]);
                for (size_t q = 0; q < 6; ++q)
                {
                    oracle_case(bytes, terms[q], 1 + cases % 9);
                    ++cases;
                }
            }
    printf("body discovery: %zu independent byte/scalar oracle cases\n", cases);
}

static void scope_order_and_removal(void)
{
    struct index_filter_entry entries[205];
    for (size_t at = 0; at < 205; ++at)
    {
        entries[at] = body_entry(at, "before\nneedle\na later needle");
        entries[at].favorite = at == 204;
    }
    struct index_filter_query query = body_query("needle");
    query.favorites_first = true;
    struct index_filter *result = build_body(&query, entries, 205, 7);
    require(index_filter_count(result) == 205 && index_filter_ranked_count(result) == 200 &&
                index_filter_ranked(result, 0).ref.note == 204 &&
                index_filter_ranked(result, 199).ref.note == 198,
            "body ordering has only favorite group and stable ledger order with truthful total");
    struct index_filter *removed = nullptr;
    require(index_filter_removed(result, (struct note_ref){0, 0}, &removed) ==
                INDEX_FILTER_ACCEPTED,
            "remove body result through canonical projection rebuild");
    struct index_ranked_note last = index_filter_ranked(removed, 199);
    require(index_filter_count(removed) == 204 && last.ref.note == 198 && last.match.line == 2 &&
                last.match.bytes.start == 7 && last.match.native.start == 7 &&
                index_filter_ranked(removed, 0).ref.note == 203,
            "beyond-200 candidate is promoted with exact saved coordinates after renumbering");
    index_filter_destroy(removed);
    index_filter_destroy(result);
    query.favorites_only = true;
    result = build_body(&query, entries, 205, 5);
    require(index_filter_count(result) == 1 && index_filter_ranked(result, 0).ref.note == 204,
            "body honors favorites-only independently of priority");
    index_filter_destroy(result);
    entries[0] = body_entry(0, nullptr);
    entries[1] = body_entry(1, "different body");
    query = body_query("needle");
    result = build_body(&query, entries, 2, 1);
    require(index_filter_count(result) == 0, "body scope never falls back to name or category");
    index_filter_destroy(result);
}

static void body_query_boundaries(void)
{
    struct index_filter_query query = body_query("");
    struct index_filter_build *build = nullptr;
    require(index_filter_build_create(&query, 0, &build) == INDEX_FILTER_NO_TERM &&
                build == nullptr,
            "empty body has no all-notes interpretation");
    struct note_text *text = text_of("---\ntags: [red]\n---\nbody");
    query.tags = note_metadata_tags(note_text_metadata(text));
    require(index_filter_build_create(&query, 1, &build) == INDEX_FILTER_NO_TERM,
            "tag filters cannot turn an empty body query into a match");
    const char *invalid[] = {"a\nb", "a\rb", "\xff"};
    for (size_t at = 0; at < 3; ++at)
    {
        query = body_query(invalid[at]);
        require(index_filter_build_create(&query, 1, &build) == INDEX_FILTER_MALFORMED,
                "body rejects newlines and malformed UTF8 before constructing a builder");
    }
    query = body_query("a");
    query.length = 2;
    require(index_filter_build_create(&query, 1, &build) == INDEX_FILTER_MALFORMED,
            "explicit NUL inside a body query is invalid");
    query = body_query("body");
    query.tags = note_metadata_tags(note_text_metadata(text));
    struct index_filter_entry entries[] = {body_entry(0, note_text_bytes(text)),
                                           body_entry(1, "body")};
    entries[0].tags = query.tags;
    struct index_filter *result = build_body(&query, entries, 2, 1);
    require(index_filter_count(result) == 1 && index_filter_ranked(result, 0).ref.note == 0,
            "body and tags intersect through the same matcher");
    index_filter_destroy(result);
    note_text_destroy(text);
}

static void context_window(void)
{
    char bytes[1500];
    size_t used = 0;
    for (size_t line = 0; line < 140; ++line)
    {
        const char *part = line == 42 ? "needle\r\n" : "行😀\r\n";
        memcpy(bytes + used, part, strlen(part));
        used += strlen(part);
    }
    bytes[used] = '\0';
    struct note_source_match match = match_of(bytes, "needle", 1);
    require(match.line == 43 && match.context_line == 23 && match.context_start == 198 &&
                match.bytes.start == 378 && match.native.start == 168,
            "21-slot context ring retains the exact twentieth preceding line");
    struct note_text *text = text_of(bytes);
    struct note_excerpt *excerpt = nullptr;
    require(note_excerpt_create_match(text, &match, &excerpt) == NOTE_EXCERPT_READY &&
                note_excerpt_first_line(excerpt) == 23 && note_excerpt_line_count(excerpt) == 80 &&
                note_excerpt_extent(excerpt) == NOTE_EXCERPT_LINE_LIMIT &&
                !note_excerpt_leading_partial(excerpt) && !note_excerpt_trailing_partial(excerpt),
            "matched context is bounded by logical rows without dropping the match");
    struct note_search_span selection = note_excerpt_selection(excerpt);
    require(selection.start == 100 && selection.end == 106,
            "preview context position accounts for CRLF");
    note_excerpt_destroy(excerpt);
    note_text_destroy(text);
}

static void long_line_window(void)
{
    char *bytes = malloc(60001);
    require(bytes != nullptr, "long line fixture allocation");
    for (size_t at = 0; at < 60000; at += 3)
        memcpy(bytes + at, "日", 3);
    memcpy(bytes + 21000, "hit", 3);
    bytes[60000] = '\0';
    struct note_source_match match = match_of(bytes, "hit", 7);
    struct note_text *text = text_of(bytes);
    struct note_excerpt *excerpt = nullptr;
    require(note_excerpt_create_match(text, &match, &excerpt) == NOTE_EXCERPT_READY &&
                note_excerpt_first_line(excerpt) == 1 && note_excerpt_length(excerpt) == 16383 &&
                note_excerpt_leading_partial(excerpt) && note_excerpt_trailing_partial(excerpt),
            "long line shifts to scalar boundary near the match and marks both clipped edges");
    struct note_search_span selection = note_excerpt_selection(excerpt);
    require(selection.start == 85 && selection.end == 88 &&
                memcmp(note_excerpt_bytes(excerpt) + 255, "hit", 3) == 0,
            "scalar-aligned window contains the entire match with exact native selection");
    char snippet[note_excerpt_snippet_capacity];
    struct note_snippet_clip clip = note_excerpt_snippet(text, &match, snippet);
    require(clip.leading && clip.trailing && strlen(snippet) <= 192 &&
                memcmp(snippet + 48, "hit", 3) == 0,
            "row snippet remains bounded and contains the start of the match");
    note_excerpt_destroy(excerpt);
    note_text_destroy(text);
    free(bytes);
}

static void invalid_windows(void)
{
    struct note_text *text = text_of("a\r\n日本語");
    struct note_source_match match = match_of(note_text_bytes(text), "日本", 1);
    struct note_excerpt *sentinel = nullptr;
    require(note_excerpt_create(text, &sentinel) == NOTE_EXCERPT_READY, "sentinel window");
    struct note_source_match invalid[] = {match, match, match, match, match, match, match};
    invalid[0].bytes.start += 1;
    invalid[1].bytes.end = 100;
    invalid[2].context_start = 2;
    invalid[3].context_line = 0;
    invalid[4].line_start = 100;
    invalid[5].bytes.start = 1;
    invalid[6].bytes.end = invalid[6].bytes.start;
    for (size_t at = 0; at < 7; ++at)
    {
        struct note_excerpt *out = sentinel;
        require(note_excerpt_create_match(text, &invalid[at], &out) == NOTE_EXCERPT_INVALID_RANGE &&
                    out == sentinel,
                "malformed byte/scalar/CRLF coordinates preserve the existing output");
    }
    note_excerpt_destroy(sentinel);
    note_text_destroy(text);
}

static void target_boundaries(void)
{
    struct note_text *text = text_of("a\r\n日😀 hit");
    struct note_source_match match = match_of(note_text_bytes(text), "hit", 1);
    struct note_identity *identity = nullptr;
    require(note_identity_create("category", "note.md", &identity) == NOTE_IDENTITY_READY,
            "identity");
    struct discovery_target *target = nullptr;
    require(discovery_target_create(identity, text, &match, &target) == DISCOVERY_TARGET_READY,
            "completed match captured as owned intent");
    note_identity_destroy(identity);
    note_text_destroy(text);
    require(same_text(note_identity_note(discovery_target_identity(target)), "note.md") &&
                same_text(note_text_bytes(discovery_target_body(target)), "a\r\n日😀 hit") &&
                discovery_target_selection(target).start == 6,
            "target survives old identity/corpus lifetime without reparsing a stem");
    text = text_of("a\a hit");
    match = match_of(note_text_bytes(text), "hit", 2);
    struct discovery_target *out = target;
    require(discovery_target_create(discovery_target_identity(target), text, &match, &out) ==
                    DISCOVERY_TARGET_UNEDITABLE &&
                out == target,
            "editable native mapping is refused before any departure mutation");
    match.bytes.end = 100;
    require(discovery_target_create(discovery_target_identity(target), text, &match, &out) ==
                    DISCOVERY_TARGET_INVALID_RANGE &&
                out == target,
            "invalid owned-target coordinates preserve out");
    note_text_destroy(text);
    discovery_target_destroy(target);
    discovery_target_destroy(nullptr);
}

static void snippet_projection(void)
{
    const char *bytes = "\xef\xbb\xbf\xef\xbb\xbf\t\x07hit\xe2\x80\xa8"
                        "end\xe2\x80\xa9";
    struct note_text *text = text_of(bytes);
    struct note_source_match match = match_of(note_text_bytes(text), "hit", 1);
    char snippet[note_excerpt_snippet_capacity];
    struct note_snippet_clip clip = note_excerpt_snippet(text, &match, snippet);
    require(strcmp(snippet, "\xef\xbb\xbf  hit end ") == 0 && !clip.leading && !clip.trailing &&
                strcmp(note_text_bytes(text), bytes + 3) == 0,
            "snippet sanitizes native row separators and controls without rewriting saved source "
            "or BOM");
    note_text_destroy(text);
}

static void context_byte_fallback(void)
{
    char *bytes = malloc(18020);
    require(bytes != nullptr, "large preceding-line fixture");
    memset(bytes, 'a', 18000);
    memcpy(bytes + 18000, "\r\nneedle\r\nend", 14);
    struct note_source_match match = match_of(bytes, "needle", 65536);
    struct note_text *text = text_of(bytes);
    struct note_excerpt *excerpt = nullptr;
    require(note_excerpt_create_match(text, &match, &excerpt) == NOTE_EXCERPT_READY &&
                note_excerpt_first_line(excerpt) == 2 && !note_excerpt_leading_partial(excerpt) &&
                !note_excerpt_trailing_partial(excerpt) &&
                strcmp(note_excerpt_bytes(excerpt), "needle\r\nend") == 0 &&
                note_excerpt_selection(excerpt).start == 0,
            "oversized preceding context is dropped at the matching line boundary before partial "
            "clipping");
    note_excerpt_destroy(excerpt);
    note_text_destroy(text);
    free(bytes);
}

static void maximum_query_window(void)
{
    char query[1025];
    for (size_t at = 0; at < 1024; at += 4)
        memcpy(query + at, "\xf0\x9f\x98\x80", 4);
    query[1024] = '\0';
    char *bytes = malloc(25026);
    require(bytes != nullptr, "maximum query long-line fixture");
    memset(bytes, 'x', 24000);
    memcpy(bytes + 24000, query, 1024);
    memcpy(bytes + 25024, "z", 2);
    struct note_source_match match = match_of(bytes, query, 65536);
    struct note_text *text = text_of(bytes);
    struct note_excerpt *excerpt = nullptr;
    require(note_excerpt_create_match(text, &match, &excerpt) == NOTE_EXCERPT_READY &&
                note_excerpt_leading_partial(excerpt) &&
                note_excerpt_selection(excerpt).start == 256 &&
                note_excerpt_selection(excerpt).end == 768 &&
                memcmp(note_excerpt_bytes(excerpt) + 256, query, 1024) == 0,
            "all 256 supplementary scalars remain inside a bounded shifted preview");
    struct note_identity *identity = nullptr;
    struct discovery_target *target = nullptr;
    require(note_identity_create("category", "note", &identity) == NOTE_IDENTITY_READY &&
                discovery_target_create(identity, text, &match, &target) ==
                    DISCOVERY_TARGET_READY &&
                discovery_target_selection(target).start == 24000 &&
                discovery_target_selection(target).end == 24512,
            "maximum query native positions stay independent of cropped preview positions");
    discovery_target_destroy(target);
    note_identity_destroy(identity);
    note_excerpt_destroy(excerpt);
    note_text_destroy(text);
    free(bytes);
}

static void new_allocation_boundaries(void)
{
    struct note_text *text = text_of("prefix\r\nhit 日");
    struct note_source_match match = match_of(note_text_bytes(text), "hit", 2);
    struct note_identity *identity = nullptr;
    require(note_identity_create("category", "note", &identity) == NOTE_IDENTITY_READY, "identity");
    size_t target_failures = 0, allocations = 0;
    for (size_t nth = 1; nth < 32; ++nth)
    {
        struct discovery_target *target = nullptr;
        allocation_probe_fail_at(nth);
        enum discovery_target_outcome made =
            discovery_target_create(identity, text, &match, &target);
        allocations = allocation_probe_calls();
        allocation_probe_fail_at(0);
        if (made == DISCOVERY_TARGET_READY)
        {
            discovery_target_destroy(target);
            break;
        }
        require(made == DISCOVERY_TARGET_OUT_OF_MEMORY && target == nullptr,
                "every target partial allocation is released and out remains untouched");
        ++target_failures;
    }
    require(target_failures == allocations,
            "every allocation in successful target trace was rejected once");
    size_t excerpt_failures = 0;
    for (size_t nth = 1; nth < 4; ++nth)
    {
        struct note_excerpt *excerpt = nullptr;
        allocation_probe_fail_at(nth);
        enum note_excerpt_outcome made = note_excerpt_create_match(text, &match, &excerpt);
        allocation_probe_fail_at(0);
        if (made == NOTE_EXCERPT_READY)
        {
            note_excerpt_destroy(excerpt);
            break;
        }
        require(made == NOTE_EXCERPT_OUT_OF_MEMORY && excerpt == nullptr,
                "match excerpt OOM preserves out");
        ++excerpt_failures;
    }
    require(excerpt_failures == 0 || excerpt_failures == 2,
            "both matched-window allocation points");
    printf("body discovery: %zu target and %zu matched-excerpt allocation failures\n",
           target_failures, excerpt_failures);
    note_identity_destroy(identity);
    note_text_destroy(text);
}

void run_body_discovery_tests(void)
{
    fixed_positions();
    exhaustive_small_sources();
    scope_order_and_removal();
    body_query_boundaries();
    context_window();
    long_line_window();
    invalid_windows();
    target_boundaries();
    snippet_projection();
    context_byte_fallback();
    maximum_query_window();
    new_allocation_boundaries();
}
