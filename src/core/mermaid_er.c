/* #882 / ADR0105: Mermaid `erDiagram` read into the flowchart model, so the existing layout and
 * drawing serve it. Entities are boxes whose label lists their attributes; a relationship is a
 * line without arrowheads whose label adds the two cardinalities as text. Quoted entity names,
 * aliases, cardinality words and styles are unsupported and never produce a partial graph. */
#include "mermaid_er.h"
#include "diagram_label.h"
#include "diagram_statement.h"
#include "mermaid_flow.h"
#include <string.h>
struct mermaid_er
{
    struct diagram_model *_Nonnull model;
    enum diagram_outcome outcome;
    size_t empty;
    size_t open; /* the entity whose attribute block is open, or diagram_none */
    size_t used;
    char label[diagram_label_limit];
};
/* The left end is written `|o` ... `}|`, the right end mirrored; both read as the same text. */
static const char *const left_marks[] = {"|o", "||", "}o", "}|"};
static const char *const right_marks[] = {"o|", "||", "o{", "|{"};
static const char *const cardinalities[] = {"0..1", "1", "0..*", "1..*"};
static bool reject(struct mermaid_er *_Nonnull parser, enum diagram_outcome outcome)
{
    parser->outcome = outcome;
    return false;
}
static bool append(struct mermaid_er *_Nonnull parser, const char *_Nonnull bytes, size_t length)
{
    if (length > sizeof parser->label - parser->used)
        return reject(parser, DIAGRAM_TOO_LARGE);
    memcpy(parser->label + parser->used, bytes, length);
    parser->used += length;
    return true;
}
/* An entity is found by name or created as a box labelled by its name. */
static bool entity(struct mermaid_er *_Nonnull parser, struct diagram_text name,
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
static const char *_Nullable mark(struct diagram_text text, const char *const *_Nonnull marks)
{
    for (size_t index = 0; index < 4; ++index)
        if (diagram_text_starts(text, marks[index]))
            return cardinalities[index];
    return nullptr;
}
/* `|o--o{`: two marks around `--` (identifying, solid) or `..` (non-identifying, dashed). */
static bool relation(struct mermaid_er *_Nonnull parser, struct diagram_text *_Nonnull text,
                     struct diagram_link *_Nonnull link)
{
    const char *_Nullable left = mark(*text, left_marks);
    struct diagram_text line = diagram_text_skip(*text, 2);
    const char *_Nullable right = mark(diagram_text_skip(line, 2), right_marks);
    bool solid = diagram_text_starts(line, "--");
    if (left == nullptr || right == nullptr || (!solid && !diagram_text_starts(line, "..")))
        return reject(parser, DIAGRAM_INVALID);
    link->stroke = solid ? DIAGRAM_SOLID : DIAGRAM_DASHED;
    *text = diagram_text_skip(*text, 6);
    parser->used = 0;
    return append(parser, "\n", 1) && append(parser, left, strlen(left)) &&
           append(parser, " : ", 3) && append(parser, right, strlen(right));
}
/* The words after `:`, unquoted; the cardinalities go on a second line. */
static bool relation_label(struct mermaid_er *_Nonnull parser, struct diagram_text text,
                           size_t *_Nonnull label)
{
    text = diagram_text_trim(text);
    if (text.length >= 2 && text.bytes[0] == '"' && text.bytes[text.length - 1] == '"')
        text = (struct diagram_text){text.bytes + 1, text.length - 2};
    size_t marks = parser->used;
    size_t start = text.length == 0 ? 1 : 0; /* no words: no empty first line */
    if (marks + text.length > sizeof parser->label)
        return reject(parser, DIAGRAM_TOO_LARGE);
    memmove(parser->label + text.length, parser->label, marks);
    memcpy(parser->label, text.bytes, text.length);
    parser->outcome = diagram_model_text(parser->model, parser->label + start,
                                         text.length + marks - start, label);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
/* `A ||--o{ B : label`. */
static bool relationship(struct mermaid_er *_Nonnull parser, struct diagram_text name,
                         struct diagram_text rest)
{
    struct diagram_link link = {.label = parser->empty,
                                .length = 1,
                                .start = DIAGRAM_ARROW_NONE,
                                .end = DIAGRAM_ARROW_NONE};
    if (!entity(parser, name, &link.from) || !relation(parser, &rest, &link))
        return false;
    struct diagram_text other = diagram_text_identifier(&rest);
    if (other.length == 0 && rest.length != 0 && rest.bytes[0] == '"')
        return reject(parser, DIAGRAM_UNSUPPORTED);
    if (!entity(parser, other, &link.to))
        return false;
    if (rest.length == 0 || rest.bytes[0] != ':')
        return reject(parser, DIAGRAM_INVALID);
    if (!relation_label(parser, diagram_text_skip(rest, 1), &link.label))
        return false;
    parser->outcome = diagram_model_link(parser->model, link);
    return parser->outcome == DIAGRAM_ACCEPTED;
}
/* `A {` starts the entity's attributes below whatever label it already has. */
static bool open_block(struct mermaid_er *_Nonnull parser, struct diagram_text name)
{
    size_t index = 0;
    if (!entity(parser, name, &index))
        return false;
    const char *_Nonnull shown =
        diagram_model_text_at(parser->model, diagram_model_node_at(parser->model, index).label);
    parser->used = 0;
    parser->open = index;
    return append(parser, shown, strlen(shown));
}
/* `type name [PK, FK] ["comment"]`: one line under the name; the comment is left out. */
static bool attribute(struct mermaid_er *_Nonnull parser, struct diagram_text text)
{
    size_t quote = 0;
    while (quote < text.length && text.bytes[quote] != '"')
        ++quote;
    struct diagram_text shown = diagram_text_trim((struct diagram_text){text.bytes, quote});
    struct diagram_text words = shown;
    (void)diagram_text_word(&words);
    if (diagram_text_word(&words).length == 0)
        return reject(parser, DIAGRAM_INVALID);
    return append(parser, "\n", 1) && append(parser, shown.bytes, shown.length);
}
static bool close_block(struct mermaid_er *_Nonnull parser)
{
    struct diagram_node node = diagram_model_node_at(parser->model, parser->open);
    parser->outcome = diagram_model_text(parser->model, parser->label, parser->used, &node.label);
    if (parser->outcome == DIAGRAM_ACCEPTED)
        parser->outcome = diagram_model_node(parser->model, parser->open, node);
    parser->open = diagram_none;
    return parser->outcome == DIAGRAM_ACCEPTED;
}
static bool unsupported(struct diagram_text text, struct diagram_text word)
{
    static const char *const words[] = {"style", "classDef", "class", "click"};
    for (size_t index = 0; index < sizeof words / sizeof words[0]; ++index)
        if (diagram_text_equal(word, words[index]))
            return true;
    return diagram_text_starts(text, "accTitle") || diagram_text_starts(text, "accDescr") ||
           diagram_text_starts(text, "\"");
}
static bool set_direction(struct mermaid_er *_Nonnull parser, struct diagram_text text)
{
    enum diagram_direction direction = DIAGRAM_TOP_BOTTOM;
    if (!mermaid_flow_direction(diagram_text_trim(text), &direction))
        return reject(parser, DIAGRAM_INVALID);
    diagram_model_direction(parser->model, direction);
    return true;
}
static bool statement(struct mermaid_er *_Nonnull parser, struct diagram_text text)
{
    if (text.length == 0)
        return true;
    if (parser->open != diagram_none)
        return diagram_text_equal(text, "}") ? close_block(parser) : attribute(parser, text);
    struct diagram_text rest = text;
    struct diagram_text word = diagram_text_word(&rest);
    if (unsupported(text, word))
        return reject(parser, DIAGRAM_UNSUPPORTED);
    if (diagram_text_equal(word, "direction"))
        return set_direction(parser, rest);
    rest = text;
    struct diagram_text name = diagram_text_identifier(&rest);
    if (rest.length == 0)
        return entity(parser, name, &(size_t){0});
    if (diagram_text_equal(rest, "{"))
        return open_block(parser, name);
    if (rest.bytes[0] == '[')
        return reject(parser, DIAGRAM_UNSUPPORTED);
    return relationship(parser, name, rest);
}
static bool parse_document(struct mermaid_er *_Nonnull parser, struct diagram_text source)
{
    struct diagram_input input = {.text = source, .kind = DIAGRAM_SEQUENCE};
    struct diagram_statement first;
    parser->outcome = diagram_statement_read(input, 0, &first);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    struct diagram_text header = diagram_text_word(&first.text);
    if (!diagram_text_equal(header, "erDiagram") || first.text.length != 0)
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
enum diagram_outcome mermaid_er_create(struct diagram_text source,
                                       struct diagram_model *_Nullable *_Nonnull out)
{
    struct diagram_model *_Nullable model = nullptr;
    enum diagram_outcome outcome = diagram_model_create(DIAGRAM_FLOWCHART, &model);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    struct mermaid_er parser = {.model = model, .outcome = DIAGRAM_ACCEPTED, .open = diagram_none};
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
