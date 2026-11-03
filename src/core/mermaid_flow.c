/* Mermaid 12.2.0 flow grammar and Pele 3a2f075 parser informed this native parser.
 * The supported grammar is explicit; unsupported directives never produce a partial graph. */
#include "mermaid_flow.h"
#include "diagram_label.h"
#include "diagram_statement.h"
#include "mermaid_shape.h"
#include <string.h>
struct mermaid_flow
{
    struct diagram_model *_Nonnull model;
    enum diagram_outcome outcome;
    size_t groups[diagram_depth_limit];
    size_t depth;
    size_t empty;
};
static bool reject(struct mermaid_flow *_Nonnull parser, enum diagram_outcome outcome)
{
    parser->outcome = outcome;
    return false;
}
static size_t current_group(const struct mermaid_flow *_Nonnull parser)
{
    return parser->depth == 0 ? diagram_none : parser->groups[parser->depth - 1];
}
static bool direction(struct diagram_text text, enum diagram_direction *_Nonnull out)
{
    if (diagram_text_equal(text, "TD") || diagram_text_equal(text, "TB"))
        *out = DIAGRAM_TOP_BOTTOM;
    else if (diagram_text_equal(text, "BT"))
        *out = DIAGRAM_BOTTOM_TOP;
    else if (diagram_text_equal(text, "LR"))
        *out = DIAGRAM_LEFT_RIGHT;
    else if (diagram_text_equal(text, "RL"))
        *out = DIAGRAM_RIGHT_LEFT;
    else
        return false;
    return true;
}
static size_t closing_at(struct diagram_text text, const char *_Nonnull closing)
{
    bool quoted = false;
    for (size_t index = 0; index < text.length; ++index)
    {
        if (text.bytes[index] == '\\' && quoted && index + 1 < text.length)
        {
            ++index;
            continue;
        }
        if (text.bytes[index] == '"')
            quoted = !quoted;
        if (!quoted && diagram_text_starts(diagram_text_skip(text, index), closing))
            return index;
    }
    return diagram_none;
}
static bool shaped_label(struct mermaid_flow *_Nonnull parser, struct diagram_text *_Nonnull text,
                         const struct mermaid_shape *_Nonnull shape,
                         struct diagram_node *_Nonnull node)
{
    struct diagram_text body = diagram_text_skip(*text, strlen(shape->opening));
    size_t end = closing_at(body, shape->closing);
    if (end == diagram_none || end > closing_at(body, "]") || end > closing_at(body, ")") ||
        end > closing_at(body, "}"))
        return reject(parser, DIAGRAM_INVALID);
    parser->outcome =
        diagram_label_store(parser->model, (struct diagram_text){body.bytes, end}, &node->label);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    node->shape = shape->shape;
    *text = diagram_text_trim(diagram_text_skip(body, end + strlen(shape->closing)));
    return true;
}
static const struct mermaid_shape *_Nullable shape_for(struct diagram_text text)
{
    static const struct mermaid_shape shapes[] = {
        {"(((", ")))", DIAGRAM_DOUBLE_CIRCLE}, {"([", "])", DIAGRAM_STADIUM},
        {"[[", "]]", DIAGRAM_SUBROUTINE},      {"[(", ")]", DIAGRAM_CYLINDER},
        {"((", "))", DIAGRAM_CIRCLE},          {"{{", "}}", DIAGRAM_HEXAGON},
        {"[/", "/]", DIAGRAM_PARALLELOGRAM},   {"[\\", "\\]", DIAGRAM_PARALLELOGRAM_REVERSE},
        {"[/", "\\]", DIAGRAM_TRAPEZOID},      {"[\\", "/]", DIAGRAM_TRAPEZOID_REVERSE},
        {"[", "]", DIAGRAM_RECTANGLE},         {"(", ")", DIAGRAM_ROUNDED},
        {"{", "}", DIAGRAM_DIAMOND},           {">", "]", DIAGRAM_ASYMMETRIC}};
    const struct mermaid_shape *_Nullable selected = nullptr;
    size_t opening = 0;
    size_t ending = diagram_none;
    for (size_t index = 0; index < sizeof shapes / sizeof shapes[0]; ++index)
    {
        const struct mermaid_shape *shape = &shapes[index];
        size_t length = strlen(shape->opening);
        if (length < opening || !diagram_text_starts(text, shape->opening))
            continue;
        size_t end = closing_at(diagram_text_skip(text, length), shape->closing);
        if (length > opening || end < ending)
        {
            selected = shape;
            opening = length;
            ending = end;
        }
    }
    return selected;
}
static bool node_shape(struct mermaid_flow *_Nonnull parser, struct diagram_text *_Nonnull text,
                       struct diagram_node *_Nonnull node)
{
    const struct mermaid_shape *_Nullable shape = shape_for(*text);
    return shape == nullptr || shaped_label(parser, text, shape, node);
}
static bool new_node(struct mermaid_flow *_Nonnull parser, struct diagram_text id,
                     struct diagram_node *_Nonnull node)
{
    parser->outcome = diagram_model_text(parser->model, id.bytes, id.length, &node->id);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    node->label = node->id;
    node->shape = DIAGRAM_RECTANGLE;
    node->group = current_group(parser);
    return true;
}
static bool read_node(struct mermaid_flow *_Nonnull parser, struct diagram_text *_Nonnull text,
                      size_t *_Nonnull index)
{
    struct diagram_text id = diagram_text_identifier(text);
    if (id.length == 0)
        return reject(parser, DIAGRAM_INVALID);
    if (diagram_model_find_group(parser->model, id.bytes, id.length) != diagram_none)
        return reject(parser, DIAGRAM_UNSUPPORTED);
    *index = diagram_model_find_node(parser->model, id.bytes, id.length);
    struct diagram_node node = {.group = diagram_none};
    if (*index == diagram_none)
    {
        *index = diagram_model_node_count(parser->model);
        if (!new_node(parser, id, &node))
            return false;
    }
    else
        node = diagram_model_node_at(parser->model, *index);
    if (!node_shape(parser, text, &node))
        return false;
    if (current_group(parser) != diagram_none)
        node.group = current_group(parser);
    parser->outcome = diagram_model_node(parser->model, *index, node);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static enum diagram_arrow arrow(char value)
{
    if (value == '<' || value == '>')
        return DIAGRAM_ARROW_CLOSED;
    if (value == 'o')
        return DIAGRAM_ARROW_CIRCLE;
    if (value == 'x')
        return DIAGRAM_ARROW_CROSS;
    return DIAGRAM_ARROW_NONE;
}
static size_t link_token(struct diagram_text text)
{
    size_t count = 0;
    if (text.length != 0 && diagram_text_character_in(text.bytes[0], "<ox"))
        ++count;
    while (count < text.length && diagram_text_character_in(text.bytes[count], "-.=~"))
        ++count;
    if (count < text.length && diagram_text_character_in(text.bytes[count], ">ox"))
        ++count;
    return count;
}
static enum diagram_stroke stroke(char value)
{
    if (value == '=')
        return DIAGRAM_THICK;
    if (value == '~')
        return DIAGRAM_INVISIBLE;
    return DIAGRAM_SOLID;
}
static bool finish_stroke(struct mermaid_flow *_Nonnull parser, char solid, size_t dots,
                          struct diagram_link *_Nonnull link)
{
    if (link->length < 2 || dots > 1 || (dots != 0 && solid != '-'))
        return reject(parser, DIAGRAM_INVALID);
    link->stroke = dots == 0 ? stroke(solid) : DIAGRAM_DASHED;
    return true;
}
static bool symbolic_body(struct mermaid_flow *_Nonnull parser, struct diagram_text text,
                          struct diagram_link *_Nonnull link)
{
    size_t dashes = 0;
    size_t dots = 0;
    char solid = text.length == 0 ? '\0' : text.bytes[0];
    for (size_t index = 0; index < text.length; ++index)
    {
        char value = text.bytes[index];
        if (value == '.')
        {
            ++dots;
            continue;
        }
        if (value != solid || !diagram_text_character_in(value, "-=~"))
            return reject(parser, DIAGRAM_INVALID);
        ++dashes;
    }
    link->length = dashes;
    return finish_stroke(parser, solid, dots, link);
}
static bool symbolic_link(struct mermaid_flow *_Nonnull parser, struct diagram_text token,
                          struct diagram_link *_Nonnull link)
{
    if (token.length < 3)
        return reject(parser, DIAGRAM_INVALID);
    link->start = arrow(token.bytes[0]);
    link->end = arrow(token.bytes[token.length - 1]);
    size_t begin = link->start == DIAGRAM_ARROW_NONE ? 0 : 1;
    size_t end = token.length - (link->end == DIAGRAM_ARROW_NONE ? 0 : 1);
    if (!symbolic_body(parser, (struct diagram_text){token.bytes + begin, end - begin}, link))
        return false;
    size_t base = link->start != DIAGRAM_ARROW_NONE || link->end != DIAGRAM_ARROW_NONE ? 1 : 2;
    link->length = link->length > base ? link->length - base : 1;
    if (link->length > 16)
        return reject(parser, DIAGRAM_TOO_LARGE);
    return true;
}
static bool pipe_label(struct mermaid_flow *_Nonnull parser, struct diagram_text *_Nonnull text,
                       struct diagram_link *_Nonnull link)
{
    if (text->length == 0 || text->bytes[0] != '|')
        return true;
    struct diagram_text body = diagram_text_skip(*text, 1);
    size_t end = closing_at(body, "|");
    if (end == diagram_none)
        return reject(parser, DIAGRAM_INVALID);
    parser->outcome =
        diagram_label_store(parser->model, (struct diagram_text){body.bytes, end}, &link->label);
    *text = diagram_text_trim(diagram_text_skip(body, end + 1));
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static bool text_link(struct mermaid_flow *_Nonnull parser, struct diagram_text *_Nonnull text,
                      struct diagram_link *_Nonnull link)
{
    const char *ending = diagram_text_starts(*text, "-.") ? ".->" : "-->";
    link->stroke = diagram_text_starts(*text, "-.") ? DIAGRAM_DASHED : DIAGRAM_SOLID;
    if (diagram_text_starts(*text, "=="))
    {
        ending = "==>";
        link->stroke = DIAGRAM_THICK;
    }
    struct diagram_text body = diagram_text_skip(*text, 2);
    size_t end = closing_at(body, ending);
    if (end == diagram_none)
        return reject(parser, DIAGRAM_INVALID);
    parser->outcome =
        diagram_label_store(parser->model, (struct diagram_text){body.bytes, end}, &link->label);
    *text = diagram_text_trim(diagram_text_skip(body, end + strlen(ending)));
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static bool read_link(struct mermaid_flow *_Nonnull parser, struct diagram_text *_Nonnull text,
                      struct diagram_link *_Nonnull link)
{
    *link = (struct diagram_link){.label = parser->empty,
                                  .length = 1,
                                  .stroke = DIAGRAM_SOLID,
                                  .start = DIAGRAM_ARROW_NONE,
                                  .end = DIAGRAM_ARROW_CLOSED};
    size_t length = link_token(*text);
    if (length == 2 && (diagram_text_starts(*text, "--") || diagram_text_starts(*text, "-.") ||
                        diagram_text_starts(*text, "==")))
        return text_link(parser, text, link);
    if (length == 0)
        return reject(parser, DIAGRAM_UNSUPPORTED);
    if (!symbolic_link(parser, (struct diagram_text){text->bytes, length}, link))
        return false;
    *text = diagram_text_trim(diagram_text_skip(*text, length));
    return pipe_label(parser, text, link);
}
static bool chain(struct mermaid_flow *_Nonnull parser, struct diagram_text text)
{
    size_t from = 0;
    if (!read_node(parser, &text, &from))
        return false;
    while (text.length != 0)
    {
        struct diagram_link link;
        if (!read_link(parser, &text, &link))
            return false;
        link.from = from;
        if (!read_node(parser, &text, &link.to))
            return false;
        parser->outcome = diagram_model_link(parser->model, link);
        if (parser->outcome != DIAGRAM_ACCEPTED)
            return false;
        from = link.to;
    }
    return true;
}
static bool open_group(struct mermaid_flow *_Nonnull parser, struct diagram_text text)
{
    if (parser->depth == diagram_depth_limit)
        return reject(parser, DIAGRAM_TOO_LARGE);
    struct diagram_text title = diagram_text_trim(text);
    struct diagram_text id = diagram_text_identifier(&text);
    if (id.length == 0 ||
        diagram_model_find_group(parser->model, id.bytes, id.length) != diagram_none ||
        diagram_model_find_node(parser->model, id.bytes, id.length) != diagram_none)
        return reject(parser, DIAGRAM_INVALID);
    if (text.length != 0 && text.bytes[0] == '[')
    {
        if (text.bytes[text.length - 1] != ']')
            return reject(parser, DIAGRAM_INVALID);
        title = (struct diagram_text){text.bytes + 1, text.length - 2};
    }
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
static bool set_direction(struct mermaid_flow *_Nonnull parser, struct diagram_text text)
{
    enum diagram_direction value;
    if (!direction(text, &value))
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
static bool unsupported(struct diagram_text word)
{
    static const char *const words[] = {"classDef", "class",     "style",     "linkStyle",
                                        "click",    "accTitle:", "accDescr:", "---"};
    for (size_t index = 0; index < sizeof words / sizeof words[0]; ++index)
        if (diagram_text_equal(word, words[index]))
            return true;
    return false;
}
static bool statement(struct mermaid_flow *_Nonnull parser, struct diagram_text text)
{
    if (text.length == 0)
        return true;
    struct diagram_text rest = text;
    struct diagram_text word = diagram_text_word(&rest);
    if (unsupported(word))
        return reject(parser, DIAGRAM_UNSUPPORTED);
    if (diagram_text_equal(word, "subgraph"))
        return open_group(parser, rest);
    if (diagram_text_equal(word, "direction"))
        return set_direction(parser, rest);
    if (diagram_text_equal(word, "end"))
    {
        if (parser->depth == 0 || rest.length != 0)
            return reject(parser, DIAGRAM_INVALID);
        --parser->depth;
        return true;
    }
    return chain(parser, text);
}
static bool parse_document(struct mermaid_flow *_Nonnull parser, struct diagram_text source)
{
    struct diagram_input input = {.text = source, .kind = DIAGRAM_FLOWCHART};
    struct diagram_statement first;
    parser->outcome = diagram_statement_read(input, 0, &first);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    struct diagram_text header = diagram_text_word(&first.text);
    if (!diagram_text_equal(header, "flowchart") && !diagram_text_equal(header, "graph"))
        return reject(parser, DIAGRAM_INVALID);
    if (!set_direction(parser, first.text))
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
    if (parser->depth != 0 || diagram_model_node_count(parser->model) == 0)
        return reject(parser, DIAGRAM_INVALID);
    return true;
}
enum diagram_outcome mermaid_flow_create(struct diagram_text source,
                                         struct diagram_model *_Nullable *_Nonnull out)
{
    struct diagram_model *_Nullable model = nullptr;
    enum diagram_outcome outcome = diagram_model_create(DIAGRAM_FLOWCHART, &model);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    struct mermaid_flow parser = {.model = model, .outcome = DIAGRAM_ACCEPTED};
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
