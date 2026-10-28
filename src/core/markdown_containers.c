#include "markdown_containers.h"
#include "markdown_container_kind.h"
#include "markdown_item_state.h"
#include "markdown_table.h"
#include <stdint.h>
#include <stdlib.h>

struct markdown_containers
{
    struct
    {
        enum markdown_container_kind kind;
        size_t width;
        size_t number;
        size_t display;
        size_t quotes;
        size_t lists;
        size_t barriers;
        char delimiter;
        bool ordered;
        bool pending;
        enum markdown_item_state state;
    } *_Nullable frames;
    size_t count;
    size_t capacity;
    size_t pending_count;
    size_t pending_cursor;
    struct markdown_line line;
    enum markdown_container_event event;
};

enum markdown_rtf_outcome
markdown_containers_create(struct markdown_containers *_Nullable *_Nonnull out)
{
    struct markdown_containers *containers = calloc(1, sizeof *containers);
    if (containers == nullptr)
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    containers->line.bytes = "";
    *out = containers;
    return MARKDOWN_RTF_CONVERTED;
}

void markdown_containers_destroy(struct markdown_containers *_Nullable containers)
{
    if (containers == nullptr)
        return;
    free(containers->frames);
    free(containers);
}

static bool reserve(struct markdown_containers *_Nonnull containers)
{
    if (containers->count < containers->capacity)
        return true;
    size_t capacity = containers->capacity == 0 ? 8 : containers->capacity * 2;
    if (capacity < containers->capacity || capacity > SIZE_MAX / sizeof *containers->frames)
        return false;
    typeof(containers->frames) grown = realloc(containers->frames, capacity * sizeof *grown);
    if (grown == nullptr)
        return false;
    containers->frames = grown;
    containers->capacity = capacity;
    return true;
}

size_t markdown_containers_indent(const struct markdown_containers *_Nonnull containers)
{
    return containers->count == 0 ? 0 : containers->frames[containers->count - 1].display;
}

static bool item_continues(enum markdown_item_state state)
{
    switch (state)
    {
    case MARKDOWN_ITEM_STATE_EMPTY:
    case MARKDOWN_ITEM_STATE_CONTENT:
        return true;
    case MARKDOWN_ITEM_STATE_CLOSED:
        return false;
    }
    return false;
}

static bool frame_matches(const struct markdown_containers *_Nonnull containers, size_t index,
                          struct markdown_line *_Nonnull line)
{
    switch (containers->frames[index].kind)
    {
    case MARKDOWN_CONTAINER_KIND_QUOTE:
        return markdown_line_quote(line);
    case MARKDOWN_CONTAINER_KIND_LIST_ITEM:
        return item_continues(containers->frames[index].state) &&
               (markdown_line_blank(*line) ||
                markdown_line_skip(line, containers->frames[index].width));
    }
    return false;
}

static size_t blank_prefix(const struct markdown_containers *_Nonnull containers, size_t matched,
                           size_t limit)
{
    size_t barriers = matched == 0 ? 0 : containers->frames[matched - 1].barriers;
    size_t low = matched;
    size_t high = limit;
    while (low < high)
    {
        size_t middle = low + (high - low) / 2;
        if (containers->frames[middle].barriers == barriers)
            low = middle + 1;
        else
            high = middle;
    }
    return low;
}

static size_t match_prefix(const struct markdown_containers *_Nonnull containers,
                           struct markdown_line *_Nonnull line, size_t limit)
{
    size_t matched = 0;
    while (matched < limit)
    {
        if (markdown_line_blank(*line))
            return blank_prefix(containers, matched, limit);
        if (!frame_matches(containers, matched, line))
            break;
        ++matched;
    }
    return matched;
}

enum markdown_container_match
markdown_containers_match(const struct markdown_containers *_Nonnull containers,
                          struct markdown_line *_Nonnull line)
{
    if (!markdown_line_valid(*line))
        return MARKDOWN_CONTAINER_MATCH_INVALID_INPUT;
    struct markdown_line matched = *line;
    if (match_prefix(containers, &matched, containers->count) != containers->count)
        return MARKDOWN_CONTAINER_MATCH_LEFT;
    *line = matched;
    return MARKDOWN_CONTAINER_MATCH_MATCHED;
}

static bool sibling(const struct markdown_containers *_Nonnull containers, size_t matched,
                    struct markdown_line line)
{
    if (matched == containers->count || line.marker == 0 || markdown_line_thematic(line))
        return false;
    return containers->frames[matched].kind == MARKDOWN_CONTAINER_KIND_LIST_ITEM &&
           line.indent < containers->frames[matched].width;
}

static bool list_starts(struct markdown_line line, enum markdown_paragraph_state paragraph)
{
    if (line.marker == 0 || markdown_line_thematic(line))
        return false;
    if (paragraph == MARKDOWN_PARAGRAPH_STATE_NONE)
        return true;
    if (!markdown_line_enter_list(&line) || markdown_line_blank(line))
        return false;
    return !line.ordered || line.number == 1;
}

static bool table_starts(const struct markdown_containers *_Nonnull containers,
                         struct markdown_line line, struct markdown_line next, size_t matched)
{
    if (match_prefix(containers, &next, matched) != matched)
        return false;
    return markdown_table_starts(line, next);
}

static bool leaf_starts(struct markdown_line line)
{
    return markdown_line_heading(line) != 0 || markdown_line_fence(line) != 0 ||
           markdown_line_thematic(line) || markdown_line_setext(line) != 0;
}

static bool interrupts(const struct markdown_containers *_Nonnull containers,
                       struct markdown_line line, struct markdown_line next, size_t matched)
{
    if (markdown_line_blank(line) || leaf_starts(line) || sibling(containers, matched, line))
        return true;
    struct markdown_line quote = line;
    return markdown_line_quote(&quote) || list_starts(line, MARKDOWN_PARAGRAPH_STATE_OPEN) ||
           table_starts(containers, line, next, matched);
}

static void item_has_content(struct markdown_containers *_Nonnull containers)
{
    if (containers->count == 0)
        return;
    size_t index = containers->count - 1;
    if (containers->frames[index].kind != MARKDOWN_CONTAINER_KIND_LIST_ITEM)
        return;
    switch (containers->frames[index].state)
    {
    case MARKDOWN_ITEM_STATE_EMPTY:
        containers->frames[index].state = MARKDOWN_ITEM_STATE_CONTENT;
        --containers->frames[index].barriers;
        return;
    case MARKDOWN_ITEM_STATE_CONTENT:
    case MARKDOWN_ITEM_STATE_CLOSED:
        return;
    }
}

static bool item_content(struct markdown_line line)
{
    return markdown_line_enter_list(&line) && !markdown_line_blank(line);
}

static enum markdown_rtf_outcome push(struct markdown_containers *_Nonnull containers,
                                      enum markdown_container_kind kind, struct markdown_line line)
{
    item_has_content(containers);
    size_t step = 0;
    size_t quotes = markdown_containers_quote_depth(containers);
    size_t lists = containers->count == 0 ? 0 : containers->frames[containers->count - 1].lists;
    bool pending = false;
    bool content = false;
    size_t barriers =
        containers->count == 0 ? 0 : containers->frames[containers->count - 1].barriers;
    switch (kind)
    {
    case MARKDOWN_CONTAINER_KIND_QUOTE:
        step = 480;
        ++quotes;
        ++barriers;
        break;
    case MARKDOWN_CONTAINER_KIND_LIST_ITEM:
        step = 440;
        ++lists;
        pending = true;
        content = item_content(line);
        barriers += (size_t)!content;
        break;
    }
    size_t display = markdown_containers_indent(containers);
    if (step == 0 || display > SIZE_MAX - step || line.indent > SIZE_MAX - line.marker_columns)
        return MARKDOWN_RTF_INVALID_INPUT;
    if (!reserve(containers))
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    if (pending)
    {
        if (containers->pending_count == 0)
            containers->pending_cursor = containers->count;
        ++containers->pending_count;
    }
    containers->frames[containers->count++] = (typeof(*containers->frames)){
        .kind = kind,
        .width = line.indent + line.marker_columns,
        .number = line.number,
        .display = display + step,
        .quotes = quotes,
        .lists = lists,
        .barriers = barriers,
        .delimiter = line.delimiter,
        .ordered = line.ordered,
        .pending = pending,
        .state = content ? MARKDOWN_ITEM_STATE_CONTENT : MARKDOWN_ITEM_STATE_EMPTY};
    return MARKDOWN_RTF_CONVERTED;
}

static void trim(struct markdown_containers *_Nonnull containers, size_t count)
{
    while (containers->count > count)
    {
        --containers->count;
        if (containers->frames[containers->count].pending)
            --containers->pending_count;
    }
    if (containers->pending_cursor > count)
        containers->pending_cursor = count;
}

static enum markdown_rtf_outcome next_sibling(struct markdown_containers *_Nonnull containers,
                                              size_t matched, struct markdown_line *_Nonnull line)
{
    if (containers->frames[matched].ordered != line->ordered ||
        containers->frames[matched].delimiter != line->delimiter)
    {
        trim(containers, matched);
        return MARKDOWN_RTF_CONVERTED;
    }
    if (containers->frames[matched].number == SIZE_MAX ||
        line->indent > SIZE_MAX - line->marker_columns)
        return MARKDOWN_RTF_INVALID_INPUT;
    trim(containers, matched + 1);
    ++containers->frames[matched].number;
    containers->frames[matched].width = line->indent + line->marker_columns;
    bool content = item_content(*line);
    containers->frames[matched].state =
        content ? MARKDOWN_ITEM_STATE_CONTENT : MARKDOWN_ITEM_STATE_EMPTY;
    size_t barriers = matched == 0 ? 0 : containers->frames[matched - 1].barriers;
    containers->frames[matched].barriers = barriers + (size_t)!content;
    if (!containers->frames[matched].pending)
    {
        if (containers->pending_count == 0)
            containers->pending_cursor = matched;
        ++containers->pending_count;
        containers->frames[matched].pending = true;
    }
    return markdown_line_enter_list(line) ? MARKDOWN_RTF_CONVERTED : MARKDOWN_RTF_INVALID_INPUT;
}

static enum markdown_rtf_outcome open_list(struct markdown_containers *_Nonnull containers,
                                           struct markdown_line *_Nonnull line)
{
    enum markdown_rtf_outcome result = push(containers, MARKDOWN_CONTAINER_KIND_LIST_ITEM, *line);
    if (result != MARKDOWN_RTF_CONVERTED)
        return result;
    return markdown_line_enter_list(line) ? MARKDOWN_RTF_CONVERTED : MARKDOWN_RTF_INVALID_INPUT;
}

static enum markdown_rtf_outcome open_containers(struct markdown_containers *_Nonnull containers,
                                                 struct markdown_line *_Nonnull line,
                                                 enum markdown_paragraph_state paragraph)
{
    while (!markdown_line_blank(*line))
    {
        struct markdown_line entered = *line;
        if (markdown_line_quote(&entered))
        {
            enum markdown_rtf_outcome result =
                push(containers, MARKDOWN_CONTAINER_KIND_QUOTE, *line);
            if (result != MARKDOWN_RTF_CONVERTED)
                return result;
            *line = entered;
            paragraph = MARKDOWN_PARAGRAPH_STATE_NONE;
            continue;
        }
        markdown_line_list(line);
        if (!list_starts(*line, paragraph))
            break;
        enum markdown_rtf_outcome result = open_list(containers, line);
        if (result != MARKDOWN_RTF_CONVERTED)
            return result;
        paragraph = MARKDOWN_PARAGRAPH_STATE_NONE;
    }
    line->marker = line->marker_columns = 0;
    return MARKDOWN_RTF_CONVERTED;
}

static bool paragraph_valid(enum markdown_paragraph_state paragraph)
{
    switch (paragraph)
    {
    case MARKDOWN_PARAGRAPH_STATE_NONE:
    case MARKDOWN_PARAGRAPH_STATE_OPEN:
        return true;
    }
    return false;
}

static bool opens_container(struct markdown_line line, enum markdown_paragraph_state paragraph)
{
    struct markdown_line quote = line;
    return markdown_line_quote(&quote) || list_starts(line, paragraph);
}

static bool lazy_continuation(const struct markdown_containers *_Nonnull containers,
                              struct markdown_line line, struct markdown_line next, size_t matched)
{
    return matched < containers->count && !interrupts(containers, line, next, matched);
}

static bool closes_empty_item(const struct markdown_containers *_Nonnull containers,
                              struct markdown_line line, size_t matched)
{
    return matched < containers->count && markdown_line_blank(line) &&
           containers->frames[matched].kind == MARKDOWN_CONTAINER_KIND_LIST_ITEM &&
           containers->frames[matched].state != MARKDOWN_ITEM_STATE_CONTENT;
}

static enum markdown_rtf_outcome commit_path(struct markdown_containers *_Nonnull containers,
                                             struct markdown_line *_Nonnull line, size_t matched,
                                             enum markdown_paragraph_state paragraph)
{
    if (closes_empty_item(containers, *line, matched))
    {
        trim(containers, matched + 1);
        containers->frames[matched].state = MARKDOWN_ITEM_STATE_CLOSED;
        return MARKDOWN_RTF_CONVERTED;
    }
    if (sibling(containers, matched, *line))
    {
        enum markdown_rtf_outcome result = next_sibling(containers, matched, line);
        if (result != MARKDOWN_RTF_CONVERTED)
            return result;
    }
    else
        trim(containers, matched);
    return open_containers(containers, line, paragraph);
}

static void publish_line(struct markdown_containers *_Nonnull containers, struct markdown_line line,
                         enum markdown_container_event event)
{
    line.marker = line.marker_columns = 0;
    if (!markdown_line_blank(line))
        item_has_content(containers);
    containers->line = line;
    containers->event = markdown_line_blank(line) ? MARKDOWN_CONTAINER_EVENT_BLANK : event;
}

enum markdown_rtf_outcome
markdown_containers_accept(struct markdown_containers *_Nonnull containers,
                           struct markdown_line raw, struct markdown_line next,
                           enum markdown_paragraph_state paragraph)
{
    if (!markdown_line_valid(raw) || !markdown_line_valid(next) || !paragraph_valid(paragraph))
        return MARKDOWN_RTF_INVALID_INPUT;
    struct markdown_line line = raw;
    size_t matched = match_prefix(containers, &line, containers->count);
    markdown_line_list(&line);
    if (paragraph == MARKDOWN_PARAGRAPH_STATE_OPEN &&
        lazy_continuation(containers, line, next, matched))
    {
        publish_line(containers, line, MARKDOWN_CONTAINER_EVENT_CONTINUATION);
        return MARKDOWN_RTF_CONVERTED;
    }
    bool changing = matched < containers->count || opens_container(line, paragraph);
    if (changing && paragraph == MARKDOWN_PARAGRAPH_STATE_OPEN)
    {
        containers->event = MARKDOWN_CONTAINER_EVENT_FLUSH_REQUIRED;
        return MARKDOWN_RTF_CONVERTED;
    }
    enum markdown_rtf_outcome result = commit_path(containers, &line, matched, paragraph);
    if (result != MARKDOWN_RTF_CONVERTED)
        return result;
    publish_line(containers, line,
                 changing ? MARKDOWN_CONTAINER_EVENT_BOUNDARY
                          : MARKDOWN_CONTAINER_EVENT_CONTINUATION);
    return MARKDOWN_RTF_CONVERTED;
}

struct markdown_line markdown_containers_line(const struct markdown_containers *_Nonnull containers)
{
    return containers->line;
}

enum markdown_container_event
markdown_containers_event(const struct markdown_containers *_Nonnull containers)
{
    return containers->event;
}

size_t markdown_containers_quote_depth(const struct markdown_containers *_Nonnull containers)
{
    return containers->count == 0 ? 0 : containers->frames[containers->count - 1].quotes;
}

bool markdown_containers_in_list(const struct markdown_containers *_Nonnull containers)
{
    return containers->count != 0 && containers->frames[containers->count - 1].lists != 0;
}

bool markdown_containers_task_start(const struct markdown_containers *_Nonnull containers)
{
    if (containers->count == 0)
        return false;
    size_t index = containers->count - 1;
    switch (containers->frames[index].kind)
    {
    case MARKDOWN_CONTAINER_KIND_QUOTE:
        return false;
    case MARKDOWN_CONTAINER_KIND_LIST_ITEM:
        return containers->frames[index].pending;
    }
    return false;
}

size_t markdown_containers_pending(const struct markdown_containers *_Nonnull containers)
{
    return containers->pending_count;
}

static size_t pending_index(const struct markdown_containers *_Nonnull containers)
{
    return containers->pending_count == 0 ? containers->count : containers->pending_cursor;
}

size_t markdown_containers_marker_indent(const struct markdown_containers *_Nonnull containers)
{
    size_t index = pending_index(containers);
    return index == containers->count ? 0 : containers->frames[index].display;
}

size_t markdown_containers_marker_number(const struct markdown_containers *_Nonnull containers)
{
    size_t index = pending_index(containers);
    return index == containers->count ? 0 : containers->frames[index].number;
}

char markdown_containers_marker_delimiter(const struct markdown_containers *_Nonnull containers)
{
    size_t index = pending_index(containers);
    return index == containers->count ? '.' : containers->frames[index].delimiter;
}

bool markdown_containers_marker_ordered(const struct markdown_containers *_Nonnull containers)
{
    size_t index = pending_index(containers);
    return index < containers->count && containers->frames[index].ordered;
}

void markdown_containers_consume_marker(struct markdown_containers *_Nonnull containers)
{
    size_t index = pending_index(containers);
    if (index < containers->count)
    {
        containers->frames[index].pending = false;
        --containers->pending_count;
        ++containers->pending_cursor;
        while (containers->pending_cursor < containers->count &&
               !containers->frames[containers->pending_cursor].pending)
            ++containers->pending_cursor;
    }
}
