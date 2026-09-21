#include "regex_adapter.h"

#include "regex_matches.h"
#include "regex_request.h"

#include <stdlib.h>
#include <windows.h>

#include <icu.h>

/* 1 回の一致操作の上限（ADR 0028 の決定 4(a)）。ICU が自分の中で数える「steps」なので、
 * ここでも core でも時計は読まない（ARC-007）。測定機では 1 step ≒ 0.15ms で、
 * 2,000 steps ≒ 300ms（out/design/2026-09-22/replace-probe の §9）。
 * 上限は 1 回の一致操作ごとに数え直されるので、全走査の最悪は「一致の数 × 上限」まで膨らむ。 */
constexpr int32_t regex_step_limit = 2000;

struct regex_adapter
{
    int32_t steps;
};

/* ICU の UErrorCode を閉じた値へ写す唯一の場所（決定 2）。生の数値は上へ出さない。 */
static enum regex_scan_outcome from_icu(UErrorCode status)
{
    if (U_SUCCESS(status))
    {
        return REGEX_SCAN_READY;
    }
    if (status == U_REGEX_TIME_OUT)
    {
        return REGEX_SCAN_TIMED_OUT;
    }
    if (status == U_REGEX_STACK_OVERFLOW)
    {
        return REGEX_SCAN_TOO_COMPLEX;
    }
    if (status == U_MEMORY_ALLOCATION_ERROR)
    {
        return REGEX_SCAN_OUT_OF_MEMORY;
    }
    return REGEX_SCAN_BAD_PATTERN;
}

/* 群 1〜9 の位置。10 個目以降は報告しない（決定 2）。参加しなかった群は start が負になる。 */
static void read_groups(URegularExpression *_Nonnull expression, struct regex_match *_Nonnull match)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t groups = uregex_groupCount(expression, &status);
    for (int32_t number = 1; number <= groups && number <= (int32_t)regex_match_groups; ++number)
    {
        int32_t start = uregex_start(expression, number, &status);
        int32_t end = uregex_end(expression, number, &status);
        if (U_FAILURE(status) || start < 0 || end < start)
        {
            status = U_ZERO_ERROR;
            continue;
        }
        match->groups[number - 1].span.start = (size_t)start;
        match->groups[number - 1].span.end = (size_t)end;
        match->groups[number - 1].present = true;
    }
}

static struct regex_match whole_match(URegularExpression *_Nonnull expression,
                                      UErrorCode *_Nonnull status)
{
    struct regex_match match = {.whole = {.start = 0, .end = 0}, .groups = {}};
    int32_t start = uregex_start(expression, 0, status);
    int32_t end = uregex_end(expression, 0, status);
    if (U_FAILURE(*status) || start < 0 || end < start)
    {
        return match;
    }
    match.whole.start = (size_t)start;
    match.whole.end = (size_t)end;
    read_groups(expression, &match);
    return match;
}

/* 一致を順に数え、入れ物に入るぶんだけ書く（決定 2）。ゼロ幅の一致でも findNext は前進する。 */
static enum regex_scan_outcome collect(URegularExpression *_Nonnull expression,
                                       struct regex_matches *_Nonnull matches)
{
    UErrorCode status = U_ZERO_ERROR;
    size_t count = 0;
    while (uregex_findNext(expression, &status))
    {
        if (count < matches->capacity)
        {
            matches->items[count] = whole_match(expression, &status);
        }
        count += 1;
        if (count > regex_match_limit)
        {
            return REGEX_SCAN_TOO_MANY; /* 走査を打ち切る（決定 4(b)） */
        }
    }
    matches->count = count;
    return from_icu(status);
}

static enum regex_scan_outcome scan(struct regex_adapter *_Nonnull adapter,
                                    const struct regex_request *_Nonnull request,
                                    struct regex_matches *_Nonnull matches,
                                    struct regex_pattern_error *_Nonnull error)
{
    error->offset = 0;
    matches->count = 0;
    UErrorCode status = U_ZERO_ERROR;
    UParseError parsed = {};
    /* 旗は固定（決定 3）。UNIX_LINES は付けず、大小無視は利用者の `(?i)` に任せる。 */
    uint32_t flags = (uint32_t)UREGEX_MULTILINE | (uint32_t)UREGEX_ERROR_ON_UNKNOWN_ESCAPES;
    URegularExpression *_Nullable expression =
        uregex_open(request->pattern, (int32_t)request->pattern_length, flags, &parsed, &status);
    if (U_FAILURE(status) || expression == nullptr)
    {
        /* UParseError.offset は 1 起算のコード単位。pre/post は切り詰められるので読まない。 */
        error->offset = parsed.offset > 0 ? (size_t)parsed.offset : 0;
        return from_icu(status);
    }
    uregex_setTimeLimit(expression, adapter->steps, &status);
    /* 本文は長さ 0 でも実体のある番地で渡す（NULL は U_ILLEGAL_ARGUMENT_ERROR・probe の §8）。 */
    uregex_setText(expression, request->text, (int32_t)request->length, &status);
    enum regex_scan_outcome scanned =
        U_FAILURE(status) ? from_icu(status) : collect(expression, matches);
    uregex_close(expression);
    return scanned;
}

enum regex_adapter_outcome regex_adapter_create(struct regex_adapter *_Nullable *_Nonnull out)
{
    struct regex_adapter *_Nullable adapter = calloc(1, sizeof *adapter);
    if (adapter == nullptr)
    {
        return REGEX_ADAPTER_OUT_OF_MEMORY;
    }
    adapter->steps = regex_step_limit;
    *out = adapter;
    return REGEX_ADAPTER_CREATED;
}

struct regex_port regex_adapter_port(struct regex_adapter *_Nonnull adapter)
{
    struct regex_port port = {.adapter = adapter, .scan = scan};
    return port;
}

void regex_adapter_destroy(struct regex_adapter *_Nullable adapter)
{
    free(adapter);
}
