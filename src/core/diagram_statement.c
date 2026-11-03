#include "diagram_statement.h"
#include "diagram_limits.h"
#include "diagram_statement_scan.h"
#include <string.h>
static bool line_end(char value)
{
    return value == '\r' || value == '\n' || value == ';';
}
static bool entity_character(char value)
{
    return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'z') ||
           (value >= 'A' && value <= 'Z') || value == '#';
}
static size_t entity_end(struct diagram_text text, size_t at)
{
    if (text.bytes[at] != '#' && text.bytes[at] != '&')
        return at;
    for (size_t end = at + 1; end < text.length && end - at < 32; ++end)
    {
        char value = text.bytes[end];
        if (value == ';')
            return end;
        if (!entity_character(value))
            return at;
    }
    return at;
}
static size_t skip_line(struct diagram_text text, size_t start)
{
    while (start < text.length && text.bytes[start] != '\r' && text.bytes[start] != '\n')
        ++start;
    return start;
}
static enum diagram_outcome start_at(struct diagram_text text, size_t *_Nonnull cursor)
{
    while (*cursor < text.length)
    {
        char value = text.bytes[*cursor];
        if (value == ' ' || value == '\t' || line_end(value))
        {
            ++*cursor;
            continue;
        }
        struct diagram_text rest = diagram_text_skip(text, *cursor);
        if (!diagram_text_starts(rest, "%%"))
            break;
        if (diagram_text_starts(rest, "%%{"))
            return DIAGRAM_UNSUPPORTED;
        *cursor = skip_line(text, *cursor);
    }
    return DIAGRAM_ACCEPTED;
}
static void quoted_character(struct diagram_statement_scan *_Nonnull scan, struct diagram_text text)
{
    char value = text.bytes[scan->cursor];
    if (value == '\\' && scan->cursor + 1 < text.length)
        ++scan->cursor;
    else if (value == scan->quote)
        scan->quote = 0;
}
static enum diagram_outcome flow_character(struct diagram_statement_scan *_Nonnull scan,
                                           struct diagram_text text)
{
    char value = text.bytes[scan->cursor];
    if (scan->quote != 0)
    {
        quoted_character(scan, text);
        return DIAGRAM_ACCEPTED;
    }
    if (value == '"' || value == '|')
        scan->quote = value;
    if (diagram_text_character_in(value, "[({"))
        ++scan->brackets;
    if (diagram_text_character_in(value, "])}") && scan->brackets != 0)
        --scan->brackets;
    return scan->brackets > 64 ? DIAGRAM_TOO_LARGE : DIAGRAM_ACCEPTED;
}
static bool boundary(const struct diagram_statement_scan *_Nonnull scan, struct diagram_text text)
{
    if (scan->quote != 0 || scan->brackets != 0)
        return false;
    return line_end(text.bytes[scan->cursor]) ||
           diagram_text_starts(diagram_text_skip(text, scan->cursor), "%%");
}
static enum diagram_outcome scan_character(struct diagram_statement_scan *_Nonnull scan,
                                           struct diagram_input input)
{
    enum diagram_outcome outcome = DIAGRAM_ACCEPTED;
    switch (input.kind)
    {
    case DIAGRAM_FLOWCHART:
        outcome = flow_character(scan, input.text);
        break;
    case DIAGRAM_SEQUENCE:
        break;
    }
    if (outcome == DIAGRAM_ACCEPTED)
        scan->cursor = entity_end(input.text, scan->cursor) + 1;
    return outcome;
}
static enum diagram_outcome scan_statement(struct diagram_statement_scan *_Nonnull scan,
                                           struct diagram_input input)
{
    while (scan->cursor < input.text.length && !boundary(scan, input.text))
    {
        enum diagram_outcome outcome = scan_character(scan, input);
        if (outcome != DIAGRAM_ACCEPTED)
            return outcome;
    }
    if (scan->cursor < input.text.length &&
        diagram_text_starts(diagram_text_skip(input.text, scan->cursor), "%%{"))
        return DIAGRAM_UNSUPPORTED;
    return scan->quote != 0 || scan->brackets != 0 ? DIAGRAM_INVALID : DIAGRAM_ACCEPTED;
}
enum diagram_outcome diagram_statement_read(struct diagram_input input, size_t start,
                                            struct diagram_statement *_Nonnull out)
{
    if (start > input.text.length)
        return DIAGRAM_INVALID;
    enum diagram_outcome outcome = start_at(input.text, &start);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    struct diagram_statement_scan scan = {.cursor = start};
    outcome = scan_statement(&scan, input);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    size_t end = scan.cursor;
    if (scan.cursor < input.text.length && input.text.bytes[scan.cursor] == '%')
        scan.cursor = skip_line(input.text, scan.cursor);
    if (scan.cursor < input.text.length)
        ++scan.cursor;
    *out = (struct diagram_statement){
        .text = diagram_text_trim((struct diagram_text){input.text.bytes + start, end - start}),
        .next = scan.cursor};
    return DIAGRAM_ACCEPTED;
}
