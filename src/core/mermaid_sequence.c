#include "mermaid_sequence.h"
#include "diagram_label.h"
#include "diagram_statement.h"
#include "text_buffer.h"
#include <string.h>
struct mermaid_sequence
{
    struct diagram_model *_Nonnull model;
    enum diagram_outcome outcome;
    enum diagram_frame_kind frames[diagram_depth_limit];
    size_t depth;
    size_t active[diagram_node_limit];
    size_t empty;
    size_t next_number;
    size_t number_step;
    bool numbered;
};
static bool reject(struct mermaid_sequence *_Nonnull parser, enum diagram_outcome outcome)
{
    parser->outcome = outcome;
    return false;
}
static bool actor(struct mermaid_sequence *_Nonnull parser, struct diagram_text id,
                  size_t *_Nonnull index)
{
    if (id.length == 0)
        return reject(parser, DIAGRAM_INVALID);
    *index = diagram_model_find_node(parser->model, id.bytes, id.length);
    if (*index != diagram_none)
        return true;
    struct diagram_node node = {.shape = DIAGRAM_RECTANGLE, .group = diagram_none};
    parser->outcome = diagram_model_text(parser->model, id.bytes, id.length, &node.id);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    node.label = node.id;
    *index = diagram_model_node_count(parser->model);
    parser->outcome = diagram_model_node(parser->model, *index, node);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static bool participant(struct mermaid_sequence *_Nonnull parser, struct diagram_text text,
                        enum diagram_shape shape)
{
    struct diagram_text id = diagram_text_identifier(&text);
    size_t index = 0;
    if (!actor(parser, id, &index))
        return false;
    struct diagram_node node = diagram_model_node_at(parser->model, index);
    node.shape = shape;
    if (text.length != 0)
    {
        struct diagram_text alias = diagram_text_word(&text);
        if (!diagram_text_equal(alias, "as") || text.length == 0)
            return reject(parser, DIAGRAM_UNSUPPORTED);
        parser->outcome = diagram_label_store(parser->model, text, &node.label);
    }
    if (parser->outcome == DIAGRAM_ACCEPTED)
        parser->outcome = diagram_model_node(parser->model, index, node);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static bool add_event(struct mermaid_sequence *_Nonnull parser, struct diagram_event event)
{
    parser->outcome = diagram_model_event(parser->model, event);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static bool activation(struct mermaid_sequence *_Nonnull parser, size_t index,
                       enum diagram_event_kind kind)
{
    if (index >= diagram_model_node_count(parser->model))
        return reject(parser, DIAGRAM_INVALID);
    if (kind == DIAGRAM_ACTIVATE)
    {
        if (parser->active[index] == diagram_depth_limit)
            return reject(parser, DIAGRAM_TOO_LARGE);
        ++parser->active[index];
    }
    if (kind == DIAGRAM_DEACTIVATE)
    {
        if (parser->active[index] == 0)
            return reject(parser, DIAGRAM_INVALID);
        --parser->active[index];
    }
    struct diagram_event event = {
        .kind = kind, .link = {.from = index, .to = index, .label = parser->empty, .length = 1}};
    return add_event(parser, event);
}
static void append_number(struct text_buffer *_Nonnull buffer, size_t value)
{
    char digits[24];
    size_t count = 0;
    do
    {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    while (count != 0)
        text_buffer_append(buffer, &digits[--count], 1);
}
static bool message_label(struct mermaid_sequence *_Nonnull parser, struct diagram_text text,
                          size_t *_Nonnull label)
{
    parser->outcome = diagram_label_store(parser->model, text, label);
    if (parser->outcome != DIAGRAM_ACCEPTED || !parser->numbered)
        return parser->outcome == DIAGRAM_ACCEPTED;
    if (parser->next_number > 1000000)
        return reject(parser, DIAGRAM_TOO_LARGE);
    struct text_buffer *_Nullable buffer = nullptr;
    if (text_buffer_create(&buffer) != TEXT_BUFFER_ACCEPTED)
        return reject(parser, DIAGRAM_OUT_OF_MEMORY);
    append_number(buffer, parser->next_number);
    text_buffer_append_text(buffer, ". ");
    text_buffer_append_text(buffer, diagram_model_text_at(parser->model, *label));
    parser->outcome = text_buffer_finish(buffer) == TEXT_BUFFER_ACCEPTED
                          ? diagram_model_text(parser->model, text_buffer_bytes(buffer),
                                               text_buffer_length(buffer), label)
                          : DIAGRAM_OUT_OF_MEMORY;
    text_buffer_destroy(buffer);
    parser->next_number += parser->number_step;
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static enum diagram_arrow signal_arrow(const char *_Nonnull token, size_t length)
{
    if (token[length - 1] == ')')
        return DIAGRAM_ARROW_OPEN;
    if (token[length - 1] == 'x')
        return DIAGRAM_ARROW_CROSS;
    if (length >= 2 && token[length - 1] == '>' && token[length - 2] == '>')
        return DIAGRAM_ARROW_CLOSED;
    return DIAGRAM_ARROW_NONE;
}
static bool signal(struct mermaid_sequence *_Nonnull parser, struct diagram_text *_Nonnull text,
                   struct diagram_link *_Nonnull link)
{
    static const char *const tokens[] = {"<<-->>", "<<->>", "-->>", "->>", "--x",
                                         "-x",     "--)",   "-)",   "-->", "->"};
    size_t selected = diagram_none;
    for (size_t index = 0; index < sizeof tokens / sizeof tokens[0]; ++index)
        if (diagram_text_starts(*text, tokens[index]))
        {
            selected = index;
            break;
        }
    if (selected == diagram_none)
        return reject(parser, DIAGRAM_UNSUPPORTED);
    const char *token = tokens[selected];
    size_t stroke_at = token[0] == '<' ? 2 : 0;
    link->stroke = token[stroke_at + 1] == '-' ? DIAGRAM_DASHED : DIAGRAM_SOLID;
    link->start = token[0] == '<' ? DIAGRAM_ARROW_CLOSED : DIAGRAM_ARROW_NONE;
    size_t length = strlen(token);
    link->end = signal_arrow(token, length);
    link->length = 1;
    *text = diagram_text_trim(diagram_text_skip(*text, length));
    return true;
}
static char activation_prefix(struct diagram_text *_Nonnull text)
{
    if (text->length == 0 || (text->bytes[0] != '+' && text->bytes[0] != '-'))
        return '\0';
    char change = text->bytes[0];
    *text = diagram_text_trim(diagram_text_skip(*text, 1));
    return change;
}
static bool message(struct mermaid_sequence *_Nonnull parser, struct diagram_text text)
{
    struct diagram_event event = {.kind = DIAGRAM_MESSAGE};
    struct diagram_text from = diagram_text_identifier(&text);
    if (!actor(parser, from, &event.link.from) || !signal(parser, &text, &event.link))
        return false;
    char change = activation_prefix(&text);
    struct diagram_text to = diagram_text_identifier(&text);
    if (!actor(parser, to, &event.link.to))
        return false;
    if (text.length == 0 || text.bytes[0] != ':')
        return reject(parser, DIAGRAM_INVALID);
    if (!message_label(parser, diagram_text_skip(text, 1), &event.link.label) ||
        !add_event(parser, event))
        return false;
    if (change == '+')
        return activation(parser, event.link.to, DIAGRAM_ACTIVATE);
    if (change == '-')
        return activation(parser, event.link.from, DIAGRAM_DEACTIVATE);
    return true;
}
static bool note_position(struct mermaid_sequence *_Nonnull parser,
                          struct diagram_text *_Nonnull text, enum diagram_note_side *_Nonnull side)
{
    struct diagram_text word = diagram_text_word(text);
    if (diagram_text_equal(word, "over"))
    {
        *side = DIAGRAM_NOTE_OVER;
        return true;
    }
    if (!diagram_text_equal(word, "left") && !diagram_text_equal(word, "right"))
        return reject(parser, DIAGRAM_INVALID);
    *side = diagram_text_equal(word, "left") ? DIAGRAM_NOTE_LEFT : DIAGRAM_NOTE_RIGHT;
    return diagram_text_equal(diagram_text_word(text), "of") || reject(parser, DIAGRAM_INVALID);
}
static bool note_actors(struct mermaid_sequence *_Nonnull parser,
                        struct diagram_text *_Nonnull text, struct diagram_event *_Nonnull event)
{
    struct diagram_text first = diagram_text_identifier(text);
    if (!actor(parser, first, &event->link.from))
        return false;
    event->link.to = event->link.from;
    if (text->length == 0 || text->bytes[0] != ',')
        return true;
    if (event->side != DIAGRAM_NOTE_OVER)
        return reject(parser, DIAGRAM_INVALID);
    *text = diagram_text_skip(*text, 1);
    struct diagram_text second = diagram_text_identifier(text);
    return actor(parser, second, &event->link.to);
}
static bool note(struct mermaid_sequence *_Nonnull parser, struct diagram_text text)
{
    struct diagram_event event = {.kind = DIAGRAM_NOTE};
    if (!note_position(parser, &text, &event.side) || !note_actors(parser, &text, &event))
        return false;
    if (text.length == 0 || text.bytes[0] != ':')
        return reject(parser, DIAGRAM_INVALID);
    parser->outcome =
        diagram_label_store(parser->model, diagram_text_skip(text, 1), &event.link.label);
    return parser->outcome == DIAGRAM_ACCEPTED && add_event(parser, event);
}
static bool frame_kind(struct diagram_text word, enum diagram_frame_kind *_Nonnull kind)
{
    if (diagram_text_equal(word, "loop"))
        *kind = DIAGRAM_FRAME_LOOP;
    else if (diagram_text_equal(word, "alt"))
        *kind = DIAGRAM_FRAME_ALT;
    else if (diagram_text_equal(word, "opt"))
        *kind = DIAGRAM_FRAME_OPT;
    else if (diagram_text_equal(word, "par"))
        *kind = DIAGRAM_FRAME_PAR;
    else if (diagram_text_equal(word, "critical"))
        *kind = DIAGRAM_FRAME_CRITICAL;
    else if (diagram_text_equal(word, "break"))
        *kind = DIAGRAM_FRAME_BREAK;
    else
        return false;
    return true;
}
static bool frame_label(struct mermaid_sequence *_Nonnull parser, struct diagram_text word,
                        struct diagram_text text, size_t *_Nonnull label)
{
    struct text_buffer *_Nullable buffer = nullptr;
    if (text_buffer_create(&buffer) != TEXT_BUFFER_ACCEPTED)
        return reject(parser, DIAGRAM_OUT_OF_MEMORY);
    text_buffer_append(buffer, word.bytes, word.length);
    if (text.length != 0)
    {
        text_buffer_append_text(buffer, "  [");
        text_buffer_append(buffer, text.bytes, text.length);
        text_buffer_append_text(buffer, "]");
    }
    parser->outcome = text_buffer_finish(buffer) == TEXT_BUFFER_ACCEPTED
                          ? diagram_label_store(parser->model,
                                                (struct diagram_text){text_buffer_bytes(buffer),
                                                                      text_buffer_length(buffer)},
                                                label)
                          : DIAGRAM_OUT_OF_MEMORY;
    text_buffer_destroy(buffer);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static bool open_frame(struct mermaid_sequence *_Nonnull parser, struct diagram_text word,
                       struct diagram_text text, enum diagram_frame_kind kind)
{
    if (parser->depth == diagram_depth_limit)
        return reject(parser, DIAGRAM_TOO_LARGE);
    struct diagram_event event = {.kind = DIAGRAM_FRAME_OPEN,
                                  .frame = kind,
                                  .link = {.from = diagram_none, .to = diagram_none}};
    if (!frame_label(parser, word, text, &event.link.label) || !add_event(parser, event))
        return false;
    parser->frames[parser->depth++] = kind;
    return true;
}
static bool frame_section(struct mermaid_sequence *_Nonnull parser, struct diagram_text word,
                          struct diagram_text text)
{
    if (parser->depth == 0)
        return reject(parser, DIAGRAM_INVALID);
    enum diagram_frame_kind frame = parser->frames[parser->depth - 1];
    bool allowed = (frame == DIAGRAM_FRAME_ALT && diagram_text_equal(word, "else")) ||
                   (frame == DIAGRAM_FRAME_PAR && diagram_text_equal(word, "and")) ||
                   (frame == DIAGRAM_FRAME_CRITICAL && diagram_text_equal(word, "option"));
    if (!allowed)
        return reject(parser, DIAGRAM_INVALID);
    struct diagram_event event = {.kind = DIAGRAM_FRAME_SECTION,
                                  .frame = frame,
                                  .link = {.from = diagram_none, .to = diagram_none}};
    return frame_label(parser, word, text, &event.link.label) && add_event(parser, event);
}
static bool close_frame(struct mermaid_sequence *_Nonnull parser, struct diagram_text text)
{
    if (parser->depth == 0 || text.length != 0)
        return reject(parser, DIAGRAM_INVALID);
    struct diagram_event event = {
        .kind = DIAGRAM_FRAME_CLOSE,
        .frame = parser->frames[--parser->depth],
        .link = {.from = diagram_none, .to = diagram_none, .label = parser->empty}};
    return add_event(parser, event);
}
static bool number(struct diagram_text text, size_t *_Nonnull out)
{
    size_t value = 0;
    if (text.length == 0)
        return false;
    for (size_t index = 0; index < text.length; ++index)
    {
        unsigned digit = (unsigned char)text.bytes[index] - (unsigned)'0';
        if (digit > 9 || value > (1000000 - digit) / 10)
            return false;
        value = value * 10 + digit;
    }
    *out = value;
    return true;
}
static bool autonumber(struct mermaid_sequence *_Nonnull parser, struct diagram_text text)
{
    if (diagram_text_equal(text, "off"))
    {
        parser->numbered = false;
        return true;
    }
    size_t start = 1;
    size_t step = 1;
    if (text.length != 0 && !number(diagram_text_word(&text), &start))
        return reject(parser, DIAGRAM_INVALID);
    if (text.length != 0 && !number(diagram_text_word(&text), &step))
        return reject(parser, DIAGRAM_INVALID);
    if (text.length != 0 || step == 0)
        return reject(parser, DIAGRAM_INVALID);
    parser->numbered = true;
    parser->next_number = start;
    parser->number_step = step;
    return true;
}
static bool named_activation(struct mermaid_sequence *_Nonnull parser, struct diagram_text text,
                             enum diagram_event_kind kind)
{
    struct diagram_text id = diagram_text_identifier(&text);
    size_t index = 0;
    if (text.length != 0)
        return reject(parser, DIAGRAM_INVALID);
    if (!actor(parser, id, &index))
        return false;
    return activation(parser, index, kind);
}
static bool title(struct mermaid_sequence *_Nonnull parser, struct diagram_text text)
{
    size_t index = 0;
    parser->outcome = diagram_label_store(parser->model, text, &index);
    if (parser->outcome == DIAGRAM_ACCEPTED)
        diagram_model_title(parser->model, index);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static bool special_statement(struct mermaid_sequence *_Nonnull parser, struct diagram_text word,
                              struct diagram_text text, bool *_Nonnull handled)
{
    *handled = true;
    if (diagram_text_equal(word, "participant"))
        return participant(parser, text, DIAGRAM_RECTANGLE);
    if (diagram_text_equal(word, "actor"))
        return participant(parser, text, DIAGRAM_ACTOR);
    if (diagram_text_equal(word, "Note") || diagram_text_equal(word, "note"))
        return note(parser, text);
    if (diagram_text_equal(word, "activate"))
        return named_activation(parser, text, DIAGRAM_ACTIVATE);
    if (diagram_text_equal(word, "deactivate"))
        return named_activation(parser, text, DIAGRAM_DEACTIVATE);
    if (diagram_text_equal(word, "autonumber"))
        return autonumber(parser, text);
    if (diagram_text_equal(word, "title"))
        return title(parser, text);
    *handled = false;
    return true;
}
static bool unsupported(struct diagram_text word)
{
    static const char *const words[] = {"box",       "rect",      "create",     "destroy",
                                        "link",      "links",     "properties", "details",
                                        "accTitle:", "accDescr:", "---"};
    for (size_t index = 0; index < sizeof words / sizeof words[0]; ++index)
        if (diagram_text_equal(word, words[index]))
            return true;
    return false;
}
static bool statement(struct mermaid_sequence *_Nonnull parser, struct diagram_text text)
{
    if (text.length == 0)
        return true;
    struct diagram_text rest = text;
    struct diagram_text word = diagram_text_word(&rest);
    bool handled = false;
    if (!special_statement(parser, word, rest, &handled) || handled)
        return parser->outcome == DIAGRAM_ACCEPTED;
    enum diagram_frame_kind kind;
    if (frame_kind(word, &kind))
        return open_frame(parser, word, rest, kind);
    if (diagram_text_equal(word, "else") || diagram_text_equal(word, "and") ||
        diagram_text_equal(word, "option"))
        return frame_section(parser, word, rest);
    if (diagram_text_equal(word, "end"))
        return close_frame(parser, rest);
    if (unsupported(word))
        return reject(parser, DIAGRAM_UNSUPPORTED);
    return message(parser, text);
}
static bool parse_document(struct mermaid_sequence *_Nonnull parser, struct diagram_text source)
{
    struct diagram_input input = {.text = source, .kind = DIAGRAM_SEQUENCE};
    struct diagram_statement first;
    parser->outcome = diagram_statement_read(input, 0, &first);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    if (!diagram_text_equal(first.text, "sequenceDiagram"))
        return reject(parser, DIAGRAM_INVALID);
    size_t cursor = first.next;
    while (cursor < source.length)
    {
        struct diagram_statement next;
        parser->outcome = diagram_statement_read(input, cursor, &next);
        if (parser->outcome != DIAGRAM_ACCEPTED || !statement(parser, next.text))
            return false;
        cursor = next.next;
    }
    if (parser->depth != 0 || diagram_model_node_count(parser->model) == 0)
        return reject(parser, DIAGRAM_INVALID);
    return true;
}
enum diagram_outcome mermaid_sequence_create(struct diagram_text source,
                                             struct diagram_model *_Nullable *_Nonnull out)
{
    struct diagram_model *_Nullable model = nullptr;
    enum diagram_outcome outcome = diagram_model_create(DIAGRAM_SEQUENCE, &model);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    struct mermaid_sequence parser = {.model = model, .outcome = DIAGRAM_ACCEPTED};
    parser.outcome = diagram_model_text(model, "", 0, &parser.empty);
    if (parser.outcome == DIAGRAM_ACCEPTED)
        (void)parse_document(&parser, source);
    if (parser.outcome != DIAGRAM_ACCEPTED)
    {
        diagram_model_destroy(model);
        return parser.outcome;
    }
    diagram_model_seal(model);
    *out = model;
    return DIAGRAM_ACCEPTED;
}
