#include "open_request.h"
#include "ascii_fold.h"
#include "category_name.h"
#include "note_name.h"
#include "utf8_text.h"
#include <stdlib.h>
#include <string.h>

struct open_request
{
    struct category_name *_Nullable category;
    struct note_name *_Nullable note;
    char *_Nullable text; /* 終端付き。無ければ nullptr */
    size_t text_length;
    struct open_request_lines lines;
    bool lines_seen;
    bool text_seen;
};

/* bytes が word（小文字の ASCII）で始まるか。ASCII の大小は問わない。 */
static bool starts_folded(const char *_Nonnull bytes, size_t length, const char *_Nonnull word)
{
    size_t count = strlen(word);
    if (length < count)
        return false;
    for (size_t index = 0; index < count; ++index)
    {
        if (ascii_fold_byte(bytes[index]) != word[index])
            return false;
    }
    return true;
}

/* `nenefolio:[//]open[/]?` の次の位置。形が違えば 0（query は 0 から始まらない）。 */
static size_t query_start(const char *_Nonnull bytes, size_t length)
{
    static const char scheme[] = "nenefolio:";
    static const char action[] = "open";
    if (!starts_folded(bytes, length, scheme))
        return 0;
    size_t at = sizeof scheme - 1;
    if (length - at >= 2 && bytes[at] == '/' && bytes[at + 1] == '/')
        at += 2;
    if (!starts_folded(bytes + at, length - at, action))
        return 0;
    at += sizeof action - 1;
    if (at < length && bytes[at] == '/')
        at += 1;
    return at < length && bytes[at] == '?' ? at + 1 : 0;
}

/* 16 進の 1 桁の値。桁でなければ 16。 */
static unsigned hex_digit(char value)
{
    if (value >= '0' && value <= '9')
        return (unsigned)(value - '0');
    char folded = ascii_fold_byte(value);
    return folded >= 'a' && folded <= 'f' ? (unsigned)(folded - 'a') + 10 : 16;
}

/* value の at にある 1 バイト（% なら 3 文字ぶん）を out へ入れ、読んだ文字数を返す。
 * % の後が 16 進 2 桁でなければ 0。 */
static size_t decoded_byte(const char *_Nonnull value, size_t length, size_t at,
                           unsigned char *_Nonnull out)
{
    if (value[at] != '%')
    {
        *out = (unsigned char)value[at];
        return 1;
    }
    if (length - at < 3)
        return 0;
    unsigned high = hex_digit(value[at + 1]);
    unsigned low = hex_digit(value[at + 2]);
    if (high > 15 || low > 15)
        return 0;
    *out = (unsigned char)(high * 16 + low);
    return 3;
}

/* 全体が妥当な UTF-8 か。 */
static bool valid_utf8(const char *_Nonnull bytes, size_t length)
{
    size_t at = 0;
    while (at < length)
    {
        uint32_t point = 0;
        size_t used = utf8_text_decode(bytes + at, length - at, &point);
        if (used == 0)
            return false;
        at += used;
    }
    return true;
}

/* % を戻した値（終端付き）を作る。制御文字（NUL を含む）と不正な UTF-8 は MALFORMED。 */
static enum open_request_outcome decode(const char *_Nonnull value, size_t length,
                                        char *_Nullable *_Nonnull out, size_t *_Nonnull out_length)
{
    char *decoded = malloc(length + 1);
    if (decoded == nullptr)
        return OPEN_REQUEST_OUT_OF_MEMORY;
    size_t used = 0;
    size_t at = 0;
    while (at < length)
    {
        unsigned char byte = 0;
        size_t read = decoded_byte(value, length, at, &byte);
        if (read == 0 || byte < 0x20 || byte == 0x7F)
        {
            free(decoded);
            return OPEN_REQUEST_MALFORMED;
        }
        decoded[used++] = (char)byte;
        at += read;
    }
    decoded[used] = '\0';
    if (!valid_utf8(decoded, used))
    {
        free(decoded);
        return OPEN_REQUEST_MALFORMED;
    }
    *out = decoded;
    *out_length = used;
    return OPEN_REQUEST_ACCEPTED;
}

static enum open_request_outcome take_category(struct open_request *_Nonnull request,
                                               const char *_Nonnull text, size_t length)
{
    if (request->category != nullptr)
        return OPEN_REQUEST_MALFORMED;
    switch (category_name_create(text, length, &request->category))
    {
    case CATEGORY_NAME_ACCEPTED:
        return OPEN_REQUEST_ACCEPTED;
    case CATEGORY_NAME_INVALID:
        return OPEN_REQUEST_NAME_INVALID;
    case CATEGORY_NAME_OUT_OF_MEMORY:
        return OPEN_REQUEST_OUT_OF_MEMORY;
    }
    return OPEN_REQUEST_NAME_INVALID;
}

static enum open_request_outcome take_note(struct open_request *_Nonnull request,
                                           const char *_Nonnull text, size_t length)
{
    if (request->note != nullptr)
        return OPEN_REQUEST_MALFORMED;
    switch (note_name_create(text, length, &request->note))
    {
    case NOTE_NAME_ACCEPTED:
        return OPEN_REQUEST_ACCEPTED;
    case NOTE_NAME_INVALID:
        return OPEN_REQUEST_NAME_INVALID;
    case NOTE_NAME_OUT_OF_MEMORY:
        return OPEN_REQUEST_OUT_OF_MEMORY;
    }
    return OPEN_REQUEST_NAME_INVALID;
}

/* text の at から 10 進の行番号を読み、次の位置を返す。桁が無い・0・上限を超えるなら 0。 */
static size_t line_number(const char *_Nonnull text, size_t length, size_t at, size_t *_Nonnull out)
{
    size_t value = 0;
    size_t start = at;
    while (at < length && text[at] >= '0' && text[at] <= '9')
    {
        value = value * 10 + (size_t)(text[at] - '0');
        if (value > open_request_line_limit)
            return 0;
        at += 1;
    }
    if (at == start || value == 0)
        return 0;
    *out = value;
    return at;
}

/* `N` または `N-M`（1 <= N <= M）。 */
static enum open_request_outcome take_lines(struct open_request *_Nonnull request,
                                            const char *_Nonnull text, size_t length)
{
    if (request->lines_seen)
        return OPEN_REQUEST_MALFORMED;
    struct open_request_lines lines = {0, 0};
    size_t at = line_number(text, length, 0, &lines.first);
    if (at == 0)
        return OPEN_REQUEST_MALFORMED;
    lines.last = lines.first;
    if (at < length)
    {
        if (text[at] != '-')
            return OPEN_REQUEST_MALFORMED;
        at = line_number(text, length, at + 1, &lines.last);
        if (at != length || lines.last < lines.first)
            return OPEN_REQUEST_MALFORMED;
    }
    request->lines = lines;
    request->lines_seen = true;
    return OPEN_REQUEST_ACCEPTED;
}

/* 探す文を写して持つ。空なら無いのと同じ。 */
static enum open_request_outcome take_text(struct open_request *_Nonnull request,
                                           const char *_Nonnull text, size_t length)
{
    if (request->text_seen)
        return OPEN_REQUEST_MALFORMED;
    if (length > open_request_text_limit)
        return OPEN_REQUEST_TOO_LONG;
    request->text_seen = true;
    if (length == 0)
        return OPEN_REQUEST_ACCEPTED;
    char *copy = malloc(length + 1);
    if (copy == nullptr)
        return OPEN_REQUEST_OUT_OF_MEMORY;
    memcpy(copy, text, length + 1);
    request->text = copy;
    request->text_length = length;
    return OPEN_REQUEST_ACCEPTED;
}

/* 知っているキーの番号（下の known の順）。知らなければ known_count。 */
constexpr size_t known_count = 4;
static const char *const known[known_count] = {"category", "note", "lines", "text"};

static size_t known_key(const char *_Nonnull key, size_t length)
{
    for (size_t index = 0; index < known_count; ++index)
    {
        if (strlen(known[index]) == length && memcmp(key, known[index], length) == 0)
            return index;
    }
    return known_count;
}

/* % を戻した値を、キーの番号の受け取り手へ渡す。 */
static enum open_request_outcome take_decoded(struct open_request *_Nonnull request, size_t which,
                                              const char *_Nonnull decoded, size_t length)
{
    if (which == 0)
        return take_category(request, decoded, length);
    if (which == 1)
        return take_note(request, decoded, length);
    if (which == 2)
        return take_lines(request, decoded, length);
    return take_text(request, decoded, length);
}

/* `key=value` を 1 つ読む。キーが空・`=` が無いものは MALFORMED。知らないキーは値を読まずに
 * 読み飛ばす。 */
static enum open_request_outcome read_pair(struct open_request *_Nonnull request,
                                           const char *_Nonnull pair, size_t length)
{
    const char *equals = memchr(pair, '=', length);
    if (equals == nullptr || equals == pair)
        return OPEN_REQUEST_MALFORMED;
    size_t key_length = (size_t)(equals - pair);
    size_t which = known_key(pair, key_length);
    if (which == known_count)
        return OPEN_REQUEST_ACCEPTED;
    char *_Nullable decoded = nullptr;
    size_t decoded_length = 0;
    enum open_request_outcome outcome =
        decode(equals + 1, length - key_length - 1, &decoded, &decoded_length);
    if (outcome != OPEN_REQUEST_ACCEPTED || decoded == nullptr)
        return outcome;
    outcome = take_decoded(request, which, decoded, decoded_length);
    free(decoded);
    return outcome;
}

/* `&` で区切った query を読む。`#` と空の区切りは MALFORMED。 */
static enum open_request_outcome read_query(struct open_request *_Nonnull request,
                                            const char *_Nonnull query, size_t length)
{
    if (memchr(query, '#', length) != nullptr)
        return OPEN_REQUEST_MALFORMED;
    size_t at = 0;
    enum open_request_outcome outcome = OPEN_REQUEST_ACCEPTED;
    while (outcome == OPEN_REQUEST_ACCEPTED && at <= length)
    {
        const char *next = at < length ? memchr(query + at, '&', length - at) : nullptr;
        size_t end = next != nullptr ? (size_t)(next - query) : length;
        outcome = read_pair(request, query + at, end - at);
        at = end + 1;
    }
    return outcome;
}

enum open_request_outcome open_request_create(const char *_Nonnull bytes, size_t length,
                                              struct open_request *_Nullable *_Nonnull out)
{
    if (length > open_request_byte_limit)
        return OPEN_REQUEST_TOO_LONG;
    size_t start = query_start(bytes, length);
    if (start == 0)
        return OPEN_REQUEST_MALFORMED;
    struct open_request *request = calloc(1, sizeof *request);
    if (request == nullptr)
        return OPEN_REQUEST_OUT_OF_MEMORY;
    enum open_request_outcome outcome = read_query(request, bytes + start, length - start);
    if (outcome == OPEN_REQUEST_ACCEPTED &&
        (request->category == nullptr || request->note == nullptr))
        outcome = OPEN_REQUEST_MALFORMED;
    if (outcome != OPEN_REQUEST_ACCEPTED)
    {
        open_request_destroy(request);
        return outcome;
    }
    *out = request;
    return OPEN_REQUEST_ACCEPTED;
}

const struct category_name *_Nonnull open_request_category(
    const struct open_request *_Nonnull request)
{
    return request->category;
}

const struct note_name *_Nonnull open_request_note(const struct open_request *_Nonnull request)
{
    return request->note;
}

bool open_request_lines(const struct open_request *_Nonnull request,
                        struct open_request_lines *_Nonnull out)
{
    if (!request->lines_seen)
        return false;
    *out = request->lines;
    return true;
}

const char *_Nonnull open_request_text(const struct open_request *_Nonnull request)
{
    return request->text != nullptr ? request->text : "";
}

size_t open_request_text_length(const struct open_request *_Nonnull request)
{
    return request->text_length;
}

void open_request_destroy(struct open_request *_Nullable request)
{
    if (request == nullptr)
        return;
    category_name_destroy(request->category);
    note_name_destroy(request->note);
    free(request->text);
    free(request);
}
