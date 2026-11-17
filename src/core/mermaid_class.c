/* #890 / ADR0107: Mermaid `classDiagram` read into the flowchart model, so the existing layout
 * and drawing serve it. A class is a box (its name over its members once it has any); a relation
 * is a line whose ends carry the UML markers. Cardinalities, generics, notes, namespaces and
 * styles are unsupported and never produce a partial graph. */
#include "mermaid_class.h"
#include "diagram_label.h"
#include "diagram_statement.h"
#include "mermaid_flow.h"
#include <string.h>
struct mermaid_class
{
    struct diagram_model *_Nonnull model;
    enum diagram_outcome outcome;
    size_t empty;
    size_t open; /* the class whose member block is open, or diagram_none */
    size_t used;
    char label[diagram_label_limit];
};
static bool reject(struct mermaid_class *_Nonnull parser, enum diagram_outcome outcome)
{
    parser->outcome = outcome;
    return false;
}
static bool append(struct mermaid_class *_Nonnull parser, const char *_Nonnull bytes, size_t length)
{
    if (length > sizeof parser->label - parser->used)
        return reject(parser, DIAGRAM_TOO_LARGE);
    memcpy(parser->label + parser->used, bytes, length);
    parser->used += length;
    return true;
}
/* A class is found by name or created as a box labelled by its name. Generic names (`List~T~`),
 * quoted names and `Name["shown"]` are not read. */
static bool class_node(struct mermaid_class *_Nonnull parser, struct diagram_text name,
                       size_t *_Nonnull index)
{
    if (name.length == 0)
        return reject(parser, DIAGRAM_INVALID);
    *index = diagram_model_find_node(parser->model, name.bytes, name.length);
    if (*index != diagram_none)
        return true;
    *index = diagram_model_node_count(parser->model);
    struct diagram_node node = {.group = diagram_none, .shape = DIAGRAM_RECTANGLE};
    parser->outcome = diagram_model_text(parser->model, name.bytes, name.length, &node.id);
    node.label = node.id;
    if (parser->outcome == DIAGRAM_ACCEPTED)
        parser->outcome = diagram_model_node(parser->model, *index, node);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
/* The members a class already has go back into the buffer, so more can follow them. */
static bool begin_members(struct mermaid_class *_Nonnull parser, size_t index)
{
    struct diagram_node node = diagram_model_node_at(parser->model, index);
    parser->used = 0;
    parser->open = index;
    if (node.shape != DIAGRAM_ENTITY)
        return true;
    const char *_Nonnull listed = diagram_model_text_at(parser->model, node.label);
    return append(parser, listed, strlen(listed));
}
/* One member line; `<<interface>>` reads as «interface». */
static bool member(struct mermaid_class *_Nonnull parser, struct diagram_text text)
{
    text = diagram_text_trim(text);
    if (text.length == 0)
        return reject(parser, DIAGRAM_INVALID);
    if (parser->used != 0 && !append(parser, "\n", 1))
        return false;
    bool annotation = text.length > 4 && diagram_text_starts(text, "<<") &&
                      text.bytes[text.length - 1] == '>' && text.bytes[text.length - 2] == '>';
    if (!annotation)
        return append(parser, text.bytes, text.length);
    return append(parser, "\xC2\xAB", 2) && append(parser, text.bytes + 2, text.length - 4) &&
           append(parser, "\xC2\xBB", 2);
}
static bool end_members(struct mermaid_class *_Nonnull parser)
{
    struct diagram_node node = diagram_model_node_at(parser->model, parser->open);
    size_t index = parser->open;
    parser->open = diagram_none;
    if (parser->used == 0)
        return true; /* an empty block leaves a plain box */
    node.shape = DIAGRAM_ENTITY;
    parser->outcome = diagram_model_text(parser->model, parser->label, parser->used, &node.label);
    if (parser->outcome == DIAGRAM_ACCEPTED)
        parser->outcome = diagram_model_node(parser->model, index, node);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
/* `class Name` or `class Name {`. */
static bool declaration(struct mermaid_class *_Nonnull parser, struct diagram_text rest)
{
    struct diagram_text name = diagram_text_identifier(&rest);
    size_t index = 0;
    if (rest.length != 0 && !diagram_text_equal(rest, "{"))
        return reject(parser, name.length == 0 ? DIAGRAM_INVALID : DIAGRAM_UNSUPPORTED);
    if (!class_node(parser, name, &index))
        return false;
    return rest.length == 0 || begin_members(parser, index);
}
/* The marker before the line: `<|` inheritance, `*` composition, `o` aggregation, `<` arrow. */
static enum diagram_arrow left_marker(struct diagram_text *_Nonnull text)
{
    static const char *const marks[] = {"<|", "*", "o", "<"};
    static const enum diagram_arrow arrows[] = {DIAGRAM_ARROW_HOLLOW, DIAGRAM_ARROW_DIAMOND_FILLED,
                                                DIAGRAM_ARROW_DIAMOND, DIAGRAM_ARROW_OPEN};
    for (size_t index = 0; index < sizeof marks / sizeof marks[0]; ++index)
    {
        struct diagram_text after = diagram_text_skip(*text, strlen(marks[index]));
        if (diagram_text_starts(*text, marks[index]) &&
            (diagram_text_starts(after, "--") || diagram_text_starts(after, "..")))
        {
            *text = after;
            return arrows[index];
        }
    }
    return DIAGRAM_ARROW_NONE;
}
/* The marker after the line. `o` and `*` are markers only when a space follows, so a class
 * named `order` right after the line is not read as one. */
static enum diagram_arrow right_marker(struct diagram_text *_Nonnull text)
{
    static const char *const marks[] = {"|>", "*", "o", ">"};
    static const enum diagram_arrow arrows[] = {DIAGRAM_ARROW_HOLLOW, DIAGRAM_ARROW_DIAMOND_FILLED,
                                                DIAGRAM_ARROW_DIAMOND, DIAGRAM_ARROW_OPEN};
    for (size_t index = 0; index < sizeof marks / sizeof marks[0]; ++index)
    {
        struct diagram_text after = diagram_text_skip(*text, strlen(marks[index]));
        bool letter = marks[index][0] == '*' || marks[index][0] == 'o';
        if (diagram_text_starts(*text, marks[index]) &&
            (!letter || (after.length != 0 && after.bytes[0] == ' ')))
        {
            *text = after;
            return arrows[index];
        }
    }
    return DIAGRAM_ARROW_NONE;
}
/* `A <|-- B : label`: markers around `--` (solid) or `..` (dashed). */
static bool line_between(struct mermaid_class *_Nonnull parser, struct diagram_text *_Nonnull rest,
                         struct diagram_link *_Nonnull link)
{
    link->start = left_marker(rest);
    bool solid = diagram_text_starts(*rest, "--");
    if (!solid && !diagram_text_starts(*rest, ".."))
        return reject(parser, rest->bytes[0] == '"' ? DIAGRAM_UNSUPPORTED : DIAGRAM_INVALID);
    link->stroke = solid ? DIAGRAM_SOLID : DIAGRAM_DASHED;
    *rest = diagram_text_skip(*rest, 2);
    link->end = right_marker(rest);
    return true;
}
static bool relation(struct mermaid_class *_Nonnull parser, struct diagram_text name,
                     struct diagram_text rest)
{
    struct diagram_link link = {.label = parser->empty, .length = 1};
    if (!class_node(parser, name, &link.from) || !line_between(parser, &rest, &link))
        return false;
    struct diagram_text other = diagram_text_identifier(&rest);
    if (other.length == 0 && rest.length != 0)
        return reject(parser, DIAGRAM_UNSUPPORTED);
    if (!class_node(parser, other, &link.to))
        return false;
    if (rest.length != 0 && rest.bytes[0] != ':')
        return reject(parser, DIAGRAM_UNSUPPORTED);
    if (rest.length != 0)
        parser->outcome = diagram_label_store(
            parser->model, diagram_text_trim(diagram_text_skip(rest, 1)), &link.label);
    if (parser->outcome == DIAGRAM_ACCEPTED)
        parser->outcome = diagram_model_link(parser->model, link);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static bool unsupported(struct diagram_text text, struct diagram_text word)
{
    static const char *const words[] = {"note",  "namespace", "style",    "classDef",
                                        "click", "link",      "callback", "cssClass"};
    for (size_t index = 0; index < sizeof words / sizeof words[0]; ++index)
        if (diagram_text_equal(word, words[index]))
            return true;
    return diagram_text_starts(text, "accTitle") || diagram_text_starts(text, "accDescr") ||
           diagram_text_starts(text, "\"") || diagram_text_starts(text, "<<");
}
static bool set_direction(struct mermaid_class *_Nonnull parser, struct diagram_text text)
{
    enum diagram_direction direction = DIAGRAM_TOP_BOTTOM;
    if (!mermaid_flow_direction(diagram_text_trim(text), &direction))
        return reject(parser, DIAGRAM_INVALID);
    diagram_model_direction(parser->model, direction);
    return true;
}
/* `Name`, `Name : member`, or a relation. */
static bool named(struct mermaid_class *_Nonnull parser, struct diagram_text text)
{
    struct diagram_text rest = text;
    struct diagram_text name = diagram_text_identifier(&rest);
    size_t index = 0;
    if (rest.length == 0)
        return class_node(parser, name, &index);
    if (rest.bytes[0] != ':')
        return relation(parser, name, rest);
    return class_node(parser, name, &index) && begin_members(parser, index) &&
           member(parser, diagram_text_skip(rest, 1)) && end_members(parser);
}
static bool statement(struct mermaid_class *_Nonnull parser, struct diagram_text text)
{
    if (text.length == 0)
        return true;
    if (parser->open != diagram_none)
        return diagram_text_equal(text, "}") ? end_members(parser) : member(parser, text);
    struct diagram_text rest = text;
    struct diagram_text word = diagram_text_word(&rest);
    if (unsupported(text, word))
        return reject(parser, DIAGRAM_UNSUPPORTED);
    if (diagram_text_equal(word, "direction"))
        return set_direction(parser, rest);
    if (diagram_text_equal(word, "class"))
        return declaration(parser, rest);
    return named(parser, text);
}
static bool parse_document(struct mermaid_class *_Nonnull parser, struct diagram_text source)
{
    struct diagram_input input = {.text = source, .kind = DIAGRAM_SEQUENCE};
    struct diagram_statement first;
    parser->outcome = diagram_statement_read(input, 0, &first);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    struct diagram_text header = diagram_text_word(&first.text);
    if ((!diagram_text_equal(header, "classDiagram") &&
         !diagram_text_equal(header, "classDiagram-v2")) ||
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
    if (parser->open != diagram_none || diagram_model_node_count(parser->model) == 0)
        return reject(parser, DIAGRAM_INVALID);
    return true;
}
enum diagram_outcome mermaid_class_create(struct diagram_text source,
                                          struct diagram_model *_Nullable *_Nonnull out)
{
    struct diagram_model *_Nullable model = nullptr;
    enum diagram_outcome outcome = diagram_model_create(DIAGRAM_FLOWCHART, &model);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    struct mermaid_class parser = {
        .model = model, .outcome = DIAGRAM_ACCEPTED, .open = diagram_none};
    diagram_model_direction(model, DIAGRAM_TOP_BOTTOM);
    parser.outcome = diagram_model_text(model, "", 0, &parser.empty);
    if (parser.outcome == DIAGRAM_ACCEPTED)
        (void)parse_document(&parser, source);
    outcome = parser.outcome;
    if (outcome != DIAGRAM_ACCEPTED)
    {
        diagram_model_destroy(model);
        return outcome;
    }
    diagram_model_seal(model);
    *out = model;
    return DIAGRAM_ACCEPTED;
}
