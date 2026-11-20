#include "open_place.h"
#include "open_request.h"
#include <string.h>

/* [from, length) で最初に value がある位置。無ければ length。 */
static size_t byte_index(const char *_Nonnull bytes, size_t from, size_t length, char value)
{
    size_t at = from;
    while (at < length && bytes[at] != value)
        at += 1;
    return at;
}

/* 1 始まりの line 行目が始まるバイト位置。行がそこまで無ければ length を超える値（length + 1）。 */
static size_t line_start(const char *_Nonnull source, size_t length, size_t line)
{
    size_t at = 0;
    for (size_t current = 1; current < line; ++current)
    {
        size_t newline = byte_index(source, at, length, '\n');
        if (newline >= length)
            return length + 1;
        at = newline + 1;
    }
    return at;
}

/* at から始まる行の終わり（改行と、その前の CR を含まない）。 */
static size_t line_end(const char *_Nonnull source, size_t length, size_t at)
{
    size_t end = byte_index(source, at, length, '\n');
    return end > at && source[end - 1] == '\r' ? end - 1 : end;
}

/* first〜last 行のバイト範囲。first 行目が無ければ false で out は変えない（改行で終わるノートの、
 * 最後の改行の後の空の「行」も行として数えない）。last が終わりを越えれば切り詰める。 */
static bool lines_span(const char *_Nonnull source, size_t length, struct open_request_lines lines,
                       struct note_byte_span *_Nonnull out)
{
    size_t start = line_start(source, length, lines.first);
    if (start >= length)
        return false;
    size_t last = line_start(source, length, lines.last);
    size_t end = last >= length ? length : line_end(source, length, last);
    while (end > start && (source[end - 1] == '\n' || source[end - 1] == '\r'))
        end -= 1; /* a range cut at the note's end leaves the final newline out */
    out->start = start;
    out->end = end;
    return true;
}

/* within の中で、依頼の探す文の最初の一致を探す。文が無ければ見つからない。 */
static bool find_text(const char *_Nonnull source, struct note_byte_span within,
                      const struct open_request *_Nonnull request,
                      struct note_byte_span *_Nonnull out)
{
    const char *needle = open_request_text(request);
    size_t needle_length = open_request_text_length(request);
    if (needle_length == 0 || within.end - within.start < needle_length)
        return false;
    size_t last = within.end - needle_length;
    for (size_t at = within.start; at <= last; ++at)
    {
        if (source[at] == needle[0] && memcmp(source + at, needle, needle_length) == 0)
        {
            out->start = at;
            out->end = at + needle_length;
            return true;
        }
    }
    return false;
}

/* 指定の行の前後 open_place_margin 行までの範囲。 */
static struct note_byte_span nearby(const char *_Nonnull source, size_t length,
                                    struct open_request_lines lines)
{
    struct open_request_lines wide = {
        .first = lines.first > open_place_margin ? lines.first - open_place_margin : 1,
        .last = lines.last + open_place_margin};
    struct note_byte_span span = {0, 0};
    if (!lines_span(source, length, wide, &span))
        span = (struct note_byte_span){length, length};
    return span;
}

enum open_place_kind open_place_find(const char *_Nonnull source, size_t length,
                                     const struct open_request *_Nonnull request,
                                     struct note_byte_span *_Nonnull out)
{
    struct open_request_lines lines = {0, 0};
    bool lined = open_request_lines(request, &lines);
    if (lined && find_text(source, nearby(source, length, lines), request, out))
        return OPEN_PLACE_TEXT_NEAR;
    struct note_byte_span whole = {0, length};
    if (find_text(source, whole, request, out))
        return OPEN_PLACE_TEXT_ELSEWHERE;
    return lined && lines_span(source, length, lines, out) ? OPEN_PLACE_LINES : OPEN_PLACE_NONE;
}
