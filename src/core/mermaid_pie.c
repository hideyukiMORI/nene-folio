/* #955 / ADR0112: Mermaid `pie`. The header may carry `showData` and a title; every other line
 * is `title ...`, `showData`, or a slice `"name" : number`. Values are positive decimals read as
 * thousandths; anything else never produces a partial chart. */
#include "mermaid_pie.h"
#include "diagram_label.h"
#include "diagram_statement.h"
#include "mermaid_pie_slice.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
constexpr size_t integer_digit_limit = 9;
constexpr uint64_t thousand = 1000;
struct mermaid_pie
{
    struct diagram_model *_Nonnull model;
    enum diagram_outcome outcome;
    bool show_data;
    bool titled;
    size_t count;
    uint64_t total;
    struct mermaid_pie_slice slices[mermaid_pie_slice_limit];
    size_t used;
    char label[diagram_label_limit];
};
static bool reject(struct mermaid_pie *_Nonnull parser, enum diagram_outcome outcome)
{
    parser->outcome = outcome;
    return false;
}
static bool digit(char value)
{
    return value >= '0' && value <= '9';
}
/* The digits after the point, as thousandths; digits past the third place are not counted.
 * Returns how many digits were read. */
static size_t read_fraction(struct diagram_text text, size_t at, uint64_t *_Nonnull fraction)
{
    size_t first = at;
    uint64_t scale = thousand / 10;
    for (; at < text.length && digit(text.bytes[at]); ++at, scale /= 10)
        *fraction += (uint64_t)(text.bytes[at] - '0') * scale;
    return at - first;
}
/* `12`, `12.5`, `0.125`: thousandths. */
static bool read_value(struct diagram_text text, uint64_t *_Nonnull value)
{
    size_t at = 0;
    uint64_t whole = 0;
    while (at < text.length && digit(text.bytes[at]))
        whole = whole * 10 + (uint64_t)(text.bytes[at++] - '0');
    if (at == 0 || at > integer_digit_limit)
        return false;
    uint64_t fraction = 0;
    if (at < text.length && text.bytes[at] == '.')
    {
        size_t digits = read_fraction(text, at + 1, &fraction);
        if (digits == 0)
            return false;
        at += 1 + digits;
    }
    *value = whole * thousand + fraction;
    return at == text.length;
}
/* `"name" : number`. The name is what the quotes hold; it may not be empty. */
static bool slice(struct mermaid_pie *_Nonnull parser, struct diagram_text text)
{
    size_t close = 1;
    while (close < text.length && text.bytes[close] != '"')
        ++close;
    struct diagram_text name = diagram_text_trim((struct diagram_text){text.bytes + 1, close - 1});
    struct diagram_text rest = diagram_text_trim(diagram_text_skip(text, close + 1));
    if (close >= text.length || name.length == 0 || rest.length == 0 || rest.bytes[0] != ':')
        return reject(parser, DIAGRAM_INVALID);
    struct diagram_text number = diagram_text_trim(diagram_text_skip(rest, 1));
    uint64_t value = 0;
    if (!read_value(number, &value) || value == 0)
        return reject(parser, DIAGRAM_INVALID);
    if (parser->count == mermaid_pie_slice_limit)
        return reject(parser, DIAGRAM_TOO_LARGE);
    parser->slices[parser->count++] = (struct mermaid_pie_slice){name, number, value};
    parser->total += value;
    return true;
}
static bool title(struct mermaid_pie *_Nonnull parser, struct diagram_text text)
{
    text = diagram_text_trim(text);
    if (parser->titled || text.length == 0)
        return reject(parser, DIAGRAM_INVALID);
    size_t index = 0;
    parser->outcome = diagram_label_store(parser->model, text, &index);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    diagram_model_title(parser->model, index);
    parser->titled = true;
    return true;
}
/* What may follow `pie` on the header line and stand on a line of its own. */
static bool option(struct mermaid_pie *_Nonnull parser, struct diagram_text text)
{
    struct diagram_text rest = text;
    struct diagram_text word = diagram_text_word(&rest);
    if (diagram_text_equal(word, "showData"))
    {
        parser->show_data = true;
        return rest.length == 0 || option(parser, rest);
    }
    if (diagram_text_equal(word, "title"))
        return title(parser, rest);
    if (diagram_text_starts(text, "accTitle") || diagram_text_starts(text, "accDescr"))
        return reject(parser, DIAGRAM_UNSUPPORTED);
    return reject(parser, DIAGRAM_INVALID);
}
static bool statement(struct mermaid_pie *_Nonnull parser, struct diagram_text text)
{
    if (text.length == 0)
        return true;
    return text.bytes[0] == '"' ? slice(parser, text) : option(parser, text);
}
static bool append(struct mermaid_pie *_Nonnull parser, const char *_Nonnull bytes, size_t length)
{
    if (length > sizeof parser->label - parser->used)
        return reject(parser, DIAGRAM_TOO_LARGE);
    memcpy(parser->label + parser->used, bytes, length);
    parser->used += length;
    return true;
}
static bool append_number(struct mermaid_pie *_Nonnull parser, uint64_t value)
{
    char digits[20];
    size_t count = 0;
    do
        digits[count++] = (char)('0' + value % 10);
    while ((value /= 10) != 0);
    while (count != 0)
        if (!append(parser, &digits[--count], 1))
            return false;
    return true;
}
/* The legend line: `name 40%`, or `name [12.5] 40%` with showData. Percentages are rounded to
 * whole numbers, as Mermaid shows them. */
static bool legend(struct mermaid_pie *_Nonnull parser,
                   const struct mermaid_pie_slice *_Nonnull part, size_t *_Nonnull index)
{
    parser->used = 0;
    bool written = append(parser, part->name.bytes, part->name.length);
    if (written && parser->show_data)
        written = append(parser, " [", 2) &&
                  append(parser, part->number.bytes, part->number.length) && append(parser, "]", 1);
    uint64_t percent = (part->value * 100 + parser->total / 2) / parser->total;
    if (!written || !append(parser, " ", 1) || !append_number(parser, percent) ||
        !append(parser, "%", 1))
        return false;
    parser->outcome = diagram_model_text(parser->model, parser->label, parser->used, index);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
/* One node per slice. The last slice takes what rounding left, so the sectors close the circle. */
static bool build(struct mermaid_pie *_Nonnull parser)
{
    size_t spent = 0;
    for (size_t at = 0; at < parser->count; ++at)
    {
        const struct mermaid_pie_slice *part = &parser->slices[at];
        struct diagram_node node = {.group = diagram_none, .shape = DIAGRAM_SECTOR};
        node.share = at + 1 == parser->count
                         ? mermaid_pie_circle - spent
                         : (size_t)(part->value * mermaid_pie_circle / parser->total);
        spent += node.share;
        parser->outcome = diagram_label_store(parser->model, part->name, &node.id);
        if (parser->outcome != DIAGRAM_ACCEPTED || !legend(parser, part, &node.label))
            return false;
        parser->outcome = diagram_model_node(parser->model, at, node);
        if (parser->outcome != DIAGRAM_ACCEPTED)
            return false;
    }
    return true;
}
static bool parse_document(struct mermaid_pie *_Nonnull parser, struct diagram_text source)
{
    struct diagram_input input = {.text = source, .kind = DIAGRAM_SEQUENCE};
    struct diagram_statement first;
    parser->outcome = diagram_statement_read(input, 0, &first);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    struct diagram_text header = diagram_text_word(&first.text);
    if (!diagram_text_equal(header, "pie"))
        return reject(parser, DIAGRAM_INVALID);
    if (first.text.length != 0 && !option(parser, first.text))
        return false;
    size_t cursor = first.next;
    while (cursor < source.length)
    {
        struct diagram_statement next;
        parser->outcome = diagram_statement_read(input, cursor, &next);
        if (parser->outcome != DIAGRAM_ACCEPTED || !statement(parser, next.text))
            return false;
        cursor = next.next;
    }
    if (parser->count == 0)
        return reject(parser, DIAGRAM_INVALID);
    return build(parser);
}
enum diagram_outcome mermaid_pie_create(struct diagram_text source,
                                        struct diagram_model *_Nullable *_Nonnull out)
{
    struct diagram_model *_Nullable model = nullptr;
    enum diagram_outcome outcome = diagram_model_create(DIAGRAM_PIE, &model);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    struct mermaid_pie *_Nullable parser = calloc(1, sizeof *parser);
    if (parser == nullptr)
    {
        diagram_model_destroy(model);
        return DIAGRAM_OUT_OF_MEMORY;
    }
    parser->model = model;
    parser->outcome = DIAGRAM_ACCEPTED;
    (void)parse_document(parser, source);
    outcome = parser->outcome;
    free(parser);
    if (outcome != DIAGRAM_ACCEPTED)
    {
        diagram_model_destroy(model);
        return outcome;
    }
    diagram_model_seal(model);
    *out = model;
    return DIAGRAM_ACCEPTED;
}
