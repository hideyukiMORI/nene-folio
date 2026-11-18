/* #890 / ADR0107: Mermaid `classDiagram` read into the flowchart model, so the existing layout
 * and drawing serve it. A class is a box (its name over its members once it has any); a relation
 * is a line whose ends carry the UML markers and, when written, a multiplicity (#916).
 * Generics written with tildes show in angle brackets (#938). Notes, namespaces and styles are
 * unsupported and never produce a partial graph. */
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
/* #938: a byte that can sit on either side of a generic's opening `~`: ASCII letters and digits,
 * `_`, and every byte of a non-ASCII character. Decided without the locale (ARC-003). */
static bool name_byte(char value)
{
    unsigned char byte = (unsigned char)value;
    return byte >= 0x80 || (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z') ||
           (byte >= '0' && byte <= '9') || byte == '_';
}
/* #938: Mermaid writes generics with tildes, `List~int~` and nested `List~List~int~~`. A `~`
 * between two name bytes opens `<`; while one is open, any other `~` closes `>`. Any other `~`
 * stays, such as the package visibility of `~method()`. */
static char generic_byte(struct diagram_text text, size_t at, size_t *_Nonnull depth)
{
    char value = text.bytes[at];
    if (value != '~')
        return value;
    if (at > 0 && at + 1 < text.length && name_byte(text.bytes[at - 1]) &&
        name_byte(text.bytes[at + 1]))
    {
        ++*depth;
        return '<';
    }
    if (*depth == 0)
        return value;
    --*depth;
    return '>';
}
/* An opening left unclosed is unsupported, never shown half converted. */
static bool append_generic(struct mermaid_class *_Nonnull parser, struct diagram_text text)
{
    size_t depth = 0;
    for (size_t at = 0; at < text.length; ++at)
    {
        char value = generic_byte(text, at, &depth);
        if (!append(parser, &value, 1))
            return false;
    }
    return depth == 0 || reject(parser, DIAGRAM_UNSUPPORTED);
}
/* `Box` also finds the class shown as `Box<T>`: Mermaid names the class without its generic. */
static size_t find_class(const struct diagram_model *_Nonnull model, struct diagram_text name)
{
    size_t index = diagram_model_find_node(model, name.bytes, name.length);
    for (size_t at = 0; index == diagram_none && at < diagram_model_node_count(model); ++at)
    {
        const char *_Nonnull id = diagram_model_text_at(model, diagram_model_node_at(model, at).id);
        if (strncmp(id, name.bytes, name.length) == 0 && id[name.length] == '<')
            index = at;
    }
    return index;
}
/* A class is found by name or created as a box labelled by its name. Quoted names and
 * `Name["shown"]` are not read. */
static bool class_node(struct mermaid_class *_Nonnull parser, struct diagram_text name,
                       size_t *_Nonnull index)
{
    if (name.length == 0)
        return reject(parser, DIAGRAM_INVALID);
    *index = find_class(parser->model, name);
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
/* #938: the class is named `Box<T>` from here on; a plain box shows its name. */
static bool show_generic(struct mermaid_class *_Nonnull parser, struct diagram_text whole,
                         size_t index)
{
    parser->used = 0;
    if (!append_generic(parser, whole))
        return false;
    if (memchr(parser->label, '<', parser->used) == nullptr)
        return reject(parser, DIAGRAM_UNSUPPORTED);
    struct diagram_node node = diagram_model_node_at(parser->model, index);
    parser->outcome = diagram_model_text(parser->model, parser->label, parser->used, &node.id);
    parser->used = 0;
    if (node.shape == DIAGRAM_RECTANGLE)
        node.label = node.id;
    if (parser->outcome == DIAGRAM_ACCEPTED)
        parser->outcome = diagram_model_node(parser->model, index, node);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
/* A class name, with its generic when one follows without a space: `Box~T~` is the class `Box`
 * shown as `Box<T>` (#938). A tilde run that does not end in `~` is unsupported. */
static bool read_class(struct mermaid_class *_Nonnull parser, struct diagram_text *_Nonnull rest,
                       size_t *_Nonnull index)
{
    struct diagram_text start = diagram_text_trim(*rest);
    struct diagram_text name = diagram_text_identifier(rest);
    size_t end = name.length;
    if (end != 0 && end < start.length && start.bytes[end] == '~')
    {
        while (end < start.length && (start.bytes[end] == '~' || name_byte(start.bytes[end])))
            ++end;
        *rest = diagram_text_trim(diagram_text_skip(start, end));
        if (start.bytes[end - 1] != '~')
            return reject(parser, DIAGRAM_UNSUPPORTED);
    }
    if (!class_node(parser, name, index))
        return false;
    return end == name.length ||
           show_generic(parser, (struct diagram_text){start.bytes, end}, *index);
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
        return append_generic(parser, text);
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
    size_t index = 0;
    if (!read_class(parser, &rest, &index))
        return false;
    if (rest.length != 0 && !diagram_text_equal(rest, "{"))
        return reject(parser, DIAGRAM_UNSUPPORTED);
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
/* #916: an optional `"1"` beside an end of the line. Without a quote nothing is read. */
static bool multiplicity(struct mermaid_class *_Nonnull parser, struct diagram_text *_Nonnull rest,
                         size_t *_Nonnull text)
{
    *rest = diagram_text_trim(*rest);
    if (rest->length == 0 || rest->bytes[0] != '"')
        return true;
    size_t close = 1;
    while (close < rest->length && rest->bytes[close] != '"')
        ++close;
    struct diagram_text inside =
        diagram_text_trim((struct diagram_text){rest->bytes + 1, close - 1});
    if (close == rest->length || inside.length == 0)
        return reject(parser, DIAGRAM_INVALID);
    parser->outcome = diagram_label_store(parser->model, inside, text);
    *rest = diagram_text_trim(diagram_text_skip(*rest, close + 1));
    return parser->outcome == DIAGRAM_ACCEPTED;
}
/* `A "1" <|-- "*" B : label`: markers around `--` (solid) or `..` (dashed), each end with an
 * optional multiplicity. */
static bool line_between(struct mermaid_class *_Nonnull parser, struct diagram_text *_Nonnull rest,
                         struct diagram_link *_Nonnull link)
{
    if (!multiplicity(parser, rest, &link->start_text))
        return false;
    link->start = left_marker(rest);
    bool solid = diagram_text_starts(*rest, "--");
    if (!solid && !diagram_text_starts(*rest, ".."))
        return reject(parser, DIAGRAM_INVALID);
    link->stroke = solid ? DIAGRAM_SOLID : DIAGRAM_DASHED;
    *rest = diagram_text_skip(*rest, 2);
    link->end = right_marker(rest);
    return multiplicity(parser, rest, &link->end_text);
}
static bool relation(struct mermaid_class *_Nonnull parser, size_t from, struct diagram_text rest)
{
    struct diagram_link link = {.from = from,
                                .label = parser->empty,
                                .start_text = parser->empty,
                                .end_text = parser->empty,
                                .length = 1};
    if (!line_between(parser, &rest, &link))
        return false;
    struct diagram_text peek = rest;
    if (diagram_text_identifier(&peek).length == 0 && rest.length != 0)
        return reject(parser, DIAGRAM_UNSUPPORTED);
    if (!read_class(parser, &rest, &link.to))
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
    size_t index = 0;
    if (!read_class(parser, &rest, &index))
        return false;
    if (rest.length == 0)
        return true;
    if (rest.bytes[0] != ':')
        return relation(parser, index, rest);
    return begin_members(parser, index) && member(parser, diagram_text_skip(rest, 1)) &&
           end_members(parser);
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
