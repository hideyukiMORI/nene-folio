/* #872 / ADR0104: Mermaid `stateDiagram` / `stateDiagram-v2` read into the flowchart model, so
 * the existing layout and drawing serve it. The supported grammar is explicit; notes,
 * concurrency, fork/join, styles and transitions to a composite state are unsupported and never
 * produce a partial graph. */
#include "mermaid_state.h"
#include "diagram_label.h"
#include "diagram_statement.h"
#include "mermaid_flow.h"
#include <string.h>
struct mermaid_state
{
    struct diagram_model *_Nonnull model;
    enum diagram_outcome outcome;
    size_t groups[diagram_depth_limit];
    size_t depth;
    size_t empty;
};
static bool reject(struct mermaid_state *_Nonnull parser, enum diagram_outcome outcome)
{
    parser->outcome = outcome;
    return false;
}
static size_t current_group(const struct mermaid_state *_Nonnull parser)
{
    return parser->depth == 0 ? diagram_none : parser->groups[parser->depth - 1];
}
static bool contains(struct diagram_text text, const char *_Nonnull word)
{
    for (size_t index = 0; index < text.length; ++index)
        if (diagram_text_starts(diagram_text_skip(text, index), word))
            return true;
    return false;
}
/* `[*]` is a start in its scope when a transition leaves it and an end when one reaches it. The
 * id names the scope so each composite state has its own (`[*]<` / `[*]>` and the group). */
static size_t pseudo_id(char *_Nonnull out, bool start, size_t group)
{
    memcpy(out, start ? "[*]<" : "[*]>", 4);
    size_t length = 4;
    size_t value = group == diagram_none ? 0 : group + 1;
    char digits[24];
    size_t count = 0;
    do
    {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    while (count != 0)
        out[length++] = digits[--count];
    return length;
}
static bool pseudo(struct mermaid_state *_Nonnull parser, bool start, size_t *_Nonnull index)
{
    char id[32];
    size_t length = pseudo_id(id, start, current_group(parser));
    *index = diagram_model_find_node(parser->model, id, length);
    if (*index != diagram_none)
        return true;
    *index = diagram_model_node_count(parser->model);
    struct diagram_node node = {.label = parser->empty,
                                .group = current_group(parser),
                                .shape = start ? DIAGRAM_CIRCLE : DIAGRAM_DOUBLE_CIRCLE};
    parser->outcome = diagram_model_text(parser->model, id, length, &node.id);
    if (parser->outcome == DIAGRAM_ACCEPTED)
        parser->outcome = diagram_model_node(parser->model, *index, node);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
/* A state is found by id or created in the current scope, labelled by its id. */
static bool state_node(struct mermaid_state *_Nonnull parser, struct diagram_text id,
                       size_t *_Nonnull index)
{
    if (id.length == 0)
        return reject(parser, DIAGRAM_INVALID);
    if (diagram_model_find_group(parser->model, id.bytes, id.length) != diagram_none)
        return reject(parser, DIAGRAM_UNSUPPORTED);
    *index = diagram_model_find_node(parser->model, id.bytes, id.length);
    if (*index != diagram_none)
        return true;
    *index = diagram_model_node_count(parser->model);
    struct diagram_node node = {.group = current_group(parser), .shape = DIAGRAM_ROUNDED};
    parser->outcome = diagram_model_text(parser->model, id.bytes, id.length, &node.id);
    node.label = node.id;
    if (parser->outcome == DIAGRAM_ACCEPTED)
        parser->outcome = diagram_model_node(parser->model, *index, node);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static bool endpoint(struct mermaid_state *_Nonnull parser, struct diagram_text *_Nonnull text,
                     bool start, size_t *_Nonnull index)
{
    *text = diagram_text_trim(*text);
    if (diagram_text_starts(*text, "[*]"))
    {
        *text = diagram_text_trim(diagram_text_skip(*text, 3));
        return pseudo(parser, start, index);
    }
    return state_node(parser, diagram_text_identifier(text), index);
}
/* `A --> B` with an optional `: label`. */
static bool transition(struct mermaid_state *_Nonnull parser, struct diagram_text text)
{
    struct diagram_link link = {.label = parser->empty,
                                .length = 1,
                                .stroke = DIAGRAM_SOLID,
                                .start = DIAGRAM_ARROW_NONE,
                                .end = DIAGRAM_ARROW_CLOSED};
    if (!endpoint(parser, &text, true, &link.from))
        return false;
    if (!diagram_text_starts(text, "-->"))
        return reject(parser, DIAGRAM_INVALID);
    text = diagram_text_skip(text, 3);
    if (!endpoint(parser, &text, false, &link.to))
        return false;
    if (text.length != 0 && text.bytes[0] != ':')
        return reject(parser, DIAGRAM_INVALID);
    if (text.length != 0)
        parser->outcome = diagram_label_store(
            parser->model, diagram_text_trim(diagram_text_skip(text, 1)), &link.label);
    if (parser->outcome == DIAGRAM_ACCEPTED)
        parser->outcome = diagram_model_link(parser->model, link);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
/* A state's shown text: `A : description`, or the `"..."` of `state "..." as A`. */
static bool relabel(struct mermaid_state *_Nonnull parser, struct diagram_text id,
                    struct diagram_text label)
{
    size_t index = 0;
    if (!state_node(parser, id, &index))
        return false;
    struct diagram_node node = diagram_model_node_at(parser->model, index);
    parser->outcome = diagram_label_store(parser->model, label, &node.label);
    if (parser->outcome == DIAGRAM_ACCEPTED)
        parser->outcome = diagram_model_node(parser->model, index, node);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static bool open_group(struct mermaid_state *_Nonnull parser, struct diagram_text id,
                       struct diagram_text title)
{
    if (parser->depth == diagram_depth_limit)
        return reject(parser, DIAGRAM_TOO_LARGE);
    if (id.length == 0 ||
        diagram_model_find_group(parser->model, id.bytes, id.length) != diagram_none)
        return reject(parser, DIAGRAM_INVALID);
    /* A state already met in a transition becoming a composite is a transition to a composite. */
    if (diagram_model_find_node(parser->model, id.bytes, id.length) != diagram_none)
        return reject(parser, DIAGRAM_UNSUPPORTED);
    struct diagram_group group = {.parent = current_group(parser),
                                  .direction = diagram_model_flow(parser->model)};
    parser->outcome = diagram_model_text(parser->model, id.bytes, id.length, &group.id);
    if (parser->outcome == DIAGRAM_ACCEPTED)
        parser->outcome = diagram_label_store(parser->model, title, &group.label);
    size_t index = diagram_model_group_count(parser->model);
    if (parser->outcome == DIAGRAM_ACCEPTED)
        parser->outcome = diagram_model_group(parser->model, index, group);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    parser->groups[parser->depth++] = index;
    return true;
}
/* What follows the state's id in `state ...`: nothing, `{`, or `<<choice>>`. */
static bool declared(struct mermaid_state *_Nonnull parser, struct diagram_text id,
                     struct diagram_text title, struct diagram_text rest)
{
    if (diagram_text_equal(rest, "{"))
        return open_group(parser, id, title);
    if (diagram_text_equal(rest, "<<choice>>"))
    {
        size_t index = 0;
        if (!state_node(parser, id, &index))
            return false;
        struct diagram_node node = diagram_model_node_at(parser->model, index);
        node.shape = DIAGRAM_DIAMOND;
        node.label = parser->empty;
        parser->outcome = diagram_model_node(parser->model, index, node);
        return parser->outcome == DIAGRAM_ACCEPTED;
    }
    if (diagram_text_starts(rest, "<<"))
        return reject(parser, DIAGRAM_UNSUPPORTED);
    if (rest.length != 0)
        return reject(parser, DIAGRAM_INVALID);
    return title.bytes == id.bytes ? state_node(parser, id, &(size_t){0})
                                   : relabel(parser, id, title);
}
/* `state A`, `state A {`, `state A <<choice>>`, `state "Label" as A` (optionally with `{`). */
static bool declaration(struct mermaid_state *_Nonnull parser, struct diagram_text rest)
{
    rest = diagram_text_trim(rest);
    if (rest.length == 0 || rest.bytes[0] != '"')
    {
        struct diagram_text id = diagram_text_identifier(&rest);
        return declared(parser, id, id, rest);
    }
    struct diagram_text body = diagram_text_skip(rest, 1);
    size_t close = 0;
    while (close < body.length && body.bytes[close] != '"')
        ++close;
    if (close == body.length)
        return reject(parser, DIAGRAM_INVALID);
    struct diagram_text title = {body.bytes, close};
    struct diagram_text after = diagram_text_skip(body, close + 1);
    if (!diagram_text_equal(diagram_text_word(&after), "as"))
        return reject(parser, DIAGRAM_INVALID);
    struct diagram_text id = diagram_text_identifier(&after);
    return declared(parser, id, title, after);
}
static bool set_direction(struct mermaid_state *_Nonnull parser, struct diagram_text text)
{
    enum diagram_direction value;
    if (!mermaid_flow_direction(text, &value))
        return reject(parser, DIAGRAM_INVALID);
    size_t group = current_group(parser);
    if (group == diagram_none)
        diagram_model_direction(parser->model, value);
    if (group != diagram_none)
    {
        struct diagram_group record = diagram_model_group_at(parser->model, group);
        record.direction = value;
        parser->outcome = diagram_model_group(parser->model, group, record);
    }
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static bool unsupported(struct diagram_text text, struct diagram_text word)
{
    static const char *const words[] = {"note",  "classDef",  "class",     "style",
                                        "click", "accTitle:", "accDescr:", "--"};
    for (size_t index = 0; index < sizeof words / sizeof words[0]; ++index)
        if (diagram_text_equal(word, words[index]))
            return true;
    return contains(text, ":::");
}
static bool close_group(struct mermaid_state *_Nonnull parser)
{
    if (parser->depth == 0)
        return reject(parser, DIAGRAM_INVALID);
    --parser->depth;
    return true;
}
static bool statement(struct mermaid_state *_Nonnull parser, struct diagram_text text)
{
    if (text.length == 0)
        return true;
    if (diagram_text_equal(text, "}"))
        return close_group(parser);
    struct diagram_text rest = text;
    struct diagram_text word = diagram_text_word(&rest);
    if (unsupported(text, word))
        return reject(parser, DIAGRAM_UNSUPPORTED);
    if (diagram_text_equal(word, "direction"))
        return set_direction(parser, rest);
    if (diagram_text_equal(word, "state"))
        return declaration(parser, rest);
    if (contains(text, "-->"))
        return transition(parser, text);
    rest = text;
    struct diagram_text id = diagram_text_identifier(&rest);
    if (rest.length == 0)
        return state_node(parser, id, &(size_t){0});
    if (rest.bytes[0] != ':')
        return reject(parser, DIAGRAM_INVALID);
    return relabel(parser, id, diagram_text_trim(diagram_text_skip(rest, 1)));
}
static bool parse_document(struct mermaid_state *_Nonnull parser, struct diagram_text source)
{
    struct diagram_input input = {.text = source, .kind = DIAGRAM_SEQUENCE};
    struct diagram_statement first;
    parser->outcome = diagram_statement_read(input, 0, &first);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    struct diagram_text header = diagram_text_word(&first.text);
    if ((!diagram_text_equal(header, "stateDiagram") &&
         !diagram_text_equal(header, "stateDiagram-v2")) ||
        first.text.length != 0)
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
enum diagram_outcome mermaid_state_create(struct diagram_text source,
                                          struct diagram_model *_Nullable *_Nonnull out)
{
    struct diagram_model *_Nullable model = nullptr;
    enum diagram_outcome outcome = diagram_model_create(DIAGRAM_FLOWCHART, &model);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    struct mermaid_state parser = {.model = model, .outcome = DIAGRAM_ACCEPTED};
    diagram_model_direction(model, DIAGRAM_TOP_BOTTOM);
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
