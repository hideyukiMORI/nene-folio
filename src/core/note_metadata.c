#include "note_metadata.h"
#include "markdown_line.h"
#include "metadata_scalar.h"
#include "note_byte_span.h"
#include "note_tag.h"
#include "note_tags.h"
#include "note_text.h"
#include "persisted_size_limit.h"
#include "replace_edit.h"
#include "text_buffer.h"
#include "utf8_text.h"
#include <stdlib.h>
#include <string.h>

struct note_metadata
{
    const char *_Nonnull source;
    size_t length;
    struct note_tags *_Nonnull tags;
    enum note_metadata_status status;
    size_t body;
    size_t closing;
    enum
    {
        TAGS_ABSENT,
        TAGS_BLOCK,
        TAGS_FLOW
    } style;
    size_t tag_start;
    size_t tag_value;
    size_t tag_end;
    size_t tag_indent;
    struct
    {
        struct note_tag *_Nullable name;
        size_t start;
        size_t end;
        size_t token;
        size_t token_end;
    } *_Nullable items;
    size_t item_count;
    /* Construction-only key spans and block state; no parse borrow escapes its owning note_text. */
    struct
    {
        size_t start;
        size_t length;
    } *_Nullable keys;
    size_t key_count;
    enum
    {
        BLOCK_NONE,
        BLOCK_SEQUENCE,
        BLOCK_SCALAR
    } block;
    size_t indent;
    bool collect_tags;
    bool allocation_failed;
};

static bool space(char byte)
{
    return byte == ' ' || byte == '\t';
}

static size_t separation(struct markdown_line line, size_t at)
{
    while (at < line.length && space(line.bytes[at]))
        ++at;
    return at;
}

static bool ignored(struct markdown_line line)
{
    return line.content == line.length || line.bytes[line.content] == '#';
}

static bool delimiter(struct markdown_line line)
{
    return line.length == 3 && memcmp(line.bytes, "---", 3) == 0;
}

static bool mapping_candidate(struct markdown_line line)
{
    for (size_t at = line.content; at < line.length; ++at)
        if (line.bytes[at] == ':' && (at + 1 == line.length || space(line.bytes[at + 1])))
            return true;
    return false;
}

static bool quoted_mapping_candidate(struct markdown_line line)
{
    char first = line.bytes[line.content];
    if (first != '"' && first != '\'')
        return false;
    for (size_t at = line.content + 1; at < line.length; ++at)
        if (line.bytes[at] == ':')
            return true;
    return false;
}

static bool explicit_mapping(char first)
{
    return first == '%' || first == '{' || first == '!' || first == '&';
}

static bool ordinary_marker(struct markdown_line line)
{
    char first = line.bytes[line.content];
    return (first == '-' || first == '*' || first == '>') && line.content + 1 < line.length &&
           space(line.bytes[line.content + 1]);
}

static bool candidate(struct markdown_line line)
{
    if (line.content >= line.length || line.indent >= 4)
        return false;
    if (explicit_mapping(line.bytes[line.content]))
        return true;
    if (quoted_mapping_candidate(line))
        return true;
    if (ordinary_marker(line))
        return false;
    return mapping_candidate(line);
}

static size_t bounded_length(const struct note_metadata *_Nonnull metadata)
{
    return metadata->length <= note_metadata_max_bytes ? metadata->length
                                                       : note_metadata_max_bytes + 1;
}

static bool begins(struct note_metadata *_Nonnull metadata, size_t *_Nonnull cursor)
{
    size_t length = bounded_length(metadata);
    struct markdown_line line = markdown_line_read(metadata->source, length, 0);
    if (!delimiter(line) || line.next == line.length)
        return false;
    *cursor = line.next;
    size_t at = line.next;
    while (at < length)
    {
        line = markdown_line_read(metadata->source, length, at);
        if (line.next > note_metadata_max_bytes)
        {
            metadata->status = NOTE_METADATA_LIMIT;
            return false;
        }
        if (!ignored(line))
            return candidate(line);
        at = line.next;
    }
    return false;
}

static bool fail(struct note_metadata *_Nonnull metadata, enum note_metadata_status status)
{
    metadata->status = status;
    return false;
}

static bool no_memory(struct note_metadata *_Nonnull metadata)
{
    metadata->allocation_failed = true;
    return false;
}

static bool alpha(char byte)
{
    return (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') || byte == '_';
}

static size_t key_length(struct markdown_line line)
{
    if (line.content != 0 || line.length == 0 || !alpha(line.bytes[0]))
        return 0;
    size_t at = 1;
    while (at < line.length &&
           (alpha(line.bytes[at]) || (line.bytes[at] >= '0' && line.bytes[at] <= '9') ||
            line.bytes[at] == '-'))
        ++at;
    return at;
}

static bool store_key(struct note_metadata *_Nonnull metadata, size_t start, size_t length)
{
    if (metadata->key_count == note_metadata_max_keys)
        return fail(metadata, NOTE_METADATA_LIMIT);
    for (size_t index = 0; index < metadata->key_count; ++index)
        if (metadata->keys[index].length == length &&
            memcmp(metadata->source + metadata->keys[index].start, metadata->source + start,
                   length) == 0)
            return fail(metadata, NOTE_METADATA_MALFORMED);
    metadata->keys[metadata->key_count].start = start;
    metadata->keys[metadata->key_count++].length = length;
    return true;
}

static bool scalar_result(struct note_metadata *_Nonnull metadata,
                          enum metadata_scalar_outcome outcome)
{
    switch (outcome)
    {
    case METADATA_SCALAR_READY:
        return true;
    case METADATA_SCALAR_MALFORMED:
        return fail(metadata, NOTE_METADATA_MALFORMED);
    case METADATA_SCALAR_UNSUPPORTED:
        return fail(metadata, NOTE_METADATA_UNSUPPORTED);
    case METADATA_SCALAR_OUT_OF_MEMORY:
        return no_memory(metadata);
    }
    return false;
}

static bool remember_tag(struct note_metadata *_Nonnull metadata,
                         const struct metadata_scalar *_Nonnull scalar, size_t token,
                         size_t line_start)
{
    if (metadata->item_count == note_tags_max_count)
        return fail(metadata, NOTE_METADATA_LIMIT);
    if (!metadata_scalar_is_string(scalar))
        return fail(metadata, NOTE_METADATA_INVALID_TAG);
    size_t index = metadata->item_count;
    enum note_tag_outcome outcome =
        note_tag_create(metadata_scalar_bytes(scalar), metadata_scalar_length(scalar),
                        &metadata->items[index].name);
    if (outcome == NOTE_TAG_OUT_OF_MEMORY)
        return no_memory(metadata);
    if (outcome != NOTE_TAG_READY)
        return fail(metadata, NOTE_METADATA_INVALID_TAG);
    metadata->items[index].start = line_start;
    metadata->items[index].token = token;
    metadata->items[index].token_end = token + metadata_scalar_consumed(scalar);
    ++metadata->item_count;
    return true;
}

static bool scalar(struct note_metadata *_Nonnull metadata, struct markdown_line line,
                   size_t *_Nonnull at, bool flow)
{
    struct metadata_scalar *_Nullable value = nullptr;
    enum metadata_scalar_outcome outcome =
        metadata_scalar_create(line.bytes + *at, line.length - *at,
                               flow ? METADATA_SCALAR_FLOW : METADATA_SCALAR_BLOCK, &value);
    if (!scalar_result(metadata, outcome))
        return false;
    size_t line_start = (size_t)(line.bytes - metadata->source);
    bool accepted =
        !metadata->collect_tags || remember_tag(metadata, value, line_start + *at, line_start);
    if (accepted && metadata->collect_tags)
        metadata->items[metadata->item_count - 1].end = line.next;
    *at += metadata_scalar_consumed(value);
    metadata_scalar_destroy(value);
    return accepted;
}

static bool tail(struct note_metadata *_Nonnull metadata, struct markdown_line line, size_t at)
{
    size_t end = separation(line, at);
    bool comment = end > at && end < line.length && line.bytes[end] == '#';
    return end == line.length || comment || fail(metadata, NOTE_METADATA_MALFORMED);
}

static bool flow_end(struct note_metadata *_Nonnull metadata, struct markdown_line line, size_t at)
{
    if (metadata->collect_tags)
        metadata->tag_end = (size_t)(line.bytes - metadata->source) + at + 1;
    return tail(metadata, line, at + 1);
}

static bool flow_item(struct note_metadata *_Nonnull metadata, struct markdown_line line,
                      size_t *_Nonnull at)
{
    if (!scalar(metadata, line, at, true))
        return false;
    *at = separation(line, *at);
    if (*at == line.length)
        return fail(metadata, NOTE_METADATA_MALFORMED);
    if (line.bytes[*at] == ']')
        return true;
    return line.bytes[(*at)++] == ',' || fail(metadata, NOTE_METADATA_MALFORMED);
}

static bool flow_sequence(struct note_metadata *_Nonnull metadata, struct markdown_line line,
                          size_t at)
{
    ++at;
    for (;;)
    {
        at = separation(line, at);
        if (at == line.length)
            return fail(metadata, NOTE_METADATA_MALFORMED);
        if (line.bytes[at] == ']')
            return flow_end(metadata, line, at);
        if (!flow_item(metadata, line, &at))
            return false;
    }
}

static bool block_scalar(struct note_metadata *_Nonnull metadata, struct markdown_line line,
                         size_t at)
{
    bool chomp = false;
    bool indentation = false;
    ++at;
    while (at < line.length && !space(line.bytes[at]))
    {
        char byte = line.bytes[at++];
        if ((byte == '+' || byte == '-') && !chomp)
            chomp = true;
        else if (byte >= '1' && byte <= '9' && !indentation)
        {
            metadata->indent = (size_t)(byte - '0');
            indentation = true;
        }
        else
            return fail(metadata, NOTE_METADATA_MALFORMED);
    }
    metadata->block = BLOCK_SCALAR;
    return tail(metadata, line, at);
}

static bool value(struct note_metadata *_Nonnull metadata, struct markdown_line line, size_t at)
{
    if (at == line.length || line.bytes[at] == '#')
    {
        metadata->block = BLOCK_SEQUENCE;
        return true;
    }
    if (line.bytes[at] == '[')
    {
        if (metadata->collect_tags)
            metadata->style = TAGS_FLOW;
        return flow_sequence(metadata, line, at);
    }
    if (metadata->collect_tags)
        return fail(metadata, NOTE_METADATA_UNSUPPORTED);
    if (line.bytes[at] == '|' || line.bytes[at] == '>')
        return block_scalar(metadata, line, at);
    return scalar(metadata, line, &at, false) && tail(metadata, line, at);
}

static bool mapping(struct note_metadata *_Nonnull metadata, struct markdown_line line)
{
    size_t key = key_length(line);
    size_t at = separation(line, key);
    if (key == 0 || at == line.length || line.bytes[at] != ':')
        return fail(metadata, NOTE_METADATA_UNSUPPORTED);
    ++at;
    if (at < line.length && !space(line.bytes[at]))
        return fail(metadata, NOTE_METADATA_MALFORMED);
    if (!store_key(metadata, (size_t)(line.bytes - metadata->source), key))
        return false;
    metadata->block = BLOCK_NONE;
    metadata->indent = 0;
    metadata->collect_tags = key == 4 && memcmp(line.bytes, "tags", 4) == 0;
    at = separation(line, at);
    if (metadata->collect_tags)
    {
        metadata->style = TAGS_BLOCK;
        metadata->tag_start = (size_t)(line.bytes - metadata->source);
        metadata->tag_value = metadata->tag_start + at;
        metadata->tag_end = line.next;
    }
    return value(metadata, line, at);
}

static bool sequence_line(struct note_metadata *_Nonnull metadata, struct markdown_line line)
{
    if (metadata->indent == 0)
        metadata->indent = line.content;
    if (line.content != metadata->indent || line.length - line.content < 2 ||
        line.bytes[line.content] != '-' || !space(line.bytes[line.content + 1]))
        return fail(metadata, NOTE_METADATA_UNSUPPORTED);
    size_t at = separation(line, line.content + 1);
    if (!scalar(metadata, line, &at, false) || !tail(metadata, line, at))
        return false;
    if (metadata->collect_tags)
    {
        metadata->tag_indent = metadata->indent;
        metadata->tag_end = line.next;
    }
    return true;
}

static bool indented(struct note_metadata *_Nonnull metadata, struct markdown_line line)
{
    for (size_t at = 0; at < line.content; ++at)
        if (line.bytes[at] != ' ')
            return fail(metadata, NOTE_METADATA_UNSUPPORTED);
    switch (metadata->block)
    {
    case BLOCK_NONE:
        return fail(metadata, NOTE_METADATA_UNSUPPORTED);
    case BLOCK_SEQUENCE:
        return sequence_line(metadata, line);
    case BLOCK_SCALAR:
        if (metadata->indent == 0)
            metadata->indent = line.content;
        return (line.content >= metadata->indent &&
                metadata_scalar_printable(line.bytes, line.length)) ||
               fail(metadata, NOTE_METADATA_MALFORMED);
    }
    return false;
}

static bool line_bounds(struct note_metadata *_Nonnull metadata, struct markdown_line line)
{
    if (line.next > note_metadata_max_bytes)
        return fail(metadata, NOTE_METADATA_LIMIT);
    return metadata_scalar_printable(line.bytes, line.length) ||
           fail(metadata, NOTE_METADATA_MALFORMED);
}

static bool scalar_header_content(struct note_metadata *_Nonnull metadata,
                                  struct markdown_line line)
{
    bool comment = line.bytes[line.content] == '#';
    if (comment && (line.content == 0 || line.content < metadata->indent))
    {
        metadata->block = BLOCK_NONE;
        return true;
    }
    return line.content == 0 ? mapping(metadata, line) : indented(metadata, line);
}

static bool header_content(struct note_metadata *_Nonnull metadata, struct markdown_line line)
{
    if (metadata->block == BLOCK_SCALAR && line.content != line.length)
        return scalar_header_content(metadata, line);
    if (ignored(line))
        return true;
    return line.content == 0 ? mapping(metadata, line) : indented(metadata, line);
}

static bool read_header(struct note_metadata *_Nonnull metadata, size_t cursor)
{
    size_t length = bounded_length(metadata);
    while (cursor < length)
    {
        struct markdown_line line = markdown_line_read(metadata->source, length, cursor);
        if (!line_bounds(metadata, line))
            return false;
        if (delimiter(line))
        {
            metadata->closing = cursor;
            metadata->body = line.next;
            return true;
        }
        cursor = line.next;
        if (!header_content(metadata, line))
            return false;
    }
    return fail(metadata,
                length > note_metadata_max_bytes ? NOTE_METADATA_LIMIT : NOTE_METADATA_MALFORMED);
}

static void keys_destroy(struct note_metadata *_Nonnull metadata)
{
    free(metadata->keys);
    metadata->keys = nullptr;
}

static bool prepare_header(struct note_metadata *_Nonnull metadata)
{
    metadata->keys = calloc(note_metadata_max_keys, sizeof *metadata->keys);
    metadata->items = calloc(note_tags_max_count, sizeof *metadata->items);
    return metadata->keys != nullptr && metadata->items != nullptr;
}

static bool finish_tags(struct note_metadata *_Nonnull metadata)
{
    const struct note_tag *_Nonnull names[note_tags_max_count];
    for (size_t index = 0; index < metadata->item_count; ++index)
        names[index] = metadata->items[index].name;
    struct note_tags *_Nullable tags = nullptr;
    if (note_tags_create(names, metadata->item_count, &tags) != NOTE_TAGS_READY)
        return false;
    note_tags_destroy(metadata->tags);
    metadata->tags = tags;
    return true;
}

enum note_metadata_outcome note_metadata_create(const char *_Nonnull source, size_t length,
                                                struct note_metadata *_Nullable *_Nonnull out)
{
    struct note_metadata *_Nullable metadata = calloc(1, sizeof *metadata);
    if (metadata == nullptr)
        return NOTE_METADATA_OUT_OF_MEMORY;
    struct note_tags *_Nullable tags = nullptr;
    if (note_tags_empty(&tags) != NOTE_TAGS_READY)
    {
        free(metadata);
        return NOTE_METADATA_OUT_OF_MEMORY;
    }
    metadata->tags = tags;
    metadata->source = source;
    metadata->length = length;
    size_t cursor = 0;
    if (begins(metadata, &cursor))
    {
        metadata->status = NOTE_METADATA_SUPPORTED;
        bool ready = prepare_header(metadata);
        if (ready && read_header(metadata, cursor))
            ready = finish_tags(metadata);
        if (!ready || metadata->allocation_failed)
        {
            note_metadata_destroy(metadata);
            return NOTE_METADATA_OUT_OF_MEMORY;
        }
        keys_destroy(metadata);
    }
    *out = metadata;
    return NOTE_METADATA_READY;
}

enum note_metadata_status note_metadata_status(const struct note_metadata *_Nonnull metadata)
{
    return metadata->status;
}

bool note_metadata_available(const struct note_metadata *_Nonnull metadata)
{
    switch (metadata->status)
    {
    case NOTE_METADATA_NONE:
    case NOTE_METADATA_SUPPORTED:
        return true;
    case NOTE_METADATA_UNSUPPORTED:
    case NOTE_METADATA_MALFORMED:
    case NOTE_METADATA_LIMIT:
    case NOTE_METADATA_INVALID_TAG:
        return false;
    }
    return false;
}

const struct note_tags *_Nonnull note_metadata_tags(const struct note_metadata *_Nonnull metadata)
{
    return metadata->tags;
}

size_t note_metadata_body(const struct note_metadata *_Nonnull metadata)
{
    return metadata->status == NOTE_METADATA_SUPPORTED ? metadata->body : 0;
}

static const char *_Nonnull line_break(const struct note_metadata *_Nonnull metadata)
{
    for (size_t at = 0; at < metadata->length; ++at)
    {
        if (metadata->source[at] == '\n')
            return "\n";
        if (metadata->source[at] == '\r')
            return at + 1 < metadata->length && metadata->source[at + 1] == '\n' ? "\r\n" : "\r";
    }
    return "\n";
}

static void quoted_tag(struct text_buffer *_Nonnull buffer, const struct note_tag *_Nonnull tag)
{
    const char *_Nonnull bytes = note_tag_bytes(tag);
    text_buffer_append_text(buffer, "\"");
    for (size_t at = 0; at < note_tag_length(tag); ++at)
    {
        if (bytes[at] == '"' || bytes[at] == '\\')
            text_buffer_append_text(buffer, "\\");
        text_buffer_append(buffer, bytes + at, 1);
    }
    text_buffer_append_text(buffer, "\"");
}

static void block_added(const struct note_metadata *_Nonnull metadata,
                        const struct note_tags *_Nonnull desired,
                        struct text_buffer *_Nonnull buffer)
{
    const char *_Nonnull newline = line_break(metadata);
    size_t indent = metadata->tag_indent == 0 ? 2 : metadata->tag_indent;
    for (size_t index = 0; index < note_tags_count(desired); ++index)
    {
        const struct note_tag *_Nonnull tag = note_tags_at(desired, index);
        if (note_tags_contains(metadata->tags, tag))
            continue;
        for (size_t column = 0; column < indent; ++column)
            text_buffer_append_text(buffer, " ");
        text_buffer_append_text(buffer, "- ");
        quoted_tag(buffer, tag);
        text_buffer_append_text(buffer, newline);
    }
}

static void block_changed(const struct note_metadata *_Nonnull metadata,
                          const struct note_tags *_Nonnull desired,
                          struct text_buffer *_Nonnull buffer)
{
    size_t at = metadata->tag_start;
    for (size_t index = 0; index < metadata->item_count; ++index)
    {
        size_t start = metadata->items[index].start;
        text_buffer_append(buffer, metadata->source + at, start - at);
        at = metadata->items[index].end;
        if (note_tags_contains(desired, metadata->items[index].name))
            text_buffer_append(buffer, metadata->source + start, at - start);
    }
    text_buffer_append(buffer, metadata->source + at, metadata->tag_end - at);
    block_added(metadata, desired, buffer);
}

static void flow_changed(const struct note_metadata *_Nonnull metadata,
                         const struct note_tags *_Nonnull desired,
                         struct text_buffer *_Nonnull buffer)
{
    bool first = true;
    text_buffer_append_text(buffer, "[");
    for (size_t index = 0; index < metadata->item_count; ++index)
    {
        if (!note_tags_contains(desired, metadata->items[index].name))
            continue;
        text_buffer_append_text(buffer, first ? "" : ", ");
        size_t start = metadata->items[index].token;
        text_buffer_append(buffer, metadata->source + start,
                           metadata->items[index].token_end - start);
        first = false;
    }
    for (size_t index = 0; index < note_tags_count(desired); ++index)
    {
        const struct note_tag *_Nonnull tag = note_tags_at(desired, index);
        if (note_tags_contains(metadata->tags, tag))
            continue;
        text_buffer_append_text(buffer, first ? "" : ", ");
        quoted_tag(buffer, tag);
        first = false;
    }
    text_buffer_append_text(buffer, "]");
}

static void inserted(const struct note_metadata *_Nonnull metadata,
                     const struct note_tags *_Nonnull desired, struct text_buffer *_Nonnull buffer)
{
    const char *_Nonnull newline = line_break(metadata);
    if (metadata->status == NOTE_METADATA_NONE)
    {
        text_buffer_append_text(buffer, "---");
        text_buffer_append_text(buffer, newline);
    }
    text_buffer_append_text(buffer, "tags:");
    text_buffer_append_text(buffer, newline);
    block_added(metadata, desired, buffer);
    if (metadata->status == NOTE_METADATA_NONE)
    {
        text_buffer_append_text(buffer, "---");
        text_buffer_append_text(buffer, newline);
    }
}

static struct note_byte_span edit_range(const struct note_metadata *_Nonnull metadata)
{
    if (metadata->status == NOTE_METADATA_NONE)
        return (struct note_byte_span){0, 0};
    switch (metadata->style)
    {
    case TAGS_ABSENT:
        return (struct note_byte_span){metadata->closing, metadata->closing};
    case TAGS_BLOCK:
        return (struct note_byte_span){metadata->tag_start, metadata->tag_end};
    case TAGS_FLOW:
        return (struct note_byte_span){metadata->tag_value, metadata->tag_end};
    }
    return (struct note_byte_span){0, 0};
}

static enum note_tag_edit_outcome rewrite(const struct note_metadata *_Nonnull metadata,
                                          const struct text_buffer *_Nonnull patch,
                                          const struct note_tags *_Nonnull desired,
                                          struct note_text *_Nullable *_Nonnull out)
{
    struct note_byte_span range = edit_range(metadata);
    size_t kept = metadata->length - (range.end - range.start);
    size_t added = text_buffer_length(patch);
    if (kept > persisted_size_limit || added > persisted_size_limit - kept)
        return NOTE_TAG_EDIT_LIMIT;
    struct text_buffer *_Nullable candidate_text = nullptr;
    if (text_buffer_create(&candidate_text) != TEXT_BUFFER_ACCEPTED)
        return NOTE_TAG_EDIT_OUT_OF_MEMORY;
    text_buffer_append(candidate_text, metadata->source, range.start);
    text_buffer_append(candidate_text, text_buffer_bytes(patch), added);
    text_buffer_append(candidate_text, metadata->source + range.end, metadata->length - range.end);
    struct note_text *_Nullable body = nullptr;
    enum note_tag_edit_outcome outcome = NOTE_TAG_EDIT_OUT_OF_MEMORY;
    if (text_buffer_finish(candidate_text) == TEXT_BUFFER_ACCEPTED &&
        note_text_create(text_buffer_bytes(candidate_text), text_buffer_length(candidate_text),
                         &body) == NOTE_TEXT_ACCEPTED)
    {
        /* Every changed candidate starts with the metadata opener, not a body FEFF. */
        const struct note_metadata *_Nonnull parsed = note_text_metadata(body);
        outcome =
            parsed->status == NOTE_METADATA_SUPPORTED && note_tags_equal(parsed->tags, desired)
                ? NOTE_TAG_EDIT_READY
                : NOTE_TAG_EDIT_LIMIT;
    }
    text_buffer_destroy(candidate_text);
    if (outcome != NOTE_TAG_EDIT_READY)
    {
        note_text_destroy(body);
        return outcome;
    }
    *out = body;
    return outcome;
}

/* Source is already UTF-8. Native RichEdit counts each CRLF as one paragraph unit. */
static uint32_t native_point(const char *_Nonnull bytes, size_t length, size_t *_Nonnull at)
{
    uint32_t point = 0;
    *at += utf8_text_decode(bytes + *at, length - *at, &point);
    if (point == '\r' && *at < length && bytes[*at] == '\n')
        ++*at;
    return point == '\n' ? '\r' : point;
}

static size_t native_length(const char *_Nonnull bytes, size_t length)
{
    size_t count = 0;
    for (size_t at = 0; at < length;)
        count += native_point(bytes, length, &at) > 0xffff ? 2 : 1;
    return count;
}

static void native_append(struct replace_edit *_Nonnull edit, const char *_Nonnull bytes,
                          size_t length)
{
    for (size_t at = 0; at < length;)
    {
        uint32_t point = native_point(bytes, length, &at);
        char16_t units[2] = {(char16_t)point, 0};
        size_t count = 1;
        if (point > 0xffff)
        {
            units[0] = (char16_t)(0xd800 + ((point - 0x10000) >> 10));
            units[1] = (char16_t)(0xdc00 + ((point - 0x10000) & 0x3ff));
            count = 2;
        }
        replace_edit_append(edit, units, count);
    }
}

static enum note_tag_edit_outcome native_edit(const struct note_metadata *_Nonnull metadata,
                                              struct note_byte_span range,
                                              const struct text_buffer *_Nonnull patch,
                                              struct replace_edit *_Nullable *_Nonnull out)
{
    struct note_search_span span = {native_length(metadata->source, range.start),
                                    native_length(metadata->source, range.end)};
    const char *_Nonnull bytes = text_buffer_bytes(patch);
    size_t length = text_buffer_length(patch);
    struct replace_edit *_Nullable edit = nullptr;
    if (replace_edit_create(span, native_length(bytes, length), &edit) != REPLACE_EDIT_READY)
        return NOTE_TAG_EDIT_OUT_OF_MEMORY;
    native_append(edit, bytes, length);
    *out = edit;
    return NOTE_TAG_EDIT_READY;
}

static enum note_tag_edit_outcome prepare_patch(const struct note_metadata *_Nonnull metadata,
                                                const struct note_tags *_Nonnull desired,
                                                struct text_buffer *_Nullable *_Nonnull out)
{
    if (metadata->status != NOTE_METADATA_NONE && metadata->status != NOTE_METADATA_SUPPORTED)
        return NOTE_TAG_EDIT_UNAVAILABLE;
    if (note_tags_equal(metadata->tags, desired))
        return NOTE_TAG_EDIT_UNCHANGED;
    struct text_buffer *_Nullable patch = nullptr;
    if (text_buffer_create(&patch) != TEXT_BUFFER_ACCEPTED)
        return NOTE_TAG_EDIT_OUT_OF_MEMORY;
    switch (metadata->style)
    {
    case TAGS_ABSENT:
        inserted(metadata, desired, patch);
        break;
    case TAGS_BLOCK:
        block_changed(metadata, desired, patch);
        break;
    case TAGS_FLOW:
        flow_changed(metadata, desired, patch);
        break;
    }
    if (text_buffer_finish(patch) != TEXT_BUFFER_ACCEPTED)
    {
        text_buffer_destroy(patch);
        return NOTE_TAG_EDIT_OUT_OF_MEMORY;
    }
    *out = patch;
    return NOTE_TAG_EDIT_READY;
}

enum note_tag_edit_outcome note_metadata_edit(const struct note_metadata *_Nonnull metadata,
                                              const struct note_tags *_Nonnull desired,
                                              struct replace_edit *_Nullable *_Nonnull out)
{
    struct text_buffer *_Nullable patch = nullptr;
    enum note_tag_edit_outcome outcome = prepare_patch(metadata, desired, &patch);
    if (outcome != NOTE_TAG_EDIT_READY)
        return outcome;
    struct note_text *_Nullable body = nullptr;
    outcome = rewrite(metadata, patch, desired, &body);
    if (outcome == NOTE_TAG_EDIT_READY)
        outcome = native_edit(metadata, edit_range(metadata), patch, out);
    note_text_destroy(body);
    text_buffer_destroy(patch);
    return outcome;
}

enum note_tag_edit_outcome note_metadata_rewrite(const struct note_metadata *_Nonnull metadata,
                                                 const struct note_tags *_Nonnull desired,
                                                 struct note_text *_Nullable *_Nonnull out)
{
    struct text_buffer *_Nullable patch = nullptr;
    enum note_tag_edit_outcome outcome = prepare_patch(metadata, desired, &patch);
    if (outcome != NOTE_TAG_EDIT_READY)
        return outcome;
    outcome = rewrite(metadata, patch, desired, out);
    text_buffer_destroy(patch);
    return outcome;
}

void note_metadata_destroy(struct note_metadata *_Nullable metadata)
{
    if (metadata == nullptr)
        return;
    for (size_t index = 0; index < metadata->item_count; ++index)
        note_tag_destroy(metadata->items[index].name);
    free(metadata->items);
    keys_destroy(metadata);
    note_tags_destroy(metadata->tags);
    free(metadata);
}
