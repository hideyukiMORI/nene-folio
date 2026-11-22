#include "allocation_probe.h"
#include "diagram_calendar.h"
#include "diagram_geometry.h"
#include "diagram_layout.h"
#include "diagram_palette.h"
#include "diagram_pie_layout.h"
#include "diagram_style.h"
#include "folio_theme.h"
#include "markdown_media.h"
#include "markdown_rtf.h"
#include "mermaid_diagram.h"
#include "mermaid_gantt.h"
#include "mermaid_pie.h"
#include "text_buffer.h"
#include "theme_contrast.h"
#include "ui_font.h"
#include "unit_tests.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const char flow_source[] =
    "flowchart TB\nsubgraph outer [受付]\nsubgraph inner [確認]\n"
    "A[開始] -->|はい| B{判断}\nB -.-> C(再試行)\nC --> A\nend\nD[(保存)]\nend\n"
    "B ==>|完了| D\nD --> D\nD --- E((終わり))\n";
static const char sequence_source[] =
    "sequenceDiagram\ntitle 注文の処理\nautonumber 2 3\nactor User as 利用者\n"
    "participant API as 受付\nparticipant DB as 保存先\nUser->>+API: 注文\n"
    "Note left of User: 入力内容\nloop 3回まで\nAPI->>+DB: 保存\n"
    "alt 成功\nDB-->>-API: 完了\nelse 失敗\nAPI->>API: 再確認\nend\nend\n"
    "Note over API,DB: 確認済み\nNote right of DB: 保存完了\nAPI-->>-User: 結果\n";
static struct note_text *_Nonnull body(const char *_Nonnull source)
{
    struct note_text *_Nullable text = nullptr;
    require(note_text_create(source, strlen(source), &text) == NOTE_TEXT_ACCEPTED,
            "diagram source");
    return text;
}
static struct diagram_model *_Nonnull parsed(const char *_Nonnull source)
{
    struct note_text *text = body(source);
    struct diagram_model *_Nullable model = nullptr;
    enum diagram_outcome outcome = mermaid_diagram_create(text, &model);
    if (outcome != DIAGRAM_ACCEPTED)
        fprintf(stderr, "diagram parse outcome %u: %s\n", (unsigned)outcome, source);
    require(outcome == DIAGRAM_ACCEPTED && model != nullptr, "supported Mermaid parses completely");
    require(strcmp(note_text_bytes(text), source) == 0,
            "parser leaves original Markdown unchanged");
    note_text_destroy(text);
    return model;
}
static struct diagram_size measured(const char *_Nonnull text)
{
    int width = 0;
    int longest = 0;
    int lines = 1;
    for (size_t index = 0; text[index] != '\0'; ++index)
    {
        if (text[index] == '\n')
        {
            if (longest < width)
                longest = width;
            width = 0;
            ++lines;
        }
        else if (((unsigned char)text[index] & 0xC0) != 0x80)
            width += 8;
    }
    return (struct diagram_size){width > longest ? width : longest, lines * 18};
}
static enum diagram_outcome metrics_for(const struct diagram_model *_Nonnull model,
                                        struct diagram_metrics *_Nullable *_Nonnull out)
{
    struct diagram_label_metrics sizes[diagram_text_limit];
    size_t count = diagram_model_text_count(model);
    for (size_t index = 0; index < count; ++index)
    {
        struct diagram_size size = measured(diagram_model_text_at(model, index));
        sizes[index] = (struct diagram_label_metrics){
            .body = size, .caption = {size.width * 13 / 14, size.height * 13 / 14}};
    }
    return diagram_metrics_create(sizes, count, out);
}
static struct diagram_scene *_Nonnull laid_out(const struct diagram_model *_Nonnull model)
{
    struct diagram_metrics *_Nullable metrics = nullptr;
    require(metrics_for(model, &metrics) == DIAGRAM_ACCEPTED, "deterministic measured labels");
    struct diagram_scene *_Nullable scene = nullptr;
    enum diagram_outcome outcome = diagram_layout_create(model, metrics, &scene);
    if (outcome != DIAGRAM_ACCEPTED)
        fprintf(stderr, "diagram layout outcome %u\n", (unsigned)outcome);
    require(outcome == DIAGRAM_ACCEPTED && scene != nullptr,
            "supported diagram has a complete scene");
    struct diagram_size size = diagram_scene_size(scene);
    require(size.width > 0 && size.height > 0 && size.width <= diagram_coordinate_limit &&
                size.height <= diagram_coordinate_limit,
            "scene dimensions are bounded");
    require(diagram_scene_count(scene) != 0, "scene includes visible primitives");
    diagram_metrics_destroy(metrics);
    return scene;
}
static void reject_source(const char *_Nonnull source, enum diagram_outcome expected)
{
    struct note_text *text = body(source);
    struct diagram_model *_Nullable model = nullptr;
    enum diagram_outcome outcome = mermaid_diagram_create(text, &model);
    if (outcome != expected)
        fprintf(stderr, "expected %u, got %u: %s\n", (unsigned)expected, (unsigned)outcome, source);
    require(outcome == expected && model == nullptr, "bad input exposes no partial diagram");
    note_text_destroy(text);
}
static struct diagram_rect label_bounds(const struct diagram_scene *_Nonnull scene,
                                        const struct diagram_model *_Nonnull model,
                                        const char *_Nonnull label)
{
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        if (item->kind == DIAGRAM_PRIMITIVE_TEXT &&
            strcmp(diagram_model_text_at(model, item->text), label) == 0)
            return item->bounds;
    }
    test_failure("expected diagram label is drawn");
}
/* #872 / ADR0104: a state diagram reads into the flowchart model and lays out like one. */
static size_t node_named(const struct diagram_model *_Nonnull model, const char *_Nonnull id)
{
    size_t index = diagram_model_find_node(model, id, strlen(id));
    require(index != diagram_none, "the state is in the model");
    return index;
}

static void state_contract(void)
{
    struct diagram_model *model = parsed("stateDiagram-v2\n"
                                         "    [*] --> Idle\n"
                                         "    Idle --> Running : start\n"
                                         "    state \"実行中\" as Running\n"
                                         "    Running --> Idle\n"
                                         "    Idle : 待機\n"
                                         "    state Check <<choice>>\n"
                                         "    Running --> Check\n"
                                         "    Check --> [*]\n");
    require(diagram_model_kind(model) == DIAGRAM_FLOWCHART &&
                diagram_model_flow(model) == DIAGRAM_TOP_BOTTOM,
            "a state diagram is a top-to-bottom flowchart model");
    struct diagram_node idle = diagram_model_node_at(model, node_named(model, "Idle"));
    struct diagram_node running = diagram_model_node_at(model, node_named(model, "Running"));
    struct diagram_node check = diagram_model_node_at(model, node_named(model, "Check"));
    require(idle.shape == DIAGRAM_ROUNDED &&
                strcmp(diagram_model_text_at(model, idle.label), "待機") == 0,
            "a state is a rounded box shown by its description");
    require(strcmp(diagram_model_text_at(model, running.label), "実行中") == 0,
            "state \"...\" as names the shown text");
    require(check.shape == DIAGRAM_DIAMOND, "a choice is a diamond");
    struct diagram_node start = diagram_model_node_at(model, node_named(model, "[*]<0"));
    struct diagram_node end = diagram_model_node_at(model, node_named(model, "[*]>0"));
    require(start.shape == DIAGRAM_STATE_START && end.shape == DIAGRAM_STATE_END,
            "[*] starts as a dot and ends as a ringed dot (#876)");
    require(diagram_model_link_count(model) == 5, "every transition is a link");
    struct diagram_link labelled = diagram_model_link_at(model, 1);
    require(strcmp(diagram_model_text_at(model, labelled.label), "start") == 0 &&
                labelled.end == DIAGRAM_ARROW_CLOSED,
            "a transition's label and arrow");
    struct diagram_scene *scene = laid_out(model);
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}

static void state_composite(void)
{
    struct diagram_model *model = parsed("stateDiagram\n"
                                         "direction LR\n"
                                         "state Outer {\n"
                                         "    [*] --> Inner\n"
                                         "    Inner --> [*]\n"
                                         "}\n"
                                         "state \"Second box\" as Other {\n"
                                         "    direction LR\n"
                                         "    Deep\n"
                                         "}\n");
    require(diagram_model_flow(model) == DIAGRAM_LEFT_RIGHT, "direction applies to the diagram");
    require(diagram_model_group_count(model) == 2, "each composite state is a group");
    struct diagram_group other = diagram_model_group_at(model, 1);
    require(strcmp(diagram_model_text_at(model, other.label), "Second box") == 0 &&
                other.direction == DIAGRAM_LEFT_RIGHT,
            "a labelled composite records its direction (the layout, like flowchart subgraphs, "
            "does not support one different from the diagram's)");
    struct diagram_node inner = diagram_model_node_at(model, node_named(model, "Inner"));
    require(inner.group == 0, "a state inside a composite belongs to its group");
    require(diagram_model_find_node(model, "[*]<1", 5) != diagram_none &&
                diagram_model_find_node(model, "[*]>1", 5) != diagram_none &&
                diagram_model_find_node(model, "[*]<0", 5) == diagram_none,
            "each composite has its own start and end");
    struct diagram_scene *scene = laid_out(model);
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}

static void state_rejections(void)
{
    reject_source("stateDiagram-v2\nA --> B\nnote right of A : n", DIAGRAM_UNSUPPORTED);
    reject_source("stateDiagram-v2\nstate F <<fork>>", DIAGRAM_UNSUPPORTED);
    reject_source("stateDiagram-v2\nA --> B\n--\nC --> D", DIAGRAM_UNSUPPORTED);
    reject_source("stateDiagram-v2\nclassDef hot fill:#f00\nA --> B", DIAGRAM_UNSUPPORTED);
    reject_source("stateDiagram-v2\nA:::hot --> B", DIAGRAM_UNSUPPORTED);
    reject_source("stateDiagram-v2\nstate S {\nA\n}\nB --> S", DIAGRAM_UNSUPPORTED);
    reject_source("stateDiagram-v2\n[*] --> S\nstate S {\nA\n}", DIAGRAM_UNSUPPORTED);
    reject_source("stateDiagram-v2\nA -> B", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2\nA --> B extra", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2\nstate S {\nA", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2\n}", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2\nstate \"open as A", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2\nstate \"x\" is A", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2\nstate A B", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2\ndirection XY\nA", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2 extra\nA", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2\n", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2\nA B", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2\n --> B", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2\nstate S {\n}\nstate S {\n}", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2\nstate {\n}", DIAGRAM_INVALID);
    reject_source("stateDiagram-v2\nstyle A fill:#f00\nA", DIAGRAM_UNSUPPORTED);
    reject_source("stateDiagram-v2\nclick A call x()\nA", DIAGRAM_UNSUPPORTED);
    reject_source("stateDiagram-v2\naccTitle: t\nA", DIAGRAM_UNSUPPORTED);
    reject_source("stateDiagram-v2\nstate J <<join>>", DIAGRAM_UNSUPPORTED);
    char deep[1024] = "stateDiagram-v2\n";
    for (size_t level = 0; level <= diagram_depth_limit; ++level)
        (void)sprintf_s(deep + strlen(deep), sizeof deep - strlen(deep), "state S%zu {\n", level);
    reject_source(deep, DIAGRAM_TOO_LARGE);
}

/* #876: the start is one small dot filled with the line ink; the end is a small ring around the
 * same dot, whatever the font measures. */
/* Counts one primitive as a dot or a ring, checking its size and fill. */
static void count_marker(const struct diagram_primitive *_Nonnull item, size_t *_Nonnull dots,
                         size_t *_Nonnull rings)
{
    if (item->kind != DIAGRAM_PRIMITIVE_SHAPE)
        return;
    if (item->shape == DIAGRAM_STATE_START)
    {
        require(item->fill == DIAGRAM_INK_ACCENT && item->bounds.width == diagram_state_dot &&
                    item->bounds.height == diagram_state_dot,
                "a dot is small and filled with the line ink");
        ++*dots;
    }
    if (item->shape == DIAGRAM_STATE_END)
    {
        require(item->fill == DIAGRAM_INK_NODE && item->bounds.width == diagram_state_ring,
                "the end ring is small and hollow");
        ++*rings;
    }
}

static void state_dots(void)
{
    struct diagram_model *model = parsed("stateDiagram-v2\n[*] --> A\nA --> [*]\n");
    struct diagram_scene *scene = laid_out(model);
    size_t dots = 0;
    size_t rings = 0;
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
        count_marker(diagram_scene_at(scene, index), &dots, &rings);
    require(dots == 2 && rings == 1, "the start dot, and the end ring with its own dot");
    struct diagram_size text = {400, 200};
    struct diagram_size size = diagram_geometry_node(text, DIAGRAM_STATE_END);
    require(size.width == diagram_state_ring && size.height == diagram_state_ring,
            "the marker size does not follow the text size");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}

/* #882 / ADR0105: an ER diagram reads into the flowchart model: entities are boxes listing
 * their attributes, relationships are lines without arrowheads whose label adds the
 * cardinalities. */
static const char *_Nonnull label_of(const struct diagram_model *_Nonnull model,
                                     const char *_Nonnull id)
{
    return diagram_model_text_at(model, diagram_model_node_at(model, node_named(model, id)).label);
}

/* #888: an entity box is drawn as its name centred in a band, a line, and the attributes
 * left-aligned below; the box holds both. */
static struct diagram_rect text_bounds(const struct diagram_scene *_Nonnull scene, size_t text)
{
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        if (item->kind == DIAGRAM_PRIMITIVE_TEXT && item->text == text)
            return item->bounds;
    }
    test_failure("the text is drawn");
}

static const struct diagram_primitive *_Nonnull first_of(const struct diagram_scene *_Nonnull scene,
                                                         enum diagram_primitive_kind kind)
{
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
        if (diagram_scene_at(scene, index)->kind == kind)
            return diagram_scene_at(scene, index);
    test_failure("the primitive is drawn");
}

static void er_entity_scene(void)
{
    struct diagram_model *model = parsed("erDiagram\nORDER {\n  int id PK\n  string note\n}\n");
    struct diagram_scene *scene = laid_out(model);
    struct diagram_node order = diagram_model_node_at(model, node_named(model, "ORDER"));
    const struct diagram_primitive *shape = first_of(scene, DIAGRAM_PRIMITIVE_SHAPE);
    const struct diagram_primitive *line = first_of(scene, DIAGRAM_PRIMITIVE_PATH);
    struct diagram_rect box = shape->bounds;
    struct diagram_rect name = text_bounds(scene, order.id);
    struct diagram_rect listed = text_bounds(scene, order.label);
    int rule = line->points[0].y;
    require(shape->shape == DIAGRAM_ENTITY && line->count == 2 &&
                line->points[1].x - line->points[0].x == box.width,
            "an entity box with a rule across it");
    require(name.y >= box.y && name.y + name.height <= rule && rule <= listed.y &&
                listed.y + listed.height == box.y + box.height &&
                name.x + name.width / 2 == box.x + box.width / 2,
            "the name is centred above the rule and the attributes fill the rest of the box");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}

static void er_contract(void)
{
    struct diagram_model *model = parsed("erDiagram\n"
                                         "CUSTOMER ||--o{ ORDER : places\n"
                                         "ORDER ||--|{ LINE-ITEM : \"contains\"\n"
                                         "CUSTOMER }|..|| DELIVERY-ADDRESS : uses\n"
                                         "PRODUCT |o--o| CATALOG : \"\"\n"
                                         "CUSTOMER {\n"
                                         "    string name PK \"the shown name\"\n"
                                         "    int age\n"
                                         "}\n"
                                         "LONELY\n");
    require(diagram_model_kind(model) == DIAGRAM_FLOWCHART &&
                diagram_model_flow(model) == DIAGRAM_TOP_BOTTOM,
            "an ER diagram is a top-down flowchart model");
    require(diagram_model_node_count(model) == 7 && diagram_model_link_count(model) == 4,
            "every entity once, every relationship");
    struct diagram_node customer = diagram_model_node_at(model, node_named(model, "CUSTOMER"));
    struct diagram_node lonely = diagram_model_node_at(model, node_named(model, "LONELY"));
    require(customer.shape == DIAGRAM_ENTITY && customer.group == diagram_none &&
                lonely.shape == DIAGRAM_RECTANGLE,
            "an entity with attributes is an entity box, one without a plain box");
    require(strcmp(label_of(model, "CUSTOMER"), "string name PK\nint age") == 0 &&
                strcmp(diagram_model_text_at(model, customer.id), "CUSTOMER") == 0,
            "the label lists the attributes without their comment; the name is the id");
    require(strcmp(label_of(model, "LINE-ITEM"), "LINE-ITEM") == 0, "hyphenated names");
    struct diagram_link places = diagram_model_link_at(model, 0);
    require(places.stroke == DIAGRAM_SOLID && places.start == DIAGRAM_ARROW_ER_ONE &&
                places.end == DIAGRAM_ARROW_ER_ZERO_MANY,
            "an identifying relationship is a solid line; its ends carry the cardinalities (#898)");
    require(strcmp(diagram_model_text_at(model, places.label), "places") == 0,
            "the label is the words alone");
    struct diagram_link contains = diagram_model_link_at(model, 1);
    require(strcmp(diagram_model_text_at(model, contains.label), "contains") == 0 &&
                contains.end == DIAGRAM_ARROW_ER_MANY,
            "quoted words are unquoted; `|{` is one or more");
    struct diagram_link uses = diagram_model_link_at(model, 2);
    require(uses.stroke == DIAGRAM_DASHED && uses.start == DIAGRAM_ARROW_ER_MANY &&
                uses.end == DIAGRAM_ARROW_ER_ONE,
            "a non-identifying relationship is dashed");
    struct diagram_link listed = diagram_model_link_at(model, 3);
    require(diagram_model_text_at(model, listed.label)[0] == '\0' &&
                listed.start == DIAGRAM_ARROW_ER_ZERO_ONE &&
                listed.end == DIAGRAM_ARROW_ER_ZERO_ONE,
            "no words leave the line without a label");
    diagram_model_destroy(model);
    model = parsed("erDiagram\ndirection LR\nA ||--|| B : is\nA {\n  int x\n}\nA {\n  int y\n}\n");
    require(diagram_model_flow(model) == DIAGRAM_LEFT_RIGHT, "direction is kept");
    require(strcmp(label_of(model, "A"), "int x\nint y") == 0,
            "a second block adds to the same entity");
    diagram_model_destroy(model);
    model = parsed("erDiagram\nA ||--o{ B : has\nB {\n}\n");
    require(diagram_model_node_at(model, node_named(model, "B")).shape == DIAGRAM_RECTANGLE,
            "an empty block leaves a plain box");
    diagram_scene_destroy(laid_out(model));
    diagram_model_destroy(model);
    er_entity_scene();
}

static void er_rejections(void)
{
    reject_source("erDiagram\n", DIAGRAM_INVALID);
    reject_source("erDiagram extra\nA", DIAGRAM_INVALID);
    reject_source("erDiagram\nA ||--o{ B", DIAGRAM_INVALID);
    reject_source("erDiagram\nA ||-o{ B : x", DIAGRAM_INVALID);
    reject_source("erDiagram\nA <<--o{ B : x", DIAGRAM_INVALID);
    reject_source("erDiagram\nA ||--<< B : x", DIAGRAM_INVALID);
    reject_source("erDiagram\nA ||==o{ B : x", DIAGRAM_INVALID);
    reject_source("erDiagram\nA ||--o{ : x", DIAGRAM_INVALID);
    reject_source("erDiagram\nA ||--o{ B x", DIAGRAM_INVALID);
    reject_source("erDiagram\nA {\n  int\n}", DIAGRAM_INVALID);
    reject_source("erDiagram\nA {\n  int x\n", DIAGRAM_INVALID);
    reject_source("erDiagram\ndirection SIDEWAYS\nA", DIAGRAM_INVALID);
    reject_source("erDiagram\n\"Quoted name\" ||--o{ B : x", DIAGRAM_UNSUPPORTED);
    reject_source("erDiagram\nA ||--o{ \"Quoted name\" : x", DIAGRAM_UNSUPPORTED);
    reject_source("erDiagram\nA[\"Shown\"]", DIAGRAM_UNSUPPORTED);
    reject_source("erDiagram\nA one or more--many B : x", DIAGRAM_INVALID);
    reject_source("erDiagram\nstyle A fill:#f00\nA", DIAGRAM_UNSUPPORTED);
    reject_source("erDiagram\nclassDef x fill:#f00\nA", DIAGRAM_UNSUPPORTED);
    reject_source("erDiagram\naccTitle: t\nA", DIAGRAM_UNSUPPORTED);
    static char wide[diagram_label_limit * 2] = "erDiagram\nA {\n";
    size_t used = strlen(wide);
    while (used + 16 < sizeof wide - 8)
        used += (size_t)sprintf(wide + used, "  int column%zu\n", used);
    (void)sprintf(wide + used, "}\n");
    reject_source(wide, DIAGRAM_TOO_LARGE);
}

/* #890 / ADR0107: a class diagram reads into the flowchart model: a class is a box (its name
 * over its members once it has any), a relation is a line whose ends carry the UML markers. */
static struct diagram_link class_link(const char *_Nonnull relation)
{
    char source[128];
    (void)snprintf(source, sizeof source, "classDiagram\n%s\n", relation);
    struct diagram_model *model = parsed(source);
    require(diagram_model_link_count(model) == 1 && diagram_model_node_count(model) == 2,
            "one relation between two classes");
    struct diagram_link link = diagram_model_link_at(model, 0);
    require(strcmp(diagram_model_text_at(model, diagram_model_node_at(model, link.from).id), "A") ==
                0,
            "the relation starts at the class written first");
    diagram_model_destroy(model);
    return link;
}

static void expect_relation(const char *_Nonnull relation, enum diagram_arrow start,
                            enum diagram_arrow end, enum diagram_stroke stroke)
{
    struct diagram_link link = class_link(relation);
    if (link.start != start || link.end != end || link.stroke != stroke)
        fprintf(stderr, "relation %s: %u %u %u\n", relation, (unsigned)link.start,
                (unsigned)link.end, (unsigned)link.stroke);
    require(link.start == start && link.end == end && link.stroke == stroke,
            "the relation's markers and line");
}

static void class_relations(void)
{
    expect_relation("A <|-- B", DIAGRAM_ARROW_HOLLOW, DIAGRAM_ARROW_NONE, DIAGRAM_SOLID);
    expect_relation("A --|> B", DIAGRAM_ARROW_NONE, DIAGRAM_ARROW_HOLLOW, DIAGRAM_SOLID);
    expect_relation("A *-- B", DIAGRAM_ARROW_DIAMOND_FILLED, DIAGRAM_ARROW_NONE, DIAGRAM_SOLID);
    expect_relation("A --* B", DIAGRAM_ARROW_NONE, DIAGRAM_ARROW_DIAMOND_FILLED, DIAGRAM_SOLID);
    expect_relation("A o-- B", DIAGRAM_ARROW_DIAMOND, DIAGRAM_ARROW_NONE, DIAGRAM_SOLID);
    expect_relation("A --o B", DIAGRAM_ARROW_NONE, DIAGRAM_ARROW_DIAMOND, DIAGRAM_SOLID);
    expect_relation("A --> B", DIAGRAM_ARROW_NONE, DIAGRAM_ARROW_OPEN, DIAGRAM_SOLID);
    expect_relation("A <-- B", DIAGRAM_ARROW_OPEN, DIAGRAM_ARROW_NONE, DIAGRAM_SOLID);
    expect_relation("A -- B", DIAGRAM_ARROW_NONE, DIAGRAM_ARROW_NONE, DIAGRAM_SOLID);
    expect_relation("A ..> B", DIAGRAM_ARROW_NONE, DIAGRAM_ARROW_OPEN, DIAGRAM_DASHED);
    expect_relation("A ..|> B", DIAGRAM_ARROW_NONE, DIAGRAM_ARROW_HOLLOW, DIAGRAM_DASHED);
    expect_relation("A <|.. B", DIAGRAM_ARROW_HOLLOW, DIAGRAM_ARROW_NONE, DIAGRAM_DASHED);
    expect_relation("A .. B", DIAGRAM_ARROW_NONE, DIAGRAM_ARROW_NONE, DIAGRAM_DASHED);
    /* A class named like a marker right after the line is a class, not a marker. */
    struct diagram_model *model = parsed("classDiagram\nA --order\n");
    require(diagram_model_find_node(model, "order", 5) != diagram_none &&
                diagram_model_link_at(model, 0).end == DIAGRAM_ARROW_NONE,
            "`--order` links to the class `order`");
    diagram_model_destroy(model);
}

/* UML markers that share a class's edge sit side by side; plain arrows keep one shared point. */
static int path_end_x(const struct diagram_scene *_Nonnull scene, size_t which, bool start)
{
    size_t seen = 0;
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        if (item->kind == DIAGRAM_PRIMITIVE_PATH && seen++ == which)
            return item->points[start ? 0 : item->count - 1].x;
    }
    test_failure("the relation is drawn");
}

static void class_marker_spread(void)
{
    struct diagram_model *model = parsed("classDiagram\nA *-- B\nA o-- C\nD --|> A\nE --|> A\n");
    struct diagram_scene *scene = laid_out(model);
    require(path_end_x(scene, 0, true) != path_end_x(scene, 1, true),
            "a composition and an aggregation leave the class side by side");
    require(path_end_x(scene, 2, false) != path_end_x(scene, 3, false),
            "two inheritances reach the class side by side");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
    model = parsed("classDiagram\nA --> B\nA --> C\n");
    scene = laid_out(model);
    require(path_end_x(scene, 0, true) == path_end_x(scene, 1, true),
            "plain arrows still leave from one point");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}

/* #916: a multiplicity is the text beside its end of the line. */
static const struct diagram_primitive *_Nonnull scene_text(
    const struct diagram_scene *_Nonnull scene, size_t text)
{
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        if (item->kind == DIAGRAM_PRIMITIVE_TEXT && item->text == text)
            return item;
    }
    test_failure("the text is drawn");
}

static const struct diagram_primitive *_Nonnull first_path(
    const struct diagram_scene *_Nonnull scene)
{
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
        if (diagram_scene_at(scene, index)->kind == DIAGRAM_PRIMITIVE_PATH)
            return diagram_scene_at(scene, index);
    test_failure("the relation is drawn");
}

static void class_multiplicity_places(struct diagram_model *_Nonnull model)
{
    struct diagram_link link = diagram_model_link_at(model, 0);
    struct diagram_scene *scene = laid_out(model);
    const struct diagram_primitive *start = scene_text(scene, link.start_text);
    const struct diagram_primitive *end = scene_text(scene, link.end_text);
    const struct diagram_primitive *path = first_path(scene);
    struct diagram_point from = path->points[0];
    struct diagram_point to = path->points[path->count - 1];
    require(start->role == DIAGRAM_TEXT_CAPTION && start->bounds.y >= from.y &&
                start->bounds.x > from.x && start->bounds.x - from.x < 32,
            "the first multiplicity sits just beside the line where it leaves the class");
    require(end->bounds.y + end->bounds.height <= to.y && end->bounds.x > to.x &&
                end->bounds.x - to.x < 32,
            "the second multiplicity sits just beside the line where it reaches the class");
    require(start->bounds.y + start->bounds.height <= end->bounds.y,
            "the two multiplicities do not overlap between the rows");
    struct diagram_size size = diagram_scene_size(scene);
    require(end->bounds.x + end->bounds.width <= size.width &&
                start->bounds.x + start->bounds.width <= size.width,
            "the multiplicities stay inside the drawing");
    diagram_scene_destroy(scene);
}

/* Left to right, and on a relation that runs back up: the texts stay inside the drawing. */
static void class_multiplicity_sideways(void)
{
    struct diagram_model *model = parsed("classDiagram\ndirection LR\nA \"1\" --> \"0..*\" B\n"
                                         "B \"many\" --> \"one\" A\n");
    struct diagram_scene *scene = laid_out(model);
    struct diagram_size size = diagram_scene_size(scene);
    for (size_t index = 0; index < 2; ++index)
    {
        struct diagram_link link = diagram_model_link_at(model, index);
        const struct diagram_primitive *ends[] = {scene_text(scene, link.start_text),
                                                  scene_text(scene, link.end_text)};
        for (size_t end = 0; end < 2; ++end)
            require(ends[end]->bounds.x >= 0 && ends[end]->bounds.y >= 0 &&
                        ends[end]->bounds.x + ends[end]->bounds.width <= size.width &&
                        ends[end]->bounds.y + ends[end]->bounds.height <= size.height,
                    "a multiplicity stays inside a sideways drawing");
    }
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}

/* #918: the texts beside the ends are the caption texts without a ground. None of them may
 * overlap another, be crossed by a line, or leave the drawing. */
static bool end_text(const struct diagram_primitive *_Nonnull item)
{
    return item->kind == DIAGRAM_PRIMITIVE_TEXT && item->role == DIAGRAM_TEXT_CAPTION &&
           item->fill == DIAGRAM_INK_NONE;
}

static bool rects_meet(struct diagram_rect a, struct diagram_rect b)
{
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height &&
           b.y < a.y + a.height;
}

static struct diagram_rect segment_rect(struct diagram_point a, struct diagram_point b)
{
    int left = a.x < b.x ? a.x : b.x;
    int top = a.y < b.y ? a.y : b.y;
    return (struct diagram_rect){left, top, a.x + b.x - left * 2 + 1, a.y + b.y - top * 2 + 1};
}

static bool path_meets(const struct diagram_primitive *_Nonnull path, struct diagram_rect text)
{
    for (size_t point = 0; point + 1 < path->count; ++point)
        if (rects_meet(segment_rect(path->points[point], path->points[point + 1]), text))
            return true;
    return false;
}

static bool end_text_clear(const struct diagram_scene *_Nonnull scene, size_t at)
{
    struct diagram_rect text = diagram_scene_at(scene, at)->bounds;
    struct diagram_size size = diagram_scene_size(scene);
    if (text.x < 0 || text.y < 0 || text.x + text.width > size.width ||
        text.y + text.height > size.height)
        return false;
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *other = diagram_scene_at(scene, index);
        if (index != at && end_text(other) && rects_meet(other->bounds, text))
            return false;
        if (other->kind == DIAGRAM_PRIMITIVE_PATH && path_meets(other, text))
            return false;
    }
    return true;
}

static void expect_clear_ends(const char *_Nonnull source, size_t texts)
{
    struct diagram_model *model = parsed(source);
    struct diagram_scene *scene = laid_out(model);
    size_t seen = 0;
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        if (!end_text(diagram_scene_at(scene, index)))
            continue;
        ++seen;
        if (!end_text_clear(scene, index))
            fprintf(stderr, "end text %zu of:\n%s\n", seen, source);
        require(end_text_clear(scene, index),
                "a multiplicity is inside the drawing, off every line and off the others");
    }
    require(seen == texts, "every multiplicity is drawn");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}

/* The inputs the independent review of #916 found (#918), in all four directions. */
static void class_multiplicity_clear(void)
{
    static const char *const directions[] = {"TB", "BT", "LR", "RL"};
    static const char *const bodies[] = {
        "A \"1\" --> \"2\" B\nA \"3\" --> \"4\" B\n",         /* a second line goes round */
        "A \"1\" --> \"0..*\" B\nB \"many\" --> \"one\" A\n", /* a line back up */
        "A --> \"1\" B\nC --> B\nD --> \"2\" B\n",            /* a plain arrow between */
        "A \"0..*\" --> \"1..*\" B\n",                        /* long texts along the flow */
        "A \"1\" --> \"0..many\" A\n",                        /* a class related to itself */
        "A \"1\" --> \"2\" B\nB \"3\" --> \"4\" C\nA \"5\" --> \"6\" C\n", /* a rank skipped */
        "A \"1\" *-- \"2\" B\nA o-- C\nA \"3\" <|-- \"4\" D\nB \"5\" --> \"6\" A\n"};
    static const size_t texts[] = {4, 4, 2, 2, 2, 6, 6};
    for (size_t direction = 0; direction < 4; ++direction)
        for (size_t body = 0; body < sizeof bodies / sizeof bodies[0]; ++body)
        {
            char source[256];
            (void)snprintf(source, sizeof source, "classDiagram\ndirection %s\n%s",
                           directions[direction], bodies[body]);
            expect_clear_ends(source, texts[body]);
        }
}

/* #918: a line without multiplicities reserves nothing for them. An empty text still measures
 * one line high, which a sideways flow once took for a width: the margin grew (#916) and plain
 * arrows were pulled apart. */
static void flow_without_end_texts(void)
{
    struct diagram_model *model = parsed("flowchart LR\nA --> B\nA --> C\n");
    struct diagram_scene *scene = laid_out(model);
    require(path_end_x(scene, 0, true) == path_end_x(scene, 1, true),
            "plain arrows of a sideways flowchart still leave from one point");
    int top = diagram_coordinate_limit;
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        if (item->kind == DIAGRAM_PRIMITIVE_SHAPE && item->bounds.y < top)
            top = item->bounds.y;
    }
    /* The margin has long held half an empty caption's line (16 here); nothing more. */
    if (top != diagram_outer_margin + 8)
        fprintf(stderr, "sideways margin %d\n", top);
    require(top == diagram_outer_margin + 8,
            "a sideways flowchart keeps its old margin beside its nodes");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}

static void class_multiplicity(void)
{
    struct diagram_model *model = parsed("classDiagram\nA \"1\" *-- \" 0..* \" B : has\n");
    struct diagram_link link = diagram_model_link_at(model, 0);
    require(strcmp(diagram_model_text_at(model, link.start_text), "1") == 0 &&
                strcmp(diagram_model_text_at(model, link.end_text), "0..*") == 0 &&
                strcmp(diagram_model_text_at(model, link.label), "has") == 0 &&
                link.start == DIAGRAM_ARROW_DIAMOND_FILLED && link.end == DIAGRAM_ARROW_NONE,
            "both multiplicities, the label and the markers of one relation");
    class_multiplicity_places(model);
    diagram_model_destroy(model);
    model = parsed("classDiagram\nA --> \"*\" B\nC \"1\"..>D\n");
    link = diagram_model_link_at(model, 0);
    require(diagram_model_text_at(model, link.start_text)[0] == '\0' &&
                strcmp(diagram_model_text_at(model, link.end_text), "*") == 0,
            "one end alone may carry a multiplicity");
    link = diagram_model_link_at(model, 1);
    require(strcmp(diagram_model_text_at(model, link.start_text), "1") == 0 &&
                link.stroke == DIAGRAM_DASHED && link.end == DIAGRAM_ARROW_OPEN,
            "a multiplicity written tight against the line");
    diagram_model_destroy(model);
    class_multiplicity_sideways();
    class_multiplicity_clear();
    flow_without_end_texts();
    /* Plain arrows share one point; with a multiplicity each end gets its own place. */
    model = parsed("classDiagram\nA --> \"1\" B\nC --> \"many\" B\n");
    struct diagram_scene *scene = laid_out(model);
    require(path_end_x(scene, 0, false) != path_end_x(scene, 1, false),
            "two ends with multiplicities reach the class side by side");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}

/* #938: `~T~` reads as `<T>` in class names and members; the class keeps its bare name. */
static void class_generics(void)
{
    struct diagram_model *model = parsed("classDiagram\n"
                                         "Shape <|-- Square\n"
                                         "class Square~Shape~ {\n"
                                         "    -List~int~ position\n"
                                         "    +getPoints() List~List~int~~\n"
                                         "    ~reset()\n"
                                         "}\n"
                                         "Square : +id int\n"
                                         "Box~T~ --> Square : holds\n"
                                         "Box ..> Item~日本~\n"
                                         "Vec2~f~ --> Pair_~K2~\n");
    require(diagram_model_node_count(model) == 6 && diagram_model_link_count(model) == 4,
            "a generic class is the same class as its bare name");
    require(node_named(model, "Square<Shape>") != diagram_none &&
                node_named(model, "Box<T>") != diagram_none &&
                node_named(model, "Item<日本>") != diagram_none &&
                node_named(model, "Vec2<f>") != diagram_none &&
                node_named(model, "Pair_<K2>") != diagram_none,
            "class names show their generic in angle brackets, nested and non-ASCII too");
    require(strcmp(label_of(model, "Square<Shape>"), "-List<int> position\n"
                                                     "+getPoints() List<List<int>>\n"
                                                     "~reset()\n"
                                                     "+id int") == 0,
            "members show generics; a leading ~ stays the package visibility");
    require(strcmp(label_of(model, "Box<T>"), "Box<T>") == 0,
            "a plain generic box shows its generic name");
    struct diagram_link holds = diagram_model_link_at(model, 1);
    require(holds.from == node_named(model, "Box<T>") &&
                holds.to == node_named(model, "Square<Shape>") &&
                diagram_model_link_at(model, 2).from == node_named(model, "Box<T>"),
            "relations reach the generic class by either name");
    diagram_scene_destroy(laid_out(model));
    diagram_model_destroy(model);
}

static void class_contract(void)
{
    struct diagram_model *model = parsed("classDiagram\n"
                                         "class Animal {\n"
                                         "    <<abstract>>\n"
                                         "    +String name\n"
                                         "    +eat() void\n"
                                         "}\n"
                                         "class Dog\n"
                                         "Dog : +bark()\n"
                                         "Dog : +fetch()\n"
                                         "Animal <|-- Dog : is a\n"
                                         "Cat\n"
                                         "class Empty {\n"
                                         "}\n");
    require(diagram_model_kind(model) == DIAGRAM_FLOWCHART &&
                diagram_model_flow(model) == DIAGRAM_TOP_BOTTOM,
            "a class diagram is a top-down flowchart model");
    require(diagram_model_node_count(model) == 4 && diagram_model_link_count(model) == 1,
            "every class once");
    struct diagram_node animal = diagram_model_node_at(model, node_named(model, "Animal"));
    require(animal.shape == DIAGRAM_ENTITY &&
                strcmp(label_of(model, "Animal"),
                       "\xC2\xAB"
                       "abstract\xC2\xBB\n+String name\n+eat() void") == 0,
            "a block lists the members under the name; an annotation reads in guillemets");
    require(strcmp(label_of(model, "Dog"), "+bark()\n+fetch()") == 0,
            "`Name : member` lines add to the same class");
    require(diagram_model_node_at(model, node_named(model, "Cat")).shape == DIAGRAM_RECTANGLE &&
                diagram_model_node_at(model, node_named(model, "Empty")).shape == DIAGRAM_RECTANGLE,
            "a class without members is a plain box");
    require(strcmp(diagram_model_text_at(model, diagram_model_link_at(model, 0).label), "is a") ==
                0,
            "a relation keeps its label");
    diagram_scene_destroy(laid_out(model));
    diagram_model_destroy(model);
    model = parsed("classDiagram-v2\ndirection LR\nA --> B\n");
    require(diagram_model_flow(model) == DIAGRAM_LEFT_RIGHT, "direction is kept");
    diagram_model_destroy(model);
    class_relations();
    class_marker_spread();
    class_multiplicity();
    class_generics();
}

/* #955 / ADR0112: a pie is slices as nodes; the shares are sweeps that close the circle. */
static size_t pie_shares(const struct diagram_model *_Nonnull model)
{
    size_t total = 0;
    for (size_t at = 0; at < diagram_model_node_count(model); ++at)
        total += diagram_model_node_at(model, at).share;
    return total;
}

static void pie_contract(void)
{
    struct diagram_model *model = parsed("pie title 内訳\n"
                                         "    \"設計\" : 40\n"
                                         "    \"実装\" : 25.5\n"
                                         "%% a comment\n"
                                         "    \"その他\" : 14.5\n");
    require(diagram_model_kind(model) == DIAGRAM_PIE && diagram_model_node_count(model) == 3 &&
                diagram_model_link_count(model) == 0,
            "a pie is a model of slices");
    require(strcmp(diagram_model_text_at(model, diagram_model_title_text(model)), "内訳") == 0,
            "the header's title is the chart's title");
    struct diagram_node first = diagram_model_node_at(model, 0);
    struct diagram_node last = diagram_model_node_at(model, 2);
    require(first.shape == DIAGRAM_SECTOR && first.share == 18000 &&
                diagram_model_node_at(model, 1).share == 11475 && last.share == 6525,
            "each share is the slice's part of the circle in hundredths of a degree");
    require(pie_shares(model) == mermaid_pie_circle, "the shares close the circle");
    require(strcmp(diagram_model_text_at(model, first.id), "設計") == 0 &&
                strcmp(diagram_model_text_at(model, first.label), "設計 50%") == 0 &&
                strcmp(diagram_model_text_at(model, last.label), "その他 18%") == 0,
            "the legend line is the name and the rounded percentage");
    diagram_model_destroy(model);
    model = parsed("pie showData\ntitle 値つき\n\"a\" : 1\n\"b\" : 2\n\"c\" : 0.001\n");
    require(strcmp(diagram_model_text_at(model, diagram_model_node_at(model, 1).label),
                   "b [2] 67%") == 0 &&
                strcmp(diagram_model_text_at(model, diagram_model_node_at(model, 2).label),
                       "c [0.001] 0%") == 0,
            "showData adds the value as written");
    require(diagram_model_node_at(model, 0).share == 11996 &&
                diagram_model_node_at(model, 2).share == 12 && pie_shares(model) == 36000,
            "the last slice takes what rounding left");
    diagram_model_destroy(model);
    model = parsed("pie\n\"only\" : 3\n");
    require(diagram_model_title_text(model) == diagram_none &&
                diagram_model_node_at(model, 0).share == mermaid_pie_circle,
            "one slice is the whole circle; a title is optional");
    diagram_model_destroy(model);
}

/* The scene: the title, then a sector per slice clockwise from the top, then a swatch and a
 * line per slice. The shades cycle. */
static void pie_layout(void)
{
    struct diagram_model *model = parsed("pie title t\n\"a\" : 1\n\"b\" : 1\n\"c\" : 1\n\"d\" : 1\n"
                                         "\"e\" : 1\n\"f\" : 3\n");
    struct diagram_scene *scene = laid_out(model);
    require(diagram_scene_count(scene) == 1 + 6 + 6 * 2, "title, sectors, swatches and lines");
    int start = 0;
    for (size_t at = 0; at < 6; ++at)
    {
        const struct diagram_primitive *sector = diagram_scene_at(scene, 1 + at);
        require(sector->kind == DIAGRAM_PRIMITIVE_SHAPE && sector->shape == DIAGRAM_SECTOR &&
                    sector->points[0].x == start &&
                    sector->points[0].y == (int)diagram_model_node_at(model, at).share &&
                    sector->bounds.width == sector->bounds.height &&
                    sector->shade == (at == 5 ? 2 : at) && sector->fill == DIAGRAM_INK_ACCENT &&
                    sector->ink == DIAGRAM_INK_BACKGROUND,
                "sectors follow each other clockwise; the last does not repeat the first's shade");
        start += sector->points[0].y;
        const struct diagram_primitive *swatch = diagram_scene_at(scene, 7 + at * 2);
        const struct diagram_primitive *line = diagram_scene_at(scene, 8 + at * 2);
        require(swatch->shape == DIAGRAM_RECTANGLE && swatch->shade == sector->shade &&
                    line->kind == DIAGRAM_PRIMITIVE_TEXT &&
                    line->text == diagram_model_node_at(model, at).label &&
                    swatch->bounds.x > sector->bounds.x + sector->bounds.width &&
                    line->bounds.x > swatch->bounds.x + swatch->bounds.width,
                "each legend row has the slice's shade and its line, right of the circle");
    }
    require(start == 36000, "the sectors end where they began");
    struct diagram_size size = diagram_scene_size(scene);
    const struct diagram_primitive *bottom = diagram_scene_at(scene, 18);
    require(bottom->bounds.y + bottom->bounds.height < size.height &&
                bottom->bounds.x + bottom->bounds.width < size.width,
            "the legend is inside the scene");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}

/* No title, and a legend taller than the circle: the circle starts at the margin and the legend
 * starts level with it. A model that is not a pie is refused. */
static void pie_layout_edges(void)
{
    static char source[32 * 16] = "pie\n";
    size_t used = strlen(source);
    for (size_t at = 0; at < 24; ++at)
        used += (size_t)sprintf(source + used, "\"s%zu\" : 1\n", at);
    struct diagram_model *model = parsed(source);
    struct diagram_scene *scene = laid_out(model);
    require(diagram_scene_count(scene) == 24 * 3, "no title: sectors, swatches and lines only");
    const struct diagram_primitive *sector = diagram_scene_at(scene, 0);
    const struct diagram_primitive *line = diagram_scene_at(scene, 25);
    const struct diagram_primitive *last = diagram_scene_at(scene, 24 * 3 - 1);
    struct diagram_size size = diagram_scene_size(scene);
    require(sector->bounds.y == line->bounds.y &&
                last->bounds.y + last->bounds.height > sector->bounds.y + sector->bounds.height &&
                last->bounds.y + last->bounds.height < size.height,
            "a long legend starts level with the circle and the scene grows to hold it");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
    model = parsed("graph TB\nA --> B");
    struct diagram_metrics *_Nullable metrics = nullptr;
    require(metrics_for(model, &metrics) == DIAGRAM_ACCEPTED, "measured labels");
    struct diagram_scene *_Nullable none = nullptr;
    require(diagram_pie_layout_create(model, metrics, &none) == DIAGRAM_INVALID && none == nullptr,
            "only a pie has a pie layout");
    diagram_metrics_destroy(metrics);
    diagram_model_destroy(model);
}

/* Neighbours never share a shade, the last and the first included, for every count. */
static void pie_neighbours(void)
{
    for (size_t count = 2; count <= 17; ++count)
    {
        char source[32 * 16] = "pie\n";
        size_t used = strlen(source);
        for (size_t at = 0; at < count; ++at)
            used += (size_t)sprintf(source + used, "\"s%zu\" : 1\n", at);
        struct diagram_model *model = parsed(source);
        struct diagram_scene *scene = laid_out(model);
        for (size_t at = 0; at < count; ++at)
            require(diagram_scene_at(scene, at)->shade !=
                        diagram_scene_at(scene, (at + 1) % count)->shade,
                    "neighbouring sectors differ in shade");
        diagram_scene_destroy(scene);
        diagram_model_destroy(model);
    }
}
/* The legend shows the name the way every label is shown; `;` and `%%` in a name are the
 * name's; a title may hold quotes; CRLF sources read the same. */
static void pie_names(void)
{
    struct diagram_model *model = parsed("pie title 5\" disks; \"a;b\" : 1\r\n"
                                         "\"R&amp;D\" : 1\r\n\"100%% sure\" : 2 %% note\r\n");
    require(strcmp(diagram_model_text_at(model, diagram_model_title_text(model)), "5\" disks") ==
                    0 &&
                diagram_model_node_count(model) == 3,
            "a title keeps its quotes; a statement still ends at a semicolon");
    require(strcmp(diagram_model_text_at(model, diagram_model_node_at(model, 0).label),
                   "a;b 25%") == 0 &&
                strcmp(diagram_model_text_at(model, diagram_model_node_at(model, 1).label),
                       "R&D 25%") == 0 &&
                strcmp(diagram_model_text_at(model, diagram_model_node_at(model, 2).label),
                       "100%% sure 50%") == 0,
            "names keep their punctuation and show their entities decoded");
    diagram_model_destroy(model);
    model = parsed("pie title x showData\n\"a\" : 1\n");
    require(
        strcmp(diagram_model_text_at(model, diagram_model_title_text(model)), "x showData") == 0 &&
            strcmp(diagram_model_text_at(model, diagram_model_node_at(model, 0).label), "a 100%") ==
                0,
        "showData after the title is part of the title");
    diagram_model_destroy(model);
    reject_source("pie\n\"a\n\" : 1\n", DIAGRAM_INVALID);
    reject_source("pie\n\"a : 1\n", DIAGRAM_INVALID);
}
static void pie_shades(void)
{
    struct diagram_palette palette = diagram_palette_for(folio_theme_palette(FOLIO_THEME_LIGHT));
    struct rgb_color accent = diagram_palette_ink(&palette, DIAGRAM_INK_ACCENT);
    struct rgb_color full = diagram_palette_shade(&palette, DIAGRAM_INK_ACCENT, 0);
    require(full.red == accent.red && full.green == accent.green && full.blue == accent.blue,
            "the first shade is the ink itself");
    int distance = 0;
    for (unsigned char shade = 1; shade < diagram_shade_count; ++shade)
    {
        struct rgb_color step = diagram_palette_shade(&palette, DIAGRAM_INK_ACCENT, shade);
        int next = abs(step.red - accent.red) + abs(step.green - accent.green) +
                   abs(step.blue - accent.blue);
        require(next > distance, "each shade steps further toward the background");
        distance = next;
    }
    struct rgb_color last =
        diagram_palette_shade(&palette, DIAGRAM_INK_ACCENT, diagram_shade_count - 1);
    struct rgb_color beyond = diagram_palette_shade(&palette, DIAGRAM_INK_ACCENT, 200);
    require(last.red == beyond.red && last.green == beyond.green && last.blue == beyond.blue,
            "a step past the last is the last");
}

/* The least contrast between two shades that can meet: each and the next, the last and the
 * first. */
static double least_neighbour(const struct diagram_palette *_Nonnull palette)
{
    double least = 100.0;
    for (unsigned char shade = 0; shade < diagram_shade_count; ++shade)
    {
        double ratio = theme_contrast_ratio(
            diagram_palette_shade(palette, DIAGRAM_INK_ACCENT, shade),
            diagram_palette_shade(palette, DIAGRAM_INK_ACCENT,
                                  (unsigned char)((shade + 1) % diagram_shade_count)));
        least = ratio < least ? ratio : least;
    }
    return least;
}

/* #967: for every theme, how far the lightest shade is from the background and how far
 * neighbouring shades (the last and the first included) are from each other, as contrast
 * ratios. The floors are what the themes measured on 2026-11-18 (1.32 and 1.18): a theme or a
 * palette change that makes a pie harder to read than today's weakest fails here. */
static void pie_theme_shades(void)
{
    double least_edge = 100.0;
    double least_step = 100.0;
    for (size_t index = 0; index < folio_theme_count(); ++index)
    {
        struct diagram_palette palette =
            diagram_palette_for(folio_theme_palette(folio_theme_at(index)));
        double edge = theme_contrast_ratio(
            diagram_palette_shade(&palette, DIAGRAM_INK_ACCENT, diagram_shade_count - 1),
            palette.background);
        double step = least_neighbour(&palette);
        least_edge = edge < least_edge ? edge : least_edge;
        least_step = step < least_step ? step : least_step;
    }
    printf("pie shades over %zu themes: least last/background %.2f, least neighbour %.2f\n",
           folio_theme_count(), least_edge, least_step);
    require(least_edge >= 1.3 && least_step >= 1.15,
            "no theme's pie is harder to read than the measured floor");
}

static void pie_rejections(void)
{
    reject_source("pie\n", DIAGRAM_INVALID);
    reject_source("pie title only a title\n", DIAGRAM_INVALID);
    reject_source("pie\n\"a\" : 0\n", DIAGRAM_INVALID);
    reject_source("pie\n\"a\" : -1\n", DIAGRAM_INVALID);
    reject_source("pie\n\"a\" : many\n", DIAGRAM_INVALID);
    reject_source("pie\n\"a\" : 1.\n", DIAGRAM_INVALID);
    reject_source("pie\n\"a\" : 1.5x\n", DIAGRAM_INVALID);
    reject_source("pie\n\"a\" : 1234567890\n", DIAGRAM_INVALID);
    reject_source("pie\n\"a\" 1\n", DIAGRAM_INVALID);
    reject_source("pie\n\"a\" :\n", DIAGRAM_INVALID);
    reject_source("pie\n\"\" : 1\n", DIAGRAM_INVALID);
    reject_source("pie\na : 1\n", DIAGRAM_INVALID);
    reject_source("pie title one\ntitle two\n\"a\" : 1\n", DIAGRAM_INVALID);
    reject_source("pie\ntitle\n\"a\" : 1\n", DIAGRAM_INVALID);
    reject_source("pie\naccTitle: t\n\"a\" : 1\n", DIAGRAM_UNSUPPORTED);
    static char many[(mermaid_pie_slice_limit + 2) * 16] = "pie\n";
    size_t used = strlen(many);
    for (size_t at = 0; at <= mermaid_pie_slice_limit; ++at)
        used += (size_t)sprintf(many + used, "\"s%zu\" : 1\n", at);
    reject_source(many, DIAGRAM_TOO_LARGE);
    /* A name that fits a label but leaves no room for the legend line's value and percentage. */
    static char wide[diagram_label_limit + 64] = "pie showData\n\"";
    size_t length = strlen(wide);
    memset(wide + length, 'n', diagram_label_limit - 4);
    static const char rest[] = "\" : 12345\n";
    memcpy(wide + length + diagram_label_limit - 4, rest, sizeof rest);
    reject_source(wide, DIAGRAM_TOO_LARGE);
}

static void pie_tests(void)
{
    pie_contract();
    pie_layout();
    pie_layout_edges();
    pie_neighbours();
    pie_names();
    pie_shades();
    pie_theme_shades();
    pie_rejections();
}

/* #992: day numbers and dates agree both ways, leap years follow the Gregorian rule, and the
 * weekday of a known day is right. */
static void calendar_round_trip(void)
{
    int32_t first = 0;
    int32_t last = 0;
    require(diagram_calendar_day((struct diagram_date){1, 1, 1}, &first) &&
                diagram_calendar_day((struct diagram_date){9999, 12, 31}, &last) && first >= 0,
            "the calendar's first and last days are non-negative day numbers");
    for (int32_t day = first; day <= last; day += 37)
    {
        struct diagram_date date = diagram_calendar_date(day);
        int32_t again = -1;
        require(diagram_calendar_day(date, &again) && again == day,
                "a day number survives a trip through its date");
    }
    struct diagram_date end = diagram_calendar_date(last);
    require(end.year == 9999 && end.month == 12 && end.day == 31, "the last day's date");
    const struct diagram_date valid[] = {{2000, 2, 29}, {2024, 2, 29}, {2026, 12, 31}};
    const struct diagram_date wrong[] = {{1900, 2, 29}, {2026, 2, 29}, {2026, 2, 30},
                                         {2026, 13, 1}, {2026, 0, 10}, {2026, 4, 31},
                                         {0, 1, 1},     {10000, 1, 1}};
    int32_t day = 0;
    for (size_t at = 0; at < sizeof valid / sizeof valid[0]; ++at)
        require(diagram_calendar_day(valid[at], &day), "existing dates are days");
    for (size_t at = 0; at < sizeof wrong / sizeof wrong[0]; ++at)
        require(!diagram_calendar_day(wrong[at], &day), "dates that do not exist are refused");
    int32_t thursday = 0;
    int32_t epoch = 0;
    int32_t tuesday = 0;
    require(diagram_calendar_day((struct diagram_date){2026, 11, 19}, &thursday) &&
                diagram_calendar_day((struct diagram_date){1970, 1, 1}, &epoch) &&
                diagram_calendar_day((struct diagram_date){2000, 2, 29}, &tuesday) &&
                diagram_calendar_weekday(thursday) == 3 && diagram_calendar_weekday(epoch) == 3 &&
                diagram_calendar_weekday(tuesday) == 1 && thursday - epoch == 20776,
            "weekdays count from Monday");
}

static const char gantt_source[] = "gantt\n"
                                   "    title 計画\n"
                                   "    dateFormat YYYY-MM-DD\n"
                                   "    %% a comment\n"
                                   "    section 設計\n"
                                   "    要件 :done, a1, 2026-11-02, 3d\n"
                                   "    画面 :active, a2, after a1, 1w\n"
                                   "    section 実装\n"
                                   "    中核 :crit, b1, after a2, 2026-11-20\n"
                                   "    確認 :milestone, m1, after b1, 0d\n";

/* A task's expected days and state. */
struct gantt_expected
{
    size_t start;
    size_t length;
    enum diagram_task_state state;
};

static void expect_task(const struct diagram_model *_Nonnull model, size_t at,
                        struct gantt_expected expected)
{
    size_t start = expected.start;
    size_t length = expected.length;
    enum diagram_task_state state = expected.state;
    struct diagram_node node = diagram_model_node_at(model, at);
    if (node.start != start || node.length != length || node.state != state)
        fprintf(stderr, "task %zu: start %zu length %zu state %u\n", at, node.start, node.length,
                (unsigned)node.state);
    require(node.start == start && node.length == length && node.state == state,
            "a task's days and state");
}

static void expect_ticks(const struct diagram_model *_Nonnull model,
                         const char *_Nonnull const *_Nonnull labels, size_t count)
{
    if (diagram_model_tick_count(model) != count)
        fprintf(stderr, "ticks: %zu\n", diagram_model_tick_count(model));
    require(diagram_model_tick_count(model) == count, "the axis has its marks");
    for (size_t at = 0; at < count; ++at)
    {
        const char *label = diagram_model_text_at(model, diagram_model_tick_at(model, at).label);
        if (strcmp(label, labels[at]) != 0)
            fprintf(stderr, "tick %zu: %s, expected %s\n", at, label, labels[at]);
        require(strcmp(label, labels[at]) == 0, "each mark has its label");
    }
}

static void gantt_contract(void)
{
    struct diagram_model *model = parsed(gantt_source);
    require(diagram_model_kind(model) == DIAGRAM_GANTT && diagram_model_node_count(model) == 4 &&
                diagram_model_group_count(model) == 2 && diagram_model_link_count(model) == 0,
            "a gantt is a model of tasks in sections");
    require(strcmp(diagram_model_text_at(model, diagram_model_title_text(model)), "計画") == 0,
            "the title");
    expect_task(model, 0, (struct gantt_expected){0, 3, DIAGRAM_TASK_DONE});
    expect_task(model, 1, (struct gantt_expected){3, 7, DIAGRAM_TASK_ACTIVE});
    expect_task(model, 2, (struct gantt_expected){10, 8, DIAGRAM_TASK_PLANNED});
    expect_task(model, 3, (struct gantt_expected){18, 0, DIAGRAM_TASK_PLANNED});
    struct diagram_node core = diagram_model_node_at(model, 2);
    struct diagram_node check = diagram_model_node_at(model, 3);
    require(core.critical && !diagram_model_node_at(model, 0).critical &&
                core.shape == DIAGRAM_RECTANGLE && check.shape == DIAGRAM_DIAMOND,
            "crit and milestone");
    require(diagram_model_node_at(model, 0).group == 0 && core.group == 1 &&
                strcmp(diagram_model_text_at(model, core.label), "中核") == 0 &&
                strcmp(diagram_model_text_at(model, core.id), "b1") == 0 &&
                strcmp(diagram_model_text_at(model, diagram_model_group_at(model, 1).label),
                       "実装") == 0,
            "names, ids and sections");
    static const char *const weeks[] = {"2026/11/2", "11/9", "11/16"};
    expect_ticks(model, weeks, 3);
    require(diagram_model_tick_at(model, 1).day == 7, "a week's mark is seven days on");
    diagram_model_destroy(model);
}

/* Durations skip excluded days, a written end date does not move, a task without a start
 * follows the one above, and `after` may name a task further down. */
static void gantt_schedule(void)
{
    struct diagram_model *model = parsed("gantt\nexcludes weekends\n"
                                         "a :x, 2026-11-06, 2d\n"
                                         "b :3d\n"
                                         "c :after d, 1d\n"
                                         "d :d, 2026-11-20, 2026-11-23\n");
    expect_task(model, 0, (struct gantt_expected){0, 4, DIAGRAM_TASK_PLANNED});
    expect_task(model, 1, (struct gantt_expected){4, 3, DIAGRAM_TASK_PLANNED});
    expect_task(model, 2, (struct gantt_expected){17, 1, DIAGRAM_TASK_PLANNED});
    expect_task(model, 3, (struct gantt_expected){14, 3, DIAGRAM_TASK_PLANNED});
    require(diagram_model_node_at(model, 1).group == diagram_none, "no section before the first");
    diagram_model_destroy(model);
    model = parsed("gantt\nexcludes 2026-11-03, 2026-11-04\nx :2026-11-02, 3d\n");
    expect_task(model, 0, (struct gantt_expected){0, 5, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    model = parsed("gantt\nx :a, 2026-11-02, 2d\ny :b, 2026-11-03, 4d\nz :after a b, 1d\n");
    expect_task(model, 2, (struct gantt_expected){5, 1, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
}

/* #1036: marks every k days or weeks fall where d3's `every(k)` puts them, wherever the chart
 * begins: the weeks counted from the one that holds 1970-01-01, the days by their date in the
 * month. 2026-11-02 is a Monday in an even week, 2026-11-09 in an odd one. */
static void expect_marks(const char *_Nonnull source, const char *_Nonnull const *_Nonnull labels,
                         size_t count)
{
    struct diagram_model *model = parsed(source);
    expect_ticks(model, labels, count);
    diagram_model_destroy(model);
}

/* #1044: marks every k days are never closer than k days, so a month's end does not widen
 * the whole chart. */
static void expect_spacing(const char *_Nonnull source, int32_t days)
{
    struct diagram_model *model = parsed(source);
    size_t count = diagram_model_tick_count(model);
    require(count > 10, "the chart has many marks");
    for (size_t at = 1; at < count; ++at)
    {
        size_t gap =
            diagram_model_tick_at(model, at).day - diagram_model_tick_at(model, at - 1).day;
        if (gap < (size_t)days)
            fprintf(stderr, "marks %zu and %zu are %zu days apart\n", at - 1, at, gap);
        require(gap >= (size_t)days, "marks every k days are at least k days apart");
    }
    diagram_model_destroy(model);
}

static void gantt_every(void)
{
    static const char *const even[] = {"2026/11/2", "11/16", "11/30"};
    expect_marks("gantt\ntickInterval 2week\na :2026-11-02, 4w\n", even, 3);
    static const char *const later[] = {"2026/11/16", "11/30"};
    expect_marks("gantt\ntickInterval 2week\na :2026-11-09, 3w\n", later, 2);
    static const char *const third[] = {"2026/11/9", "11/30"};
    expect_marks("gantt\ntickInterval 3week\na :2026-11-02, 4w\n", third, 2);
    static const char *const sundays[] = {"2026/11/1", "11/15"};
    expect_marks("gantt\nweekday sunday\ntickInterval 2week\na :2026-11-01, 2w\n", sundays, 2);
    static const char *const before[] = {"1950/1/9"};
    expect_marks("gantt\ntickInterval 2week\na :1950-01-02, 2w\n", before, 1);
    /* #1044: the 31st is left out, the 1st follows it the next day. */
    static const char *const dates[] = {"2026/10/29", "11/1", "11/3"};
    expect_marks("gantt\ntickInterval 2day\na :2026-10-29, 2026-11-04\n", dates, 3);
    static const char *const leap[] = {"2028/2/27", "3/1"};
    expect_marks("gantt\ntickInterval 2day\na :2028-02-26, 2028-03-02\n", leap, 2);
    expect_spacing("gantt\ntickInterval 2day\na :2026-10-20, 2027-01-20\n", 2);
    expect_spacing("gantt\ntickInterval 3day\na :2026-10-20, 2027-03-20\n", 3);
    expect_spacing("gantt\ntickInterval 7day\na :2026-01-20, 2027-01-20\n", 7);
    static const char *const first[] = {"2026/11/1"};
    expect_marks("gantt\ntickInterval 40day\na :2026-10-29, 2026-11-04\n", first, 1);
}

static void gantt_axis(void)
{
    struct diagram_model *model = parsed("gantt\na :2026-12-21, 3w\n");
    static const char *const new_year[] = {"2026/12/21", "12/28", "2027/1/4", "1/11"};
    expect_ticks(model, new_year, 4);
    diagram_model_destroy(model);
    model = parsed("gantt\na :2026-01-15, 2026-12-01\n");
    static const char *const quarters[] = {"2026/4/1", "7/1", "10/1"};
    expect_ticks(model, quarters, 3);
    diagram_model_destroy(model);
    model = parsed("gantt\naxisFormat %-m月%-d日\ntickInterval 2day\na :2026-11-02, 4d\n");
    static const char *const written[] = {"11月3日", "11月5日"};
    expect_ticks(model, written, 2); /* #1036: the odd dates, as d3's every(2) */
    diagram_model_destroy(model);
    /* `%%` starts a Mermaid comment anywhere on a line, so it never reaches axisFormat. */
    model = parsed("gantt\naxisFormat %Y-%m-%d %y %e%% a comment\ntickInterval 1month\n"
                   "a :2026-11-02, 2027-01-10\n");
    static const char *const months[] = {"2026-12-01 26 1", "2027-01-01 27 1"};
    expect_ticks(model, months, 2);
    diagram_model_destroy(model);
    /* #1030: week marks on the written weekday (2026-12-21 is a Monday). */
    model = parsed("gantt\nWeekday Sunday\na :2026-12-21, 3w\n");
    static const char *const sundays[] = {"2026/12/27", "2027/1/3", "1/10"};
    expect_ticks(model, sundays, 3);
    diagram_model_destroy(model);
    model = parsed("gantt\nweekday monday\ntickInterval 1week\na :2026-12-22, 2w\n");
    static const char *const mondays[] = {"2026/12/28", "2027/1/4"};
    expect_ticks(model, mondays, 2);
    diagram_model_destroy(model);
    model = parsed("gantt\ntodayMarker off\na :milestone, 2026-11-04, 0d\n");
    static const char *const one[] = {"2026/11/4", "11/5"};
    expect_ticks(model, one, 2);
    diagram_model_destroy(model);
}

static void gantt_rejections(void)
{
    static const struct
    {
        const char *_Nonnull source;
        enum diagram_outcome outcome;
    } cases[] = {
        {"gantt\n", DIAGRAM_INVALID},
        {"gantt extra\na :2026-11-01, 1d\n", DIAGRAM_INVALID},
        {"gantt\na :3d\n", DIAGRAM_UNSUPPORTED},
        {"gantt\na :after x, 1d\n", DIAGRAM_UNSUPPORTED},
        {"gantt\na :a, after b, 1d\nb :b, after a, 1d\n", DIAGRAM_INVALID},
        {"gantt\na :2026-02-30, 1d\n", DIAGRAM_INVALID},
        {"gantt\na :2026-11-01, 2026-02-30\n", DIAGRAM_INVALID},
        {"gantt\na :2026-11-05, 2026-11-01\n", DIAGRAM_INVALID},
        {"gantt\na :x, 2026-11-01, 1d\nb :x, 2026-11-02, 1d\n", DIAGRAM_INVALID},
        {"gantt\n : 2026-11-01, 1d\n", DIAGRAM_INVALID},
        {"gantt\na 2026-11-01\n", DIAGRAM_INVALID},
        {"gantt\na :\n", DIAGRAM_INVALID},
        {"gantt\na :x, y, 2026-11-01, 1d\n", DIAGRAM_INVALID},
        {"gantt\na :2026-11-01, 3\n", DIAGRAM_INVALID},
        {"gantt\na :2026-11-01, soon\n", DIAGRAM_INVALID},
        {"gantt\na :, 2026-11-01, 1d\n", DIAGRAM_INVALID},
        {"gantt\ntitle a\ntitle b\nx :2026-11-01, 1d\n", DIAGRAM_INVALID},
        {"gantt\nsection\nx :2026-11-01, 1d\n", DIAGRAM_INVALID},
        {"gantt\naxisFormat\nx :2026-11-01, 1d\n", DIAGRAM_INVALID},
        {"gantt\ntickInterval 0day\nx :2026-11-01, 1d\n", DIAGRAM_INVALID},
        {"gantt\ndateFormat DD/MM/YYYY\nx :01/11/2026, 1d\n", DIAGRAM_UNSUPPORTED},
        {"gantt\nx :2026-11-01, 1h\n", DIAGRAM_UNSUPPORTED},
        {"gantt\nx :2026-11-01, 1.5d\n", DIAGRAM_UNSUPPORTED},
        {"gantt\nx :2026-11-01, 2ms\n", DIAGRAM_UNSUPPORTED},
        {"gantt\nx :2026-11-01 10:00, 1d\n", DIAGRAM_UNSUPPORTED},
        {"gantt\nx :2026-11-01, until y\n", DIAGRAM_UNSUPPORTED},
        {"gantt\naxisFormat %b %d\nx :2026-11-01, 1d\n", DIAGRAM_UNSUPPORTED},
        {"gantt\naxisFormat %-Y\nx :2026-11-01, 1d\n", DIAGRAM_UNSUPPORTED},
        {"gantt\naxisFormat %-y\nx :2026-11-01, 1d\n", DIAGRAM_UNSUPPORTED},
        {"gantt\naxisFormat %-e\nx :2026-11-01, 1d\n", DIAGRAM_UNSUPPORTED},
        {"gantt\naxisFormat %d%-\nx :2026-11-01, 1d\n", DIAGRAM_UNSUPPORTED},
        {"gantt\naxisFormat 50%\nx :2026-11-01, 1d\n", DIAGRAM_UNSUPPORTED},
        {"gantt\nexcludes 2026-02-30\nx :2026-11-01, 1d\n", DIAGRAM_INVALID},
        {"gantt\ntickInterval 1hour\nx :2026-11-01, 1d\n", DIAGRAM_UNSUPPORTED},
        {"gantt\nweekend sunday\nx :2026-11-01, 1d\n", DIAGRAM_INVALID},
        {"gantt\nweekday\nx :2026-11-01, 1d\n", DIAGRAM_INVALID},
        {"gantt\nweekday someday\nx :2026-11-01, 1d\n", DIAGRAM_INVALID},
        {"gantt\nx :a, 2026-11-01, 1d\nclick a href \"https://example.com\"\n",
         DIAGRAM_UNSUPPORTED},
        {"gantt\naccTitle: t\nx :2026-11-01, 1d\n", DIAGRAM_UNSUPPORTED},
        {"gantt\nx :2020-01-01, 2031-01-01\n", DIAGRAM_TOO_LARGE},
        {"gantt\nx :2026-11-01, 99999d\n", DIAGRAM_TOO_LARGE},
        {"gantt\nx :2026-11-01, 100000d\n", DIAGRAM_TOO_LARGE},
        {"gantt\nx :9999-12-20, 30d\n", DIAGRAM_TOO_LARGE},
        {"gantt\ntickInterval 1day\nx :2026-11-01, 100d\n", DIAGRAM_TOO_LARGE},
    };
    for (size_t at = 0; at < sizeof cases / sizeof cases[0]; ++at)
        reject_source(cases[at].source, cases[at].outcome);
    static char many[(mermaid_gantt_task_limit + 2) * 32] = "gantt\nt0 :2026-11-02, 1d\n";
    size_t used = strlen(many);
    for (size_t at = 1; at <= mermaid_gantt_task_limit; ++at)
        used += (size_t)sprintf(many + used, "t%zu :1d\n", at);
    reject_source(many, DIAGRAM_TOO_LARGE);
}

/* The scene, in order: each mark's grid line and label, each section's name, each task's bar and
 * name, the title. */
static void gantt_layout(void)
{
    struct diagram_model *model = parsed(gantt_source);
    struct diagram_scene *scene = laid_out(model);
    require(diagram_scene_count(scene) == 3 * 2 + 2 + 4 * 2 + 1,
            "marks, sections, tasks and the title");
    const struct diagram_primitive *done = diagram_scene_at(scene, 8);
    const struct diagram_primitive *active = diagram_scene_at(scene, 10);
    const struct diagram_primitive *critical = diagram_scene_at(scene, 12);
    const struct diagram_primitive *milestone = diagram_scene_at(scene, 14);
    require(done->shape == DIAGRAM_RECTANGLE && done->fill == DIAGRAM_INK_NODE &&
                done->ink == DIAGRAM_INK_MUTED && done->stroke == DIAGRAM_SOLID,
            "a done task is pale with a muted edge");
    require(active->fill == DIAGRAM_INK_ACCENT && active->shade == 1 &&
                active->stroke == DIAGRAM_SOLID && active->ink == DIAGRAM_INK_ACCENT,
            "an active task has its section's step and a thin accent edge");
    require(critical->shade == 2 && critical->stroke == DIAGRAM_THICK &&
                critical->ink == DIAGRAM_INK_ACCENT,
            "the next section takes the next step; crit has a thick accent edge");
    require(milestone->shape == DIAGRAM_DIAMOND && milestone->shade == 0 &&
                milestone->stroke == DIAGRAM_INVISIBLE &&
                milestone->bounds.x + milestone->bounds.width / 2 ==
                    critical->bounds.x + critical->bounds.width,
            "a milestone is a diamond at its day");
    require(active->bounds.x == done->bounds.x + done->bounds.width &&
                critical->bounds.x == active->bounds.x + active->bounds.width &&
                active->bounds.y > done->bounds.y && critical->bounds.y > active->bounds.y,
            "bars follow their days and their rows");
    const struct diagram_primitive *label = diagram_scene_at(scene, 9);
    const struct diagram_primitive *grid = diagram_scene_at(scene, 2);
    require(label->kind == DIAGRAM_PRIMITIVE_TEXT &&
                label->bounds.x > done->bounds.x + done->bounds.width &&
                grid->kind == DIAGRAM_PRIMITIVE_PATH && grid->ink == DIAGRAM_INK_NODE &&
                grid->points[0].x > active->bounds.x && grid->points[0].x < critical->bounds.x &&
                grid->points[1].y >= milestone->bounds.y + milestone->bounds.height,
            "names stand right of their bars; grid lines run through every row");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}

/* #992: in every theme, how far the palest section step is from the background, how far
 * neighbouring section steps are from each other, how far a done task's edge is from its fill,
 * and how far a crit edge is from the first step. Printed; the floors are what the themes
 * measured on 2026-11-19. */
/* The four ratios of one theme: palest step on the background, the weakest pair of neighbouring
 * steps, a done task's edge on its fill, a crit edge on the first step. */
struct gantt_contrast
{
    double edge;
    double step;
    double done;
    double crit;
};

static struct gantt_contrast gantt_theme(const struct diagram_palette *_Nonnull palette)
{
    struct rgb_color steps[3];
    for (unsigned char at = 0; at < 3; ++at)
        steps[at] = diagram_palette_shade(palette, DIAGRAM_INK_ACCENT, (unsigned char)(at + 1));
    struct gantt_contrast measured = {.edge = theme_contrast_ratio(steps[2], palette->background),
                                      .step = 100.0,
                                      .done = theme_contrast_ratio(palette->muted, palette->node),
                                      .crit = theme_contrast_ratio(palette->accent, steps[0])};
    for (size_t at = 0; at < 3; ++at)
    {
        double step = theme_contrast_ratio(steps[at], steps[(at + 1) % 3]);
        measured.step = step < measured.step ? step : measured.step;
    }
    return measured;
}

static double least(double first, double second)
{
    return first < second ? first : second;
}

static void gantt_theme_contrast(void)
{
    struct gantt_contrast floor = {100.0, 100.0, 100.0, 100.0};
    for (size_t index = 0; index < folio_theme_count(); ++index)
    {
        struct diagram_palette palette =
            diagram_palette_for(folio_theme_palette(folio_theme_at(index)));
        struct gantt_contrast measured = gantt_theme(&palette);
        floor = (struct gantt_contrast){
            least(floor.edge, measured.edge), least(floor.step, measured.step),
            least(floor.done, measured.done), least(floor.crit, measured.crit)};
    }
    printf("gantt over %zu themes: least step 3/background %.3f, neighbour %.3f, done edge %.3f, "
           "crit edge %.3f\n",
           folio_theme_count(), floor.edge, floor.step, floor.done, floor.crit);
    /* Measured 2026-11-19 over 127 themes: 1.595, 1.316, 3.302, 1.377. */
    require(floor.edge >= 1.59 && floor.step >= 1.31 && floor.done >= 3.30 && floor.crit >= 1.37,
            "no theme's gantt is harder to read than the measured floor");
}

/* The calendar's last months: month marks stop at 9999 instead of wrapping (a chart there once
 * never finished), and a task may end on 9999-12-31. */
static void gantt_year_end(void)
{
    const char *const sources[] = {
        "gantt\na :9999-11-15, 10d\n", "gantt\na :milestone, 9999-12-31, 0d\n",
        "gantt\nx :9999-12-31, 1d\n", "gantt\ntickInterval 1month\nx :9999-10-20, 9999-12-30\n",
        "gantt\ntickInterval 12month\nx :9999-06-20, 9999-12-30\n"};
    for (size_t at = 0; at < sizeof sources / sizeof sources[0]; ++at)
    {
        struct diagram_model *model = parsed(sources[at]);
        require(diagram_model_tick_count(model) >= 1, "a chart at the calendar's end has marks");
        struct diagram_scene *scene = laid_out(model);
        diagram_scene_destroy(scene);
        diagram_model_destroy(model);
    }
}

/* A duration that ends before excluded days draws to its last working day; the task after it
 * starts after them (Mermaid's renderEndTime and endTime). */
static void gantt_excluded_end(void)
{
    struct diagram_model *model = parsed("gantt\nexcludes weekends\na :2026-11-06, 1d\nb :1d\n");
    expect_task(model, 0, (struct gantt_expected){0, 1, DIAGRAM_TASK_PLANNED});
    expect_task(model, 1, (struct gantt_expected){3, 1, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    model = parsed("gantt\nexcludes 2026-11-03,2026-11-04\nx :2026-11-02, 3d\n");
    expect_task(model, 0, (struct gantt_expected){0, 5, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
}

/* The upper limits are accepted and laid out: 128 tasks, a chart of 3660 days; a chart shorter
 * than a month's interval still has its one mark. */
static void gantt_limits(void)
{
    static char many[mermaid_gantt_task_limit * 24] = "gantt\nt0 :2026-11-02, 1d\n";
    size_t used = strlen(many);
    for (size_t at = 1; at < mermaid_gantt_task_limit; ++at)
        used += (size_t)sprintf(many + used, "t%zu :1d\n", at);
    const char *const sources[] = {many, "gantt\nx :2020-01-01, 2030-01-08\n"};
    for (size_t at = 0; at < 2; ++at)
    {
        struct diagram_model *model = parsed(sources[at]);
        struct diagram_scene *scene = laid_out(model);
        diagram_scene_destroy(scene);
        diagram_model_destroy(model);
    }
    struct diagram_model *model = parsed("gantt\ntickInterval 1month\na :2026-11-10, 5d\n");
    static const char *const one[] = {"2026/11/10"};
    expect_ticks(model, one, 1);
    diagram_model_destroy(model);
    reject_source("gantt\ntickInterval 99999999999day\nx :2026-11-01, 1d\n", DIAGRAM_INVALID);
    reject_source("gantt\ntickInterval 01day\nx :2026-11-01, 1d\n", DIAGRAM_INVALID);
    reject_source("gantt\nx :vert, v1, 2026-11-01, 0d\n", DIAGRAM_UNSUPPORTED);
    reject_source("gantt\naxisFormat %Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y%Y\nx :2026-11-01, 1d\n",
                  DIAGRAM_TOO_LARGE);
}

/* The bounds of the text primitive that draws a model text. */
static struct diagram_rect text_at(const struct diagram_scene *_Nonnull scene, size_t text)
{
    for (size_t at = 0; at < diagram_scene_count(scene); ++at)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, at);
        if (item->kind == DIAGRAM_PRIMITIVE_TEXT && item->text == text)
            return item->bounds;
    }
    require(false, "the text is drawn");
    return (struct diagram_rect){0};
}

/* #1006: a section name taller than its rows pushes the next section down, so the names never
 * overlap; the next section's bar starts below the long name too. */
static void gantt_section_height(void)
{
    struct diagram_model *model = parsed("gantt\nsection "
                                         "長い<br>区分の<br>名前は<br>行より高い\n"
                                         "a :2026-11-02, 2d\nsection 次\nb :2d\n");
    struct diagram_scene *scene = laid_out(model);
    struct diagram_rect first = text_at(scene, diagram_model_group_at(model, 0).label);
    struct diagram_rect second = text_at(scene, diagram_model_group_at(model, 1).label);
    struct diagram_rect bar = {0};
    for (size_t at = 0; at < diagram_scene_count(scene); ++at)
        if (diagram_scene_at(scene, at)->kind == DIAGRAM_PRIMITIVE_SHAPE)
            bar = diagram_scene_at(scene, at)->bounds; /* the last shape is b's bar */
    printf("gantt long section: first %d..%d, second at %d, bar at %d\n", first.y,
           first.y + first.height, second.y, bar.y);
    require(first.height > 40 && second.y >= first.y + first.height && bar.y >= second.y,
            "a tall section name pushes the next section below it");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}

/* #1002: directive words in any case, as Mermaid's gantt lexer reads them. */
static void gantt_keyword_case(void)
{
    struct diagram_model *model =
        parsed("gantt\nTITLE 題\nDateFormat YYYY-MM-DD\nEXCLUDES Weekends\nTodayMarker off\n"
               "Section 区分\nx :2026-11-06, 1d\ny :1d\n");
    require(diagram_model_group_count(model) == 1 &&
                diagram_model_title_text(model) != diagram_none &&
                diagram_model_node_at(model, 0).group == 0,
            "Section and TITLE are directives in any case");
    expect_task(model, 1, (struct gantt_expected){3, 1, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    reject_source("gantt\nClick x call f()\nx :2026-11-02, 1d\n", DIAGRAM_UNSUPPORTED);
    /* Task tags stay as written, as in Mermaid: `DONE` is the task's id, not its state. */
    model = parsed("gantt\nx :DONE, 2026-11-02, 1d\n");
    require(diagram_model_node_at(model, 0).state == DIAGRAM_TASK_PLANNED &&
                strcmp(diagram_model_text_at(model, diagram_model_node_at(model, 0).id), "DONE") ==
                    0,
            "a tag in capitals is an id");
    diagram_model_destroy(model);
}

/* #1022 review: the day a task begins on is counted even when excluded, a task of no days
 * ends where it begins, and an excluded target day ends the bar there while the next task
 * begins after the excluded days (Mermaid's checkTaskDates). 2026-11-07 is a Saturday. */
static void gantt_until_excluded(void)
{
    struct diagram_model *model =
        parsed("gantt\nexcludes weekends\na :2026-11-07, until b\nb :b, 2026-11-10, 1d\n");
    expect_task(model, 0, (struct gantt_expected){0, 4, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    model = parsed("gantt\nexcludes weekends\nx :2026-11-07, 1d\ny :1d\n");
    expect_task(model, 0, (struct gantt_expected){0, 1, DIAGRAM_TASK_PLANNED});
    expect_task(model, 1, (struct gantt_expected){2, 1, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    model = parsed("gantt\nexcludes weekends\nq :q, 2026-11-07, 1d\n"
                   "p :p, 2026-11-07, until q\nr :1d\n");
    expect_task(model, 1, (struct gantt_expected){0, 0, DIAGRAM_TASK_PLANNED});
    expect_task(model, 2, (struct gantt_expected){0, 1, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    model = parsed("gantt\nexcludes weekends\na :a, 2026-11-02, until b\n"
                   "b :b, 2026-11-07, 1d\nc :after a, 1d\n");
    expect_task(model, 0, (struct gantt_expected){0, 5, DIAGRAM_TASK_PLANNED});
    expect_task(model, 2, (struct gantt_expected){7, 1, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    model = parsed("gantt\nd :d, 2026-11-02, 1d\na :until c\nc :c, 2026-11-06, 1d\n"
                   "s :s, 2026-11-02, until s\n");
    expect_task(model, 1, (struct gantt_expected){1, 3, DIAGRAM_TASK_PLANNED});
    expect_task(model, 3, (struct gantt_expected){0, 0, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
}

/* #1030 / #1032 review: a line that only begins with `weekday` or `weekend` is a task, as
 * Mermaid's lexer reads it; the last written directive wins; text after `inclusiveEndDates`
 * is not dropped; a written end is not moved by excluded days, with or without the flag. */
static void gantt_week_words(void)
{
    struct diagram_model *model = parsed("gantt\nWeekend deploy :2026-11-07, 2d\n"
                                         "weekday shift :1d\nweekend sunday :1d\n");
    expect_task(model, 0, (struct gantt_expected){0, 2, DIAGRAM_TASK_PLANNED});
    expect_task(model, 2, (struct gantt_expected){3, 1, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    model = parsed("gantt\nexcludes weekends\nweekend friday\nweekend saturday\n"
                   "x :2026-11-05, 2d\n");
    expect_task(model, 0, (struct gantt_expected){0, 2, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    /* 2026-11-05 is a Thursday: marks on Wednesdays are six days ahead, then on the day. */
    model = parsed("gantt\nweekday wednesday\ntickInterval 1week\na :2026-11-05, 2w\n");
    static const char *const ahead[] = {"2026/11/11", "11/18"};
    expect_ticks(model, ahead, 2);
    diagram_model_destroy(model);
    model = parsed("gantt\nweekday thursday\ntickInterval 1week\na :2026-11-05, 2w\n");
    static const char *const same[] = {"2026/11/5", "11/12", "11/19"};
    expect_ticks(model, same, 3);
    diagram_model_destroy(model);
    model = parsed("gantt\ninclusiveEndDates\nexcludes weekends\n"
                   "x :2026-11-05, 2026-11-06\ny :1d\nz :z, 2026-11-12, 1d\n"
                   "u :2026-11-10, until z\n");
    expect_task(model, 0, (struct gantt_expected){0, 2, DIAGRAM_TASK_PLANNED});
    expect_task(model, 1, (struct gantt_expected){2, 1, DIAGRAM_TASK_PLANNED});
    expect_task(model, 3, (struct gantt_expected){5, 2, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    reject_source("gantt\ninclusiveEndDates rollout :2026-11-02, 1d\n", DIAGRAM_UNSUPPORTED);
    reject_source("gantt\nweekday monday tuesday\nx :2026-11-02, 1d\n", DIAGRAM_INVALID);
}

/* #1022: `until` ends a task where the first of the named tasks begins (a task written further
 * down too), and excluded days move that end as they move a duration's. Weekday names in
 * `excludes` are read without regard to case. 2026-11-02 is a Monday. */
static void gantt_until(void)
{
    struct diagram_model *model = parsed("gantt\na :2026-11-02, until c\nb :b, 2026-11-04, 2d\n"
                                         "c :c, 2026-11-09, 1d\nd :2026-11-02, until c b\n");
    expect_task(model, 0, (struct gantt_expected){0, 7, DIAGRAM_TASK_PLANNED});
    expect_task(model, 3, (struct gantt_expected){0, 2, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    model = parsed("gantt\nexcludes weekends\na :2026-11-05, until b\nb :b, 2026-11-10, 1d\n");
    expect_task(model, 0, (struct gantt_expected){0, 7, DIAGRAM_TASK_PLANNED});
    expect_task(model, 1, (struct gantt_expected){5, 1, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    /* Each waits for the other's start, which is known before either end. */
    model = parsed("gantt\np :p, 2026-11-02, until q\nq :q, 2026-11-02, until p\nr :2d\n");
    expect_task(model, 0, (struct gantt_expected){0, 0, DIAGRAM_TASK_PLANNED});
    expect_task(model, 1, (struct gantt_expected){0, 0, DIAGRAM_TASK_PLANNED});
    expect_task(model, 2, (struct gantt_expected){0, 2, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    model = parsed("gantt\nexcludes sunday\nx :2026-11-06, 3d\n");
    expect_task(model, 0, (struct gantt_expected){0, 4, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    model = parsed("gantt\nexcludes Monday, TUESDAY\nx :2026-11-06, 3d\ny :1d\n");
    expect_task(model, 0, (struct gantt_expected){0, 3, DIAGRAM_TASK_PLANNED});
    expect_task(model, 1, (struct gantt_expected){5, 1, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    gantt_until_excluded();
    /* #1032: `inclusiveEndDates` (anywhere in the chart) makes a written end date the last
     * day; a duration is as before, and the task after begins the day after. */
    model =
        parsed("gantt\nx :2026-11-02, 2026-11-04\ny :1d\nINCLUSIVEENDDATES\nz :2026-11-02, 2d\n");
    expect_task(model, 0, (struct gantt_expected){0, 3, DIAGRAM_TASK_PLANNED});
    expect_task(model, 1, (struct gantt_expected){3, 1, DIAGRAM_TASK_PLANNED});
    expect_task(model, 2, (struct gantt_expected){0, 2, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    model = parsed("gantt\ninclusiveEndDates\nx :2026-11-02, 2026-11-01\n");
    expect_task(model, 0, (struct gantt_expected){0, 0, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    reject_source("gantt\ninclusiveEndDates\nx :2026-11-03, 2026-11-01\n", DIAGRAM_INVALID);
    gantt_week_words();
    /* #1030: with `weekend friday` the weekends are Friday and Saturday (2026-11-05 is a
     * Thursday); `weekend saturday` writes the default. */
    model = parsed("gantt\nexcludes weekends\nweekend friday\nx :2026-11-05, 2d\ny :1d\n");
    expect_task(model, 0, (struct gantt_expected){0, 4, DIAGRAM_TASK_PLANNED});
    expect_task(model, 1, (struct gantt_expected){4, 1, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    model = parsed("gantt\nexcludes weekends\nweekend Saturday\nx :2026-11-05, 2d\ny :1d\n");
    expect_task(model, 0, (struct gantt_expected){0, 2, DIAGRAM_TASK_PLANNED});
    expect_task(model, 1, (struct gantt_expected){4, 1, DIAGRAM_TASK_PLANNED});
    diagram_model_destroy(model);
    reject_source("gantt\nx :2026-11-01, until\n", DIAGRAM_INVALID);
    reject_source("gantt\nx :2026-11-02, Until c\nc :c, 2026-11-05, 1d\n", DIAGRAM_INVALID);
    reject_source("gantt\nx :x, 2026-11-02, until y\ny :y, 1d\n", DIAGRAM_INVALID);
    reject_source("gantt\nexcludes monday tuesday wednesday thursday friday saturday sunday\n"
                  "x :2026-11-02, 2d\n",
                  DIAGRAM_TOO_LARGE);
    reject_source("gantt\nx :2026-11-05, until y\ny :y, 2026-11-02, 1d\n", DIAGRAM_INVALID);
    reject_source("gantt\na :a, after b, until b\nb :b, after a, 1d\n", DIAGRAM_INVALID);
    reject_source("gantt\nexcludes someday\nx :2026-11-01, 1d\n", DIAGRAM_UNSUPPORTED);
}

static void gantt_tests(void)
{
    calendar_round_trip();
    gantt_contract();
    gantt_schedule();
    gantt_axis();
    gantt_every();
    gantt_rejections();
    gantt_layout();
    gantt_theme_contrast();
    gantt_year_end();
    gantt_excluded_end();
    gantt_until();
    gantt_limits();
    gantt_keyword_case();
    gantt_section_height();
}

static void class_rejections(void)
{
    reject_source("classDiagram\n", DIAGRAM_INVALID);
    reject_source("classDiagram extra\nA", DIAGRAM_INVALID);
    reject_source("classDiagram\nclass\n", DIAGRAM_INVALID);
    reject_source("classDiagram\nA B", DIAGRAM_INVALID);
    reject_source("classDiagram\nA ==> B", DIAGRAM_INVALID);
    reject_source("classDiagram\nA --> ", DIAGRAM_INVALID);
    reject_source("classDiagram\nA :", DIAGRAM_INVALID);
    reject_source("classDiagram\nclass A {\n  +x\n", DIAGRAM_INVALID);
    reject_source("classDiagram\ndirection SIDEWAYS\nA", DIAGRAM_INVALID);
    reject_source("classDiagram\nA \"1 --> B", DIAGRAM_INVALID);
    reject_source("classDiagram\nA \"\" --> B", DIAGRAM_INVALID);
    reject_source("classDiagram\nA \"1\" \"2\" --> B", DIAGRAM_INVALID);
    reject_source("classDiagram\nA --> \"*\" \"2\" B", DIAGRAM_UNSUPPORTED);
    reject_source("classDiagram\nA --> \"*\"", DIAGRAM_INVALID);
    reject_source("classDiagram\nA --> B extra", DIAGRAM_UNSUPPORTED);
    reject_source("classDiagram\nclass List~T", DIAGRAM_UNSUPPORTED);
    reject_source("classDiagram\nclass Map~K,V~", DIAGRAM_UNSUPPORTED);
    reject_source("classDiagram\nclass Box~~", DIAGRAM_UNSUPPORTED);
    reject_source("classDiagram\nA : +List~int items", DIAGRAM_UNSUPPORTED);
    reject_source("classDiagram\nA --> B~T", DIAGRAM_UNSUPPORTED);
    reject_source("classDiagram\nclass A[\"Shown\"]", DIAGRAM_UNSUPPORTED);
    reject_source("classDiagram\n<<interface>> A", DIAGRAM_UNSUPPORTED);
    reject_source("classDiagram\nnote for A \"text\"\nA", DIAGRAM_UNSUPPORTED);
    reject_source("classDiagram\nnamespace N {\nclass A\n}", DIAGRAM_UNSUPPORTED);
    reject_source("classDiagram\nstyle A fill:#f00\nA", DIAGRAM_UNSUPPORTED);
    reject_source("classDiagram\ncssClass \"A\" x\nA", DIAGRAM_UNSUPPORTED);
    reject_source("classDiagram\naccDescr: d\nA", DIAGRAM_UNSUPPORTED);
    static char wide[diagram_label_limit * 2] = "classDiagram\nclass A {\n";
    size_t used = strlen(wide);
    while (used + 16 < sizeof wide - 8)
        used += (size_t)sprintf(wide + used, "  +field%zu\n", used);
    (void)sprintf(wide + used, "}\n");
    reject_source(wide, DIAGRAM_TOO_LARGE);
}

/* Reuse: one start per scope however often it is used, states met again, plain declarations. */
static void state_reuse(void)
{
    struct diagram_model *model = parsed("stateDiagram-v2\n"
                                         "[*] --> A\n[*] --> B\nA --> B\nB --> A : back\n"
                                         "state C\nstate \"Shown\" as D\nstate A <<choice>>\n"
                                         "E : described\nA --> [*]\nB --> [*]\n");
    require(diagram_model_find_node(model, "[*]<0", 5) != diagram_none &&
                diagram_model_find_node(model, "[*]>0", 5) != diagram_none,
            "one start and one end in the diagram's scope");
    require(diagram_model_node_count(model) == 7, "A B C D E and the two markers");
    require(diagram_model_link_count(model) == 6, "every transition");
    struct diagram_node d = diagram_model_node_at(model, node_named(model, "D"));
    struct diagram_node e = diagram_model_node_at(model, node_named(model, "E"));
    struct diagram_node a = diagram_model_node_at(model, node_named(model, "A"));
    require(strcmp(diagram_model_text_at(model, d.label), "Shown") == 0 &&
                strcmp(diagram_model_text_at(model, e.label), "described") == 0 &&
                a.shape == DIAGRAM_DIAMOND,
            "declarations after use relabel or reshape the same state");
    diagram_model_destroy(model);
}

static void flow_directions(void)
{
    static const char *const sources[] = {"flowchart TB\nA-->B", "graph TD\nA-->B",
                                          "flowchart BT\nA-->B", "flowchart LR\nA-->B",
                                          "graph RL\nA-->B"};
    for (size_t index = 0; index < sizeof sources / sizeof sources[0]; ++index)
    {
        struct diagram_model *model = parsed(sources[index]);
        struct diagram_scene *scene = laid_out(model);
        struct diagram_rect first = label_bounds(scene, model, "A");
        struct diagram_rect second = label_bounds(scene, model, "B");
        bool ordered = index < 2 ? first.y < second.y : first.y > second.y;
        if (index >= 3)
            ordered = index == 3 ? first.x < second.x : first.x > second.x;
        require(ordered, "graph direction orders original endpoints, not reversed arrow semantics");
        diagram_scene_destroy(scene);
        diagram_model_destroy(model);
    }
}
static void flow_shapes(void)
{
    const char source[] = "flowchart LR\nA[a];B(b);C([c]);D[[d]];E[(e)];F((f));G(((g)));"
                          "H{h};I{{i}};J[/j/];K[\\k\\];L[/l\\];M[\\m/];N>n]\n"
                          "A--text-->B;B-.dash.->C;C==thick==>D;D o--o E;E x--x F;F<-->G;"
                          "G~~~H;H---I;I-->J-->K-->L-->M-->N";
    struct diagram_model *model = parsed(source);
    require(diagram_model_node_count(model) == 14 && diagram_model_link_count(model) == 13,
            "all supported shapes and edge tokens retain their nodes and edges");
    struct diagram_scene *scene = laid_out(model);
    require(diagram_model_node_at(model, 7).shape == DIAGRAM_DIAMOND &&
                diagram_model_node_at(model, 12).shape == DIAGRAM_TRAPEZOID_REVERSE,
            "shape delimiters preserve meaning");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}
static void connected_shape_delimiters(void)
{
    static const struct
    {
        const char *text;
        enum diagram_shape shape;
    } cases[] = {{"[label]", DIAGRAM_RECTANGLE},
                 {"(label)", DIAGRAM_ROUNDED},
                 {"([label])", DIAGRAM_STADIUM},
                 {"[[label]]", DIAGRAM_SUBROUTINE},
                 {"[(label)]", DIAGRAM_CYLINDER},
                 {"((label))", DIAGRAM_CIRCLE},
                 {"(((label)))", DIAGRAM_DOUBLE_CIRCLE},
                 {"{label}", DIAGRAM_DIAMOND},
                 {"{{label}}", DIAGRAM_HEXAGON},
                 {"[/label/]", DIAGRAM_PARALLELOGRAM},
                 {"[\\label\\]", DIAGRAM_PARALLELOGRAM_REVERSE},
                 {"[/label\\]", DIAGRAM_TRAPEZOID},
                 {"[\\label/]", DIAGRAM_TRAPEZOID_REVERSE},
                 {">label]", DIAGRAM_ASYMMETRIC}};
    size_t count = sizeof cases / sizeof cases[0];
    for (size_t first = 0; first < count; ++first)
        for (size_t second = 0; second < count; ++second)
        {
            char source[128];
            int length = snprintf(source, sizeof source, "flowchart TB\nA%s --> B%s",
                                  cases[first].text, cases[second].text);
            require(length > 0 && (size_t)length < sizeof source, "connected shape fixture fits");
            struct diagram_model *model = parsed(source);
            if (diagram_model_node_count(model) != 2 || diagram_model_link_count(model) != 1)
                fprintf(stderr, "shape chain lost an endpoint or edge: %s\n", source);
            require(diagram_model_node_count(model) == 2 && diagram_model_link_count(model) == 1,
                    "each connected shape preserves both endpoints and their edge");
            struct diagram_node left = diagram_model_node_at(model, 0);
            struct diagram_node right = diagram_model_node_at(model, 1);
            struct diagram_link link = diagram_model_link_at(model, 0);
            require(left.shape == cases[first].shape && right.shape == cases[second].shape &&
                        same_text(diagram_model_text_at(model, left.label), "label") &&
                        same_text(diagram_model_text_at(model, right.label), "label") &&
                        link.from == 0 && link.to == 1,
                    "a closing delimiter belongs to its node and preserves shape and label");
            diagram_model_destroy(model);
        }
}
static void quoted_shape_delimiters(void)
{
    struct diagram_model *model = parsed(
        "flowchart TB\nA[/\"literal /] ) } and \\\"quoted\\\"\"\\] --> B[\\\"literal \\] end\"/]\n"
        "B --> C[/c/] --> D[\\d\\]");
    require(diagram_model_node_count(model) == 4 && diagram_model_link_count(model) == 3,
            "quoted closing sequences do not terminate a node");
    struct diagram_node first = diagram_model_node_at(model, 0);
    struct diagram_node second = diagram_model_node_at(model, 1);
    require(
        first.shape == DIAGRAM_TRAPEZOID && second.shape == DIAGRAM_TRAPEZOID_REVERSE &&
            same_text(diagram_model_text_at(model, first.label), "literal /] ) } and \"quoted\"") &&
            same_text(diagram_model_text_at(model, second.label), "literal \\] end"),
        "quoted text and escaped quotes retain their content independently of shape");
    diagram_model_destroy(model);
    reject_source("flowchart TB\nA[/missing]", DIAGRAM_INVALID);
    reject_source("flowchart TB\nA[\\missing]", DIAGRAM_INVALID);
    reject_source("flowchart TB\nA[/missing] --> B[/label/]", DIAGRAM_INVALID);
    reject_source("flowchart TB\nA[\\missing] --> B[\\label\\]", DIAGRAM_INVALID);
    reject_source("flowchart TB\nA[/missing) --> B[/label/]", DIAGRAM_INVALID);
    reject_source("flowchart TB\nA[\\missing} --> B[\\label\\]", DIAGRAM_INVALID);
    reject_source("flowchart TB\nA(((missing)) --> B(((label)))", DIAGRAM_INVALID);
    reject_source("flowchart TB\nA([missing) --> B([label])", DIAGRAM_INVALID);
}
static void groups_and_cycles(void)
{
    struct diagram_model *model = parsed(flow_source);
    require(diagram_model_group_count(model) == 2 && diagram_model_group_at(model, 1).parent == 0,
            "nested subgraph membership is explicit");
    struct diagram_scene *scene = laid_out(model);
    struct diagram_scene *again = laid_out(model);
    require(diagram_scene_count(scene) == diagram_scene_count(again), "layout is repeatable");
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *first = diagram_scene_at(scene, index);
        const struct diagram_primitive *second = diagram_scene_at(again, index);
        require(first->kind == second->kind && first->bounds.x == second->bounds.x &&
                    first->bounds.y == second->bounds.y && first->count == second->count,
                "identical input and metrics yield identical placement");
    }
    diagram_scene_destroy(again);
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
    model = parsed("graph TB\nsubgraph one\ndirection LR\nA-->B\nend");
    struct diagram_metrics *_Nullable metrics = nullptr;
    require(metrics_for(model, &metrics) == DIAGRAM_ACCEPTED, "local direction metrics");
    struct diagram_scene *_Nullable rejected = nullptr;
    require(diagram_layout_create(model, metrics, &rejected) == DIAGRAM_UNSUPPORTED &&
                rejected == nullptr,
            "different subgraph direction is explicitly unsupported rather than silently ignored");
    diagram_metrics_destroy(metrics);
    diagram_model_destroy(model);
}
static void sequence_features(void)
{
    struct diagram_model *model = parsed(sequence_source);
    require(diagram_model_kind(model) == DIAGRAM_SEQUENCE && diagram_model_node_count(model) == 3,
            "sequence participants retain declaration order");
    require(strcmp(diagram_model_text_at(model, diagram_model_event_at(model, 0).link.label),
                   "2. 注文") == 0,
            "autonumber starts at the requested number");
    struct diagram_scene *scene = laid_out(model);
    struct diagram_rect user = label_bounds(scene, model, "利用者");
    struct diagram_rect api = label_bounds(scene, model, "受付");
    require(user.x < api.x, "participant ordering is stable");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
    const char extended[] =
        "sequenceDiagram\nautonumber\nA->B: plain\nA-->B: dashed\n"
        "A-xB: lost\nA--xB: lost dashed\nA-)B: async\nA--)B: async dashed\n"
        "A<<->>B: both\nA<<-->>B: both dashed\nautonumber off\nactivate A\nactivate A\n"
        "opt maybe\nNote over A: quoted #quot;text#quot; #59;\nend\n"
        "par first\nA->>B: one\nand second\nB-->>A: two\nend\n"
        "critical work\nA->>A: self\noption recovery\nA->>B: retry\nend\n"
        "break stop\nNote right of A: stop\nend\ndeactivate A\n";
    model = parsed(extended);
    scene = laid_out(model);
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}
static void lifeline_positions(const char *_Nonnull source, const int *_Nonnull starts,
                               int message_y)
{
    struct diagram_model *model = parsed(source);
    struct diagram_scene *scene = laid_out(model);
    size_t found = 0, messages = 0;
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        if (item->kind != DIAGRAM_PRIMITIVE_PATH)
            continue;
        if (item->ink == DIAGRAM_INK_MUTED)
        {
            require(found < 2 && item->count == 2 && item->stroke == DIAGRAM_DASHED &&
                        item->points[0].y == starts[found] &&
                        item->points[0].x == item->points[1].x &&
                        item->points[1].y == message_y + diagram_message_gap,
                    "lifeline connects its own header while retaining the common footer");
            ++found;
        }
        if (item->ink == DIAGRAM_INK_TEXT)
        {
            require(item->points[0].y == message_y && item->points[1].y == message_y,
                    "lifeline extension does not move the shared message row");
            ++messages;
        }
    }
    require(found == 2 && messages == 1, "mixed header fixture keeps both lifelines and message");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}
static void sequence_lifeline_headers(void)
{
    const char *sources[] = {
        "sequenceDiagram\nA->>B: call\n",
        "sequenceDiagram\nactor A\nparticipant B\nA->>B: call\n",
        "sequenceDiagram\nparticipant A\nactor B\nA->>B: call\n",
        "sequenceDiagram\nparticipant A\nparticipant B as B<br/>B<br/>B\nA->>B: call\n",
        "sequenceDiagram\nactor A\nactor B as B<br/>B<br/>B\nA->>B: call\n",
        "sequenceDiagram\ntitle Report\nactor A\nparticipant B\nA->>B: call\n",
        ("sequenceDiagram\nparticipant A as A<br/>A<br/>A\n"
         "participant B as B<br/>B<br/>B\nA->>B: call\n")};
    const int starts[][2] = {{58, 58},  {90, 58},  {58, 90}, {58, 94},
                             {90, 126}, {124, 92}, {94, 94}};
    const int messages[] = {112, 144, 144, 148, 180, 178, 148};
    for (size_t index = 0; index < sizeof sources / sizeof sources[0]; ++index)
        lifeline_positions(sources[index], starts[index], messages[index]);
}
static bool separated(struct diagram_rect first, struct diagram_rect second)
{
    return first.x + first.width <= second.x || second.x + second.width <= first.x ||
           first.y + first.height <= second.y || second.y + second.height <= first.y;
}
static void edge_labels_remain_visible(void)
{
    struct diagram_model *model =
        parsed("flowchart TB\nA-->|yes|B\nA-->|no|B\nA-->|another long branch label|C");
    struct diagram_scene *scene = laid_out(model);
    struct diagram_rect yes = label_bounds(scene, model, "yes");
    struct diagram_rect no = label_bounds(scene, model, "no");
    struct diagram_rect other = label_bounds(scene, model, "another long branch label");
    require(separated(yes, no) && separated(yes, other) && separated(no, other),
            "parallel and branching edge labels remain separately readable");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}
static void horizontal_group_heading(void)
{
    static const char *const sources[] = {
        "flowchart LR\nsubgraph S [Long subgraph heading for the reader]\nA\nend",
        "flowchart RL\nsubgraph S [Long subgraph heading for the reader]\nA\nend"};
    for (size_t index = 0; index < 2; ++index)
    {
        struct diagram_model *model = parsed(sources[index]);
        struct diagram_scene *scene = laid_out(model);
        const char *heading = "Long subgraph heading for the reader";
        struct diagram_rect bounds = label_bounds(scene, model, heading);
        require(bounds.width >= measured(heading).width &&
                    bounds.height >= measured(heading).height,
                "horizontal groups reserve the physical width and height of their measured title");
        diagram_scene_destroy(scene);
        diagram_model_destroy(model);
    }
}
static void group_heading_clearance(void)
{
    const char *directions[] = {"TB", "BT", "LR", "RL"};
    for (size_t direction = 0; direction < 4; ++direction)
    {
        char source[256];
        snprintf(source, sizeof source,
                 "flowchart %s\nsubgraph G [Long group heading<br>second line]\nA[body]\nend",
                 directions[direction]);
        struct diagram_model *model = parsed(source);
        struct diagram_scene *scene = laid_out(model);
        struct diagram_rect title = label_bounds(scene, model, "Long group heading\nsecond line");
        for (size_t index = 0; index < diagram_scene_count(scene); ++index)
        {
            const struct diagram_primitive *item = diagram_scene_at(scene, index);
            if (item->kind == DIAGRAM_PRIMITIVE_SHAPE && item->fill == DIAGRAM_INK_NODE)
                require(title.y + title.height + 8 <= item->bounds.y,
                        "group title keeps its own top band after every direction transform");
        }
        diagram_scene_destroy(scene);
        diagram_model_destroy(model);
    }
}
static void shaped_label_clearance(void)
{
    const int heights[] = {18, 21, 42, 63, 84};
    for (size_t index = 0; index < sizeof heights / sizeof heights[0]; ++index)
    {
        struct diagram_size text = {196, heights[index]};
        struct diagram_size size = diagram_geometry_node(text, DIAGRAM_CYLINDER);
        struct diagram_rect box = {10, 20, size.width, size.height};
        struct diagram_rect label = diagram_geometry_label(box, text, DIAGRAM_CYLINDER);
        int curve = diagram_geometry_cylinder_curve(size.height);
        require(label.y >= box.y + curve * 2 + 7 &&
                    label.y + label.height <= box.y + box.height - curve - 7,
                "multiline cylinder labels stay clear of both cap curves");
    }
    struct diagram_size circle =
        diagram_geometry_node((struct diagram_size){196, 42}, DIAGRAM_CIRCLE);
    require(circle.width == circle.height && circle.width >= 225,
            "a multiline label's corners retain radial clearance inside a true circle");
    circle = diagram_geometry_node((struct diagram_size){1, 18}, DIAGRAM_DOUBLE_CIRCLE);
    require(circle.width == circle.height && circle.width >= 64,
            "small circular nodes do not become ellipses after minimum sizing");
}
static void stadium_corners(struct diagram_rect box, struct diagram_rect label)
{
    /* Doubled coordinates keep half-pixel centers exact, including odd dimensions. */
    const int xs[] = {label.x * 2, (label.x + label.width) * 2};
    const int ys[] = {label.y * 2, (label.y + label.height) * 2};
    int left = box.x * 2 + box.height;
    int right = (box.x + box.width) * 2 - box.height;
    int middle = box.y * 2 + box.height;
    int64_t radius = box.height - 2;
    for (size_t x = 0; x < 2; ++x)
        for (size_t y = 0; y < 2; ++y)
        {
            int64_t dx = xs[x] < left ? left - xs[x] : 0;
            if (xs[x] > right)
                dx = xs[x] - right;
            int64_t dy = ys[y] - middle;
            require(dx * dx + dy * dy <= radius * radius,
                    "label corners stay inside the capsule with border clearance");
        }
}
static void stadium_label_clearance(void)
{
    const int widths[] = {0, 1, 196, 200, 4096};
    const int heights[] = {1, 18, 32, 33, 42, 164, 4096};
    for (size_t width = 0; width < sizeof widths / sizeof widths[0]; ++width)
        for (size_t height = 0; height < sizeof heights / sizeof heights[0]; ++height)
        {
            struct diagram_size text = {widths[width], heights[height]};
            struct diagram_size size = diagram_geometry_node(text, DIAGRAM_STADIUM);
            struct diagram_rect box = {10, 20, size.width, size.height};
            struct diagram_rect label = diagram_geometry_label(box, text, DIAGRAM_STADIUM);
            require(size.width >= 64 && size.height >= 40 && size.width >= size.height,
                    "stadium retains minimum size and circular end caps for tall labels");
            stadium_corners(box, label);
        }
    struct diagram_size compact =
        diagram_geometry_node((struct diagram_size){196, 18}, DIAGRAM_STADIUM);
    require(compact.width == 252 && compact.height == 42,
            "a one-line stadium retains its established compact dimensions");
}
static void sloped_label_clearance(void)
{
    const enum diagram_shape shapes[] = {
        DIAGRAM_HEXAGON,   DIAGRAM_PARALLELOGRAM,     DIAGRAM_PARALLELOGRAM_REVERSE,
        DIAGRAM_TRAPEZOID, DIAGRAM_TRAPEZOID_REVERSE, DIAGRAM_ASYMMETRIC};
    const int widths[] = {0, 1, 14, 28, 29, 30, 31, 32, 33, 196, 200, 4096};
    for (size_t shape = 0; shape < sizeof shapes / sizeof shapes[0]; ++shape)
        for (size_t width = 0; width < sizeof widths / sizeof widths[0]; ++width)
        {
            struct diagram_size text = {widths[width], 164};
            struct diagram_size size = diagram_geometry_node(text, shapes[shape]);
            int padded = text.width + diagram_node_padding_x * 2;
            require(size.width * (diagram_polygon_slope_divisor - 2) >=
                        padded * diagram_polygon_slope_divisor,
                    "the straight central band reserves the label and horizontal padding");
            require(size.width >= 64 && size.height == 188,
                    "sloped shapes preserve minimum width and measured vertical padding");
        }
    struct diagram_size small =
        diagram_geometry_node((struct diagram_size){14, 1}, DIAGRAM_HEXAGON);
    require(small.width == 70 && small.height == 40,
            "compact sloped nodes preserve their existing width and minimum height");
    struct diagram_size subroutine =
        diagram_geometry_node((struct diagram_size){196, 164}, DIAGRAM_SUBROUTINE);
    require(subroutine.width == 252 && subroutine.height == 188,
            "subroutine separators retain their distinct existing width reservation");
}
static void shared_text_roles(void)
{
    struct diagram_model *model = parsed("flowchart TB\nA[shared] -->|shared| B[next]");
    size_t label = diagram_model_node_at(model, 0).label;
    require(label == diagram_model_link_at(model, 0).label, "fixture shares one interned string");
    struct diagram_scene *scene = laid_out(model);
    struct diagram_size body_size = {0}, caption_size = {0};
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        if (item->kind != DIAGRAM_PRIMITIVE_TEXT || item->text != label)
            continue;
        switch (item->role)
        {
        case DIAGRAM_TEXT_BODY:
            body_size = (struct diagram_size){item->bounds.width, item->bounds.height};
            break;
        case DIAGRAM_TEXT_CAPTION:
            caption_size = (struct diagram_size){item->bounds.width, item->bounds.height};
            break;
        }
    }
    require(body_size.height == 18 && caption_size.height == 20,
            "shared text uses body metrics in a node and caption metrics plus padding on an edge");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}
static void sequence_arrow_meaning(void)
{
    struct diagram_model *model = parsed("sequenceDiagram\nA->B: plain\nA-->B: dotted\n"
                                         "A-)B: async\nA--)B: async dotted\nA->>B: arrow");
    const enum diagram_arrow arrows[] = {DIAGRAM_ARROW_NONE, DIAGRAM_ARROW_NONE, DIAGRAM_ARROW_OPEN,
                                         DIAGRAM_ARROW_OPEN, DIAGRAM_ARROW_CLOSED};
    for (size_t index = 0; index < sizeof arrows / sizeof arrows[0]; ++index)
        require(diagram_model_event_at(model, index).link.end == arrows[index],
                "plain lines, asynchronous heads and closed heads preserve Mermaid meaning");
    diagram_model_destroy(model);
}
static void self_activation_arrival(void)
{
    struct diagram_model *model = parsed("sequenceDiagram\nA->>+A: call\nA->>-A: return");
    struct diagram_scene *scene = laid_out(model);
    int arrivals[2] = {0};
    size_t messages = 0;
    struct diagram_rect activation = {0};
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        if (item->kind == DIAGRAM_PRIMITIVE_SHAPE && item->bounds.width == 12)
            activation = item->bounds;
        if (item->kind == DIAGRAM_PRIMITIVE_PATH && item->end == DIAGRAM_ARROW_CLOSED &&
            messages < 2)
            arrivals[messages++] = item->points[item->count - 1].y;
    }
    require(messages == 2 && activation.y == arrivals[0] &&
                activation.y + activation.height == arrivals[1],
            "self activation starts and ends at message arrival");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}
static void activation_command(struct text_buffer *_Nonnull source, const char *_Nonnull command,
                               const char *_Nonnull actor)
{
    text_buffer_append_text(source, command);
    text_buffer_append_text(source, " ");
    text_buffer_append_text(source, actor);
    text_buffer_append_text(source, "\n");
}
static struct text_buffer *_Nonnull activation_fixture(size_t depth, bool single, bool closed,
                                                       bool framed)
{
    struct text_buffer *_Nullable source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "activation extent fixture");
    text_buffer_append_text(source, "sequenceDiagram\nparticipant A\n");
    if (!single)
        text_buffer_append_text(source, "participant B\nparticipant C\n");
    if (framed)
        text_buffer_append_text(source, "loop work\n");
    const char *actor = single ? "A" : "C";
    for (size_t index = 0; index < depth; ++index)
        activation_command(source, "activate", actor);
    text_buffer_append_text(source, "Note left of ");
    text_buffer_append_text(source, actor);
    text_buffer_append_text(source, ": working\n");
    if (!single)
        text_buffer_append_text(source, "A->>C: work\n");
    if (closed)
        for (size_t index = 0; index < depth; ++index)
            activation_command(source, "deactivate", actor);
    if (framed)
        text_buffer_append_text(source, "end\n");
    require(text_buffer_finish(source) == TEXT_BUFFER_ACCEPTED, "activation fixture complete");
    return source;
}
static void activation_extent(size_t depth, bool single, bool closed, bool framed)
{
    struct text_buffer *source = activation_fixture(depth, single, closed, framed);
    struct diagram_model *model = parsed(text_buffer_bytes(source));
    struct diagram_scene *scene = laid_out(model);
    struct diagram_size size = diagram_scene_size(scene);
    size_t activations = 0;
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        if (item->kind == DIAGRAM_PRIMITIVE_SHAPE &&
            item->bounds.width == diagram_activation_width && item->fill == DIAGRAM_INK_NODE)
        {
            ++activations;
            require(item->bounds.x + item->bounds.width <= size.width - diagram_outer_margin,
                    "every nested activation fits inside the scene's outer margin");
        }
    }
    require(activations == depth, "all supported activation levels reach the published scene");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
    text_buffer_destroy(source);
}
static void sequence_activation_extents(void)
{
    for (size_t depth = diagram_depth_limit; depth != 0; --depth)
    {
        activation_extent(depth, true, true, false);
        activation_extent(depth, true, false, false);
        activation_extent(depth, false, true, false);
        activation_extent(depth, false, false, false);
    }
    activation_extent(diagram_depth_limit, true, true, true);
    activation_extent(diagram_depth_limit, false, false, true);
    struct text_buffer *source = activation_fixture(diagram_depth_limit + 1, true, false, false);
    reject_source(text_buffer_bytes(source), DIAGRAM_TOO_LARGE);
    text_buffer_destroy(source);
}

static void activation_pair(struct text_buffer *_Nonnull source, const char *_Nonnull command,
                            bool both)
{
    activation_command(source, command, "A");
    if (both)
        activation_command(source, command, "B");
}
static struct text_buffer *_Nonnull activated_message_fixture(size_t depth, bool self, bool closed)
{
    struct text_buffer *_Nullable source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "activated message fixture");
    text_buffer_append_text(source, "sequenceDiagram\nparticipant A\n");
    if (!self)
        text_buffer_append_text(source, "participant B\n");
    for (size_t index = 0; index < depth; ++index)
        activation_pair(source, "activate", !self);
    text_buffer_append_text(source, self ? "A->>A: " : "A->>B: ");
    text_buffer_append_text(source,
                            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\n");
    text_buffer_append_text(source, self ? "A-->>A: reply\n" : "B-->>A: reply\n");
    if (closed)
    {
        for (size_t index = 0; index < depth; ++index)
            activation_pair(source, "deactivate", !self);
        text_buffer_append_text(source, self ? "A->>A: after\n" : "A->>B: after\n");
    }
    require(text_buffer_finish(source) == TEXT_BUFFER_ACCEPTED,
            "activated message fixture complete");
    return source;
}
static bool activation_shape(const struct diagram_primitive *_Nonnull item)
{
    return item->kind == DIAGRAM_PRIMITIVE_SHAPE && item->fill == DIAGRAM_INK_NODE &&
           item->bounds.width == diagram_activation_width;
}
static void activated_label_clearance(const struct diagram_scene *_Nonnull scene,
                                      const struct diagram_model *_Nonnull model)
{
    struct diagram_rect call = label_bounds(
        scene, model, "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    struct diagram_rect reply = label_bounds(scene, model, "reply");
    struct diagram_size size = diagram_scene_size(scene);
    require(call.x + call.width <= size.width - diagram_outer_margin &&
                reply.x + reply.width <= size.width - diagram_outer_margin,
            "activated message labels keep the outer margin");
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        if (activation_shape(item))
            require(separated(call, item->bounds) && separated(reply, item->bounds),
                    "long call and reply text stay outside every active bar");
    }
}
static void activated_self_turns(const struct diagram_scene *_Nonnull scene)
{
    int right = 0;
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        if (activation_shape(item) && right < item->bounds.x + item->bounds.width)
            right = item->bounds.x + item->bounds.width;
    }
    size_t active_turns = 0;
    for (size_t index = 0; index < diagram_scene_count(scene) && active_turns < 2; ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        if (item->kind == DIAGRAM_PRIMITIVE_PATH && item->ink == DIAGRAM_INK_TEXT)
        {
            require(item->count == 4 && item->points[1].x > right && item->points[2].x > right,
                    "self message turns remain visible beyond the active bar envelope");
            ++active_turns;
        }
    }
    require(active_turns == 2, "both active self message turns are present");
}
static void inactive_message_tail(const struct diagram_scene *_Nonnull scene,
                                  const struct diagram_model *_Nonnull model)
{
    struct diagram_rect from = label_bounds(scene, model, "A");
    struct diagram_rect to =
        diagram_model_node_count(model) == 1 ? from : label_bounds(scene, model, "B");
    const struct diagram_primitive *_Nullable last = nullptr;
    size_t messages = 0;
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        if (item->kind == DIAGRAM_PRIMITIVE_PATH && item->ink == DIAGRAM_INK_TEXT)
        {
            last = item;
            ++messages;
        }
    }
    require(last != nullptr && messages == 3 && last->points[0].x == from.x + from.width / 2 &&
                last->points[last->count - 1].x == to.x + to.width / 2,
            "a later message returns to both lifelines after activations close");
}
static void activated_message_extent(size_t depth, bool self, bool closed)
{
    struct text_buffer *source = activated_message_fixture(depth, self, closed);
    struct diagram_model *model = parsed(text_buffer_bytes(source));
    struct diagram_scene *scene = laid_out(model);
    activated_label_clearance(scene, model);
    if (self)
        activated_self_turns(scene);
    if (closed)
        inactive_message_tail(scene, model);
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
    text_buffer_destroy(source);
}
static void sequence_activation_connections(void)
{
    for (size_t depth = 1; depth <= diagram_depth_limit; ++depth)
    {
        activated_message_extent(depth, true, false);
        activated_message_extent(depth, true, true);
        activated_message_extent(depth, false, false);
        activated_message_extent(depth, false, true);
    }
}

static void rejected_grammar(void)
{
    static const char *const invalid[] = {"graph sideways\nA-->B",
                                          "graph TB\nA[unclosed",
                                          "flowchart TB\nsubgraph G\nA-->B",
                                          "graph TB\nend",
                                          "graph TB\nA -->",
                                          "sequenceDiagram\nA->>B",
                                          "sequenceDiagram\ndeactivate A",
                                          "sequenceDiagram\nalt condition\nA->>B: hi",
                                          "sequenceDiagram\nelse nope",
                                          "sequenceDiagram\nautonumber 1 0\nA->>B: hi",
                                          "sequenceDiagram\nend"};
    for (size_t index = 0; index < sizeof invalid / sizeof invalid[0]; ++index)
        reject_source(invalid[index], DIAGRAM_INVALID);
    static const char *const unsupported[] = {"journey\ntitle x",
                                              "%%{init: {}}%%\ngraph TB\nA-->B",
                                              "graph TB\nA-->B\nclick A callback",
                                              "graph TB\nA[<b>bold</b>]",
                                              "graph TB\nA[`markdown`]",
                                              "sequenceDiagram\nbox grey\nparticipant A\nend",
                                              "graph TB\nA & B --> C",
                                              "sequenceDiagram\ncreate participant C"};
    for (size_t index = 0; index < sizeof unsupported / sizeof unsupported[0]; ++index)
        reject_source(unsupported[index], DIAGRAM_UNSUPPORTED);
}
static void escaped_labels(void)
{
    struct diagram_model *model = parsed("graph TB\nA[\"a #35; &amp; x < y<br/>line\\nlast\\\"\"]");
    const char *label = diagram_model_text_at(model, diagram_model_node_at(model, 0).label);
    require(strcmp(label, "a # & x < y\nline\nlast\"") == 0,
            "Mermaid entities, line breaks, comparisons and escaped quotes retain label text");
    diagram_model_destroy(model);
}
static void bounded_input(void)
{
    struct text_buffer *_Nullable source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "bounded graph input");
    text_buffer_append_text(source, "graph TB\n");
    for (size_t index = 0; index <= diagram_node_limit; ++index)
    {
        char line[32];
        int length = snprintf(line, sizeof line, "N%zu\n", index);
        require(length > 0 && (size_t)length < sizeof line, "node identifier fixture fits");
        text_buffer_append(source, line, (size_t)length);
    }
    reject_source(text_buffer_bytes(source), DIAGRAM_TOO_LARGE);
    text_buffer_destroy(source);
    char *oversized = malloc(diagram_source_limit + 2);
    require(oversized != nullptr, "oversized diagram fixture");
    memset(oversized, 'x', diagram_source_limit + 1);
    oversized[diagram_source_limit + 1] = '\0';
    reject_source(oversized, DIAGRAM_TOO_LARGE);
    free(oversized);
}
static void repeated_limit(const char *_Nonnull header, const char *_Nonnull statement,
                           size_t count)
{
    struct text_buffer *_Nullable source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "bounded grammar fixture");
    text_buffer_append_text(source, header);
    for (size_t index = 0; index < count; ++index)
        text_buffer_append_text(source, statement);
    reject_source(text_buffer_bytes(source), DIAGRAM_TOO_LARGE);
    text_buffer_destroy(source);
}
static void grammar_limits(void)
{
    repeated_limit("flowchart TB\n", "A --> B\n", diagram_link_limit + 1);
    repeated_limit("sequenceDiagram\n", "A->>B: message\n", diagram_event_limit + 1);
    repeated_limit("sequenceDiagram\nparticipant A\n", "activate A\n", diagram_depth_limit + 1);
    repeated_limit("sequenceDiagram\n", "loop again\n", diagram_depth_limit + 1);
    struct text_buffer *_Nullable source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "group count fixture");
    text_buffer_append_text(source, "flowchart TB\n");
    for (size_t index = 0; index <= diagram_group_limit; ++index)
    {
        char line[64];
        int length = snprintf(line, sizeof line, "subgraph G%zu\nA%zu\nend\n", index, index);
        require(length > 0 && (size_t)length < sizeof line, "group statement fits");
        text_buffer_append(source, line, (size_t)length);
    }
    reject_source(text_buffer_bytes(source), DIAGRAM_TOO_LARGE);
    text_buffer_destroy(source);
    source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "label count fixture");
    text_buffer_append_text(source, "flowchart TB\nA[\"");
    for (size_t index = 0; index <= diagram_label_limit; ++index)
        text_buffer_append_text(source, "x");
    text_buffer_append_text(source, "\"]\n");
    reject_source(text_buffer_bytes(source), DIAGRAM_TOO_LARGE);
    text_buffer_destroy(source);
}
static void media_limits(void)
{
    struct markdown_media *_Nullable media = nullptr;
    require(markdown_media_create("", &media) == IMAGE_READY, "bounded media collection");
    size_t slot = 0;
    for (size_t index = 0; index < markdown_media_display_limit; ++index)
    {
        require(markdown_media_begin_diagram(media, &slot) == IMAGE_READY && slot == index &&
                    markdown_media_end_diagram(media, slot) == IMAGE_READY,
                "each allowed diagram retains its display slot");
    }
    require(markdown_media_begin_diagram(media, &slot) == IMAGE_TOO_LARGE &&
                markdown_media_overflow(media),
            "diagram display overflow is explicit");
    markdown_media_destroy(media);
    media = nullptr;
    require(markdown_media_create("", &media) == IMAGE_READY &&
                markdown_media_begin_diagram(media, &slot) == IMAGE_READY,
            "source limit collection");
    char *large = malloc(diagram_source_limit + 1);
    require(large != nullptr, "large raw fence fixture");
    memset(large, 'x', diagram_source_limit + 1);
    struct markdown_line line = {.bytes = large, .length = diagram_source_limit + 1};
    markdown_media_diagram_line(media, slot, line);
    require(markdown_media_end_diagram(media, slot) == IMAGE_READY &&
                markdown_media_status(media, slot) == IMAGE_TOO_LARGE,
            "oversized fence preserves body rendering and carries typed deferred failure");
    free(large);
    markdown_media_destroy(media);
}
static void markdown_contract(void)
{
    const char source[] = "本文\r\n![one](images/a.png)\r\n\r\n> ```mermaid\r\n"
                          "> graph LR\r\n> A-->B\r\n> ```\r\n\r\n![two](images/b.png)\r\n"
                          "```mermaid\nsequenceDiagram\nA->>B: hi\n```\n";
    struct note_text *text = body(source);
    struct markdown_rtf *_Nullable rtf = nullptr;
    require(markdown_rtf_create(
                text, folio_theme_palette(FOLIO_THEME_LIGHT),
                (struct markdown_rtf_locale){ui_font_face(FOLIO_LANGUAGE_JA), FOLIO_LANGUAGE_JA},
                &rtf) == MARKDOWN_RTF_CONVERTED,
            "mixed Markdown renders first");
    const struct markdown_media *media = markdown_rtf_media(rtf);
    require(media != nullptr && markdown_media_count(media) == 4,
            "images and diagrams share display order");
    require(markdown_images_count(markdown_media_images(media)) == 2,
            "export collection contains actual image destinations only");
    require(markdown_media_kind(media, 1) == MARKDOWN_MEDIA_DIAGRAM &&
                markdown_media_image_index(media, 2) == 1,
            "display slots do not alias export indices");
    require(strcmp(note_text_bytes(markdown_media_diagram(media, 1)), "graph LR\nA-->B\n") == 0,
            "canonical container parser supplies fence contents without quote prefixes");
    require(strstr(markdown_rtf_text(rtf), "<nenefolio:image:3>") != nullptr &&
                strstr(markdown_rtf_text(rtf), "A-->B") == nullptr,
            "body contains lightweight deferred placeholders");
    require(strcmp(note_text_bytes(text), source) == 0,
            "lazy diagrams never rewrite saved Markdown");
    markdown_rtf_destroy(rtf);
    note_text_destroy(text);
}
static enum diagram_outcome allocate_diagram(const struct note_text *_Nonnull text)
{
    struct diagram_model *_Nullable model = nullptr;
    struct diagram_metrics *_Nullable metrics = nullptr;
    struct diagram_scene *_Nullable scene = nullptr;
    enum diagram_outcome outcome = mermaid_diagram_create(text, &model);
    if (outcome == DIAGRAM_ACCEPTED)
        outcome = metrics_for(model, &metrics);
    if (outcome == DIAGRAM_ACCEPTED)
        outcome = diagram_layout_create(model, metrics, &scene);
    diagram_scene_destroy(scene);
    diagram_metrics_destroy(metrics);
    diagram_model_destroy(model);
    return outcome;
}
static void allocation_contract(const char *_Nonnull source)
{
    struct note_text *text = body(source);
    size_t allocations = 0;
    for (size_t nth = 0; nth <= allocations; ++nth)
    {
        allocation_probe_fail_at(nth);
        enum diagram_outcome result = allocate_diagram(text);
        if (nth == 0)
            allocations = allocation_probe_calls();
        allocation_probe_fail_at(0);
        require(nth == 0 ? result == DIAGRAM_ACCEPTED : result == DIAGRAM_OUT_OF_MEMORY,
                "each diagram allocation failure is typed and releases partial state");
    }
    note_text_destroy(text);
}
static void markdown_allocation_contract(const char *_Nonnull source)
{
    struct note_text *text = body(source);
    size_t allocations = 0;
    for (size_t nth = 0; nth <= allocations; ++nth)
    {
        allocation_probe_fail_at(nth);
        struct markdown_rtf *_Nullable rtf = nullptr;
        enum markdown_rtf_outcome result = markdown_rtf_create(
            text, folio_theme_palette(FOLIO_THEME_LIGHT),
            (struct markdown_rtf_locale){"Noto Sans JP", FOLIO_LANGUAGE_JA}, &rtf);
        if (nth == 0)
            allocations = allocation_probe_calls();
        allocation_probe_fail_at(0);
        require(nth == 0 ? result == MARKDOWN_RTF_CONVERTED
                         : result == MARKDOWN_RTF_OUT_OF_MEMORY && rtf == nullptr,
                "diagram collection never loses an allocation failure at a fence boundary");
        markdown_rtf_destroy(rtf);
    }
    note_text_destroy(text);
}
static void autonumber_boundary(void)
{
    struct diagram_model *model = parsed("sequenceDiagram\nautonumber 1000000\nA->>B: last");
    struct diagram_event event = diagram_model_event_at(model, 0);
    require(strcmp(diagram_model_text_at(model, event.link.label), "1000000. last") == 0,
            "last allowed message number is usable");
    diagram_model_destroy(model);
    reject_source("sequenceDiagram\nautonumber 1000000\nA->>B: last\nB->>A: over",
                  DIAGRAM_TOO_LARGE);
}
static void palette_contrast(void)
{
    for (size_t index = 0; index < folio_theme_count(); ++index)
    {
        struct diagram_palette palette =
            diagram_palette_for(folio_theme_palette(folio_theme_at(index)));
        require(theme_contrast_ratio(palette.text, palette.background) >= 4.5 &&
                    theme_contrast_ratio(palette.text, palette.node) >= 4.5 &&
                    theme_contrast_ratio(palette.text, palette.group) >= 4.5 &&
                    theme_contrast_ratio(palette.text, palette.note) >= 4.5,
                "all themes retain readable diagram text on each fill");
        require(theme_contrast_ratio(palette.accent, palette.node) >= 3.0,
                "all themes retain visible node outlines");
        require(theme_contrast_ratio(palette.muted, palette.background) >= 3.0 &&
                    theme_contrast_ratio(palette.muted, palette.node) >= 3.0 &&
                    theme_contrast_ratio(palette.muted, palette.group) >= 3.0 &&
                    theme_contrast_ratio(palette.muted, palette.note) >= 3.0,
                "supporting lines remain visible against every diagram fill");
    }
}
static void model_construction_contract(void)
{
    struct diagram_model *_Nullable model = nullptr;
    require(diagram_model_create(DIAGRAM_FLOWCHART, &model) == DIAGRAM_ACCEPTED,
            "model construction begins privately");
    size_t text = 0;
    require(diagram_model_text(model, "A", 1, &text) == DIAGRAM_ACCEPTED,
            "model owns its first identifier");
    const char *invalid[] = {"\xFF", "\0", "\x01"};
    for (size_t index = 0; index < sizeof invalid / sizeof invalid[0]; ++index)
        require(diagram_model_text(model, invalid[index], 1, &text) == DIAGRAM_INVALID,
                "invalid UTF8, NUL and controls cannot become labels");
    struct diagram_node node = {.id = 0, .label = 0, .group = diagram_none};
    require(diagram_model_node(model, 1, node) == DIAGRAM_INVALID &&
                diagram_model_node(model, 0, node) == DIAGRAM_ACCEPTED,
            "node construction cannot leave an uninitialized index hole");
    struct diagram_group group = {.id = 0, .label = 0, .parent = diagram_none};
    require(diagram_model_group(model, 1, group) == DIAGRAM_INVALID &&
                diagram_model_group(model, 0, group) == DIAGRAM_ACCEPTED,
            "groups also require contiguous construction");
    group.parent = 0;
    require(diagram_model_group(model, 0, group) == DIAGRAM_INVALID,
            "a group cannot contain itself");
    diagram_model_title(model, diagram_none);
    require(diagram_model_title_text(model) == diagram_none, "unknown title is not adopted");
    diagram_model_title(model, 0);
    diagram_model_seal(model);
    diagram_model_direction(model, DIAGRAM_RIGHT_LEFT);
    diagram_model_title(model, diagram_none);
    struct diagram_link link = {.from = 0, .to = 0, .label = 0, .length = 1};
    struct diagram_event event = {.kind = DIAGRAM_MESSAGE, .link = link};
    require(diagram_model_text(model, "changed", 7, &text) == DIAGRAM_INVALID &&
                diagram_model_node(model, 0, node) == DIAGRAM_INVALID &&
                diagram_model_group(model, 0, group) == DIAGRAM_INVALID &&
                diagram_model_link(model, link) == DIAGRAM_INVALID &&
                diagram_model_event(model, event) == DIAGRAM_INVALID,
            "every construction entry rejects writes after publication");
    require(diagram_model_flow(model) == DIAGRAM_TOP_BOTTOM &&
                diagram_model_title_text(model) == 0 &&
                strcmp(diagram_model_text_at(model, diagram_none), "") == 0,
            "published direction and title remain immutable and unknown text is empty");
    diagram_model_destroy(model);
}
static void model_reference_contract(void)
{
    struct diagram_model *_Nullable model = nullptr;
    require(diagram_model_create(DIAGRAM_SEQUENCE, &model) == DIAGRAM_ACCEPTED,
            "reference validation fixture");
    size_t text = 0;
    require(diagram_model_text(model, "A", 1, &text) == DIAGRAM_ACCEPTED,
            "reference fixture identifier");
    struct diagram_node node = {.id = 0, .label = 0, .group = diagram_none};
    require(diagram_model_node(model, 0, node) == DIAGRAM_ACCEPTED, "known participant");
    const struct diagram_node bad_nodes[] = {{.id = 1, .label = 0, .group = diagram_none},
                                             {.id = 0, .label = 1, .group = diagram_none},
                                             {.id = 0, .label = 0, .group = 0}};
    for (size_t index = 0; index < sizeof bad_nodes / sizeof bad_nodes[0]; ++index)
        require(diagram_model_node(model, 0, bad_nodes[index]) == DIAGRAM_INVALID,
                "unresolved node text and group references are rejected");
    const struct diagram_link bad_links[] = {{.from = 1, .to = 0, .label = 0, .length = 1},
                                             {.from = 0, .to = 1, .label = 0, .length = 1},
                                             {.from = 0, .to = 0, .label = 1, .length = 1},
                                             {.from = 0, .to = 0, .label = 0, .length = 0},
                                             {.from = 0, .to = 0, .label = 0, .length = 17}};
    for (size_t index = 0; index < sizeof bad_links / sizeof bad_links[0]; ++index)
        require(diagram_model_link(model, bad_links[index]) == DIAGRAM_INVALID,
                "unresolved endpoints and out-of-range rank lengths are rejected");
    for (size_t index = 0; index < 3; ++index)
    {
        struct diagram_event event = {.kind = DIAGRAM_MESSAGE, .link = bad_links[index]};
        require(diagram_model_event(model, event) == DIAGRAM_INVALID,
                "sequence events cannot publish unresolved references");
    }
    struct diagram_group group = {.id = 1, .label = 0, .parent = diagram_none};
    require(diagram_model_group(model, 0, group) == DIAGRAM_INVALID, "unknown group identifier");
    group.id = 0;
    group.label = 1;
    require(diagram_model_group(model, 0, group) == DIAGRAM_INVALID, "unknown group label");
    require(diagram_model_node_count(model) == 1 && diagram_model_link_count(model) == 0 &&
                diagram_model_event_count(model) == 0 && diagram_model_group_count(model) == 0,
            "rejected references never change published counts");
    diagram_model_destroy(model);
}
static void metric_input_contract(void)
{
    const struct diagram_size bad[] = {{-1, 18}, {1, 0}, {4097, 18}, {1, 4097}};
    for (size_t index = 0; index < sizeof bad / sizeof bad[0]; ++index)
        for (size_t role = 0; role < 2; ++role)
        {
            struct diagram_label_metrics pair = {{10, 18}, {9, 16}};
            if (role == 0)
                pair.body = bad[index];
            else
                pair.caption = bad[index];
            struct diagram_metrics *_Nullable metrics = nullptr;
            require(diagram_metrics_create(&pair, 1, &metrics) == DIAGRAM_TOO_LARGE &&
                        metrics == nullptr,
                    "invalid native measurements fail before layout for either text role");
        }
    struct diagram_label_metrics pair = {{10, 18}, {9, 16}};
    struct diagram_metrics *_Nullable metrics = nullptr;
    require(diagram_metrics_create(&pair, diagram_text_limit + 1, &metrics) == DIAGRAM_TOO_LARGE,
            "oversized measurement lists are rejected before reading their elements");
    require(diagram_metrics_create(&pair, 1, &metrics) == DIAGRAM_ACCEPTED,
            "valid measurement snapshot");
    pair.body.width = 100;
    require(diagram_metrics_at(metrics, 0, DIAGRAM_TEXT_BODY).width == 10 &&
                diagram_metrics_at(metrics, 1, DIAGRAM_TEXT_BODY).height == 0,
            "measurements are copied and unavailable indices have no size");
    struct diagram_model *model = parsed("flowchart TB\nA --> B");
    struct diagram_scene *_Nullable scene = nullptr;
    require(diagram_layout_create(model, metrics, &scene) == DIAGRAM_INVALID && scene == nullptr,
            "layout refuses measurements from a different model");
    diagram_model_destroy(model);
    diagram_metrics_destroy(metrics);
}
static void rejected_scene(struct diagram_primitive item)
{
    struct diagram_scene *_Nullable scene = nullptr;
    require(diagram_scene_create(&scene) == DIAGRAM_ACCEPTED &&
                diagram_scene_append(scene, &item) == DIAGRAM_ACCEPTED,
            "private scene can collect geometry before its extent is known");
    require(diagram_scene_seal(scene, (struct diagram_size){100, 100}) == DIAGRAM_TOO_LARGE,
            "out-of-bounds geometry cannot be published to the raster adapter");
    diagram_scene_destroy(scene);
}
static void scene_publication_contract(void)
{
    const struct diagram_rect bad[] = {{-1, 0, 1, 1}, {0, -1, 1, 1},  {0, 0, -1, 1},
                                       {0, 0, 1, -1}, {0, 0, 101, 1}, {0, 0, 1, 101},
                                       {99, 0, 2, 1}, {0, 99, 1, 2}};
    for (size_t index = 0; index < sizeof bad / sizeof bad[0]; ++index)
        rejected_scene(
            (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_SHAPE, .bounds = bad[index]});
    const struct diagram_point points[] = {{-1, 0}, {0, -1}, {101, 0}, {0, 101}};
    for (size_t index = 0; index < sizeof points / sizeof points[0]; ++index)
        rejected_scene((struct diagram_primitive){
            .kind = DIAGRAM_PRIMITIVE_PATH, .count = 2, .points = {{0, 0}, points[index]}});
    rejected_scene((struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_PATH, .count = 1});
    struct diagram_scene *_Nullable scene = nullptr;
    require(diagram_scene_create(&scene) == DIAGRAM_ACCEPTED, "scene publication fixture");
    struct diagram_primitive item = {.kind = DIAGRAM_PRIMITIVE_PATH, .count = 9};
    require(diagram_scene_append(scene, &item) == DIAGRAM_INVALID,
            "path point count cannot exceed the stored point array");
    const struct diagram_size sizes[] = {{0, 1}, {1, 0}, {32769, 1}, {1, 32769}};
    for (size_t index = 0; index < sizeof sizes / sizeof sizes[0]; ++index)
        require(diagram_scene_seal(scene, sizes[index]) == DIAGRAM_TOO_LARGE,
                "invalid canvas extents do not publish a scene");
    item = (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_SHAPE, .bounds = {0, 0, 1, 1}};
    require(diagram_scene_append(scene, &item) == DIAGRAM_ACCEPTED &&
                diagram_scene_seal(scene, (struct diagram_size){1, 1}) == DIAGRAM_ACCEPTED,
            "a boundary-touching valid scene is publishable");
    require(diagram_scene_append(scene, &item) == DIAGRAM_INVALID &&
                diagram_scene_seal(scene, (struct diagram_size){2, 2}) == DIAGRAM_INVALID &&
                diagram_scene_count(scene) == 1 && diagram_scene_size(scene).width == 1,
            "published geometry and extent cannot be mutated");
    diagram_scene_destroy(scene);
}
static void palette_key_contract(void)
{
    struct diagram_palette original = diagram_palette_for(folio_theme_palette(FOLIO_THEME_LIGHT));
    require(diagram_palette_equal(&original, &original), "identical appearance can reuse a bitmap");
    struct diagram_palette variants[7];
    for (size_t index = 0; index < 7; ++index)
        variants[index] = original;
    variants[0].background.red ^= 1;
    variants[1].text.red ^= 1;
    variants[2].muted.red ^= 1;
    variants[3].accent.red ^= 1;
    variants[4].node.red ^= 1;
    variants[5].group.red ^= 1;
    variants[6].note.red ^= 1;
    for (size_t index = 0; index < 7; ++index)
        require(!diagram_palette_equal(&original, &variants[index]),
                "a change to any rendered color invalidates bitmap reuse");
    struct diagram_model *model = parsed(sequence_source);
    struct diagram_scene *scene = laid_out(model);
    for (size_t index = 0; index < diagram_scene_count(scene); ++index)
    {
        const struct diagram_primitive *item = diagram_scene_at(scene, index);
        struct rgb_color ink = diagram_palette_ink(&original, item->ink);
        struct rgb_color fill = diagram_palette_ink(&original, item->fill);
        if (item->kind == DIAGRAM_PRIMITIVE_TEXT)
            require(theme_contrast_ratio(ink, fill) >= 4.5,
                    "the actual scene's role-selected text colors retain readable contrast");
    }
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}
static void low_contrast_appearance(void)
{
    const unsigned char levels[] = {0, 64, 127, 192, 255};
    for (size_t background = 0; background < sizeof levels; ++background)
        for (size_t accent = 0; accent < sizeof levels; ++accent)
        {
            struct theme_palette theme = *folio_theme_palette(FOLIO_THEME_LIGHT);
            unsigned char value = levels[background];
            theme.pane = theme.rtf.text = (struct rgb_color){value, value, value};
            value = levels[accent];
            theme.rtf.link = (struct rgb_color){value, value, value};
            struct diagram_palette palette = diagram_palette_for(&theme);
            require(theme_contrast_ratio(palette.text, palette.background) >= 4.5 &&
                        theme_contrast_ratio(palette.text, palette.node) >= 4.5 &&
                        theme_contrast_ratio(palette.text, palette.group) >= 4.5 &&
                        theme_contrast_ratio(palette.text, palette.note) >= 4.5,
                    "unreadable imported colors fall back to readable text and fills");
            require(theme_contrast_ratio(palette.accent, palette.node) >= 3.0 &&
                        theme_contrast_ratio(palette.muted, palette.note) >= 3.0,
                    "outline and background-line fallbacks remain visible");
        }
}
static void additional_sequence_contract(void)
{
    const char source[] = "sequenceDiagram\nparticipant A\nparticipant B\nparticipant C\n"
                          "autonumber\nA<<-->>B: first\nautonumber off\nB-xA: plain\n"
                          "activate A\nactivate A\nNote over A: local\nNote over C,A: wide note\n"
                          "opt\nA--xC: stopped\nend\npar work\nA<<->>B: sync\nand other\n"
                          "B--)C: async\nend\ncritical commit\nC->>A: result\noption unavailable\n"
                          "break exit\nA->>A: done\nend\nend\ndeactivate A\n";
    struct diagram_model *model = parsed(source);
    require(strcmp(diagram_model_text_at(model, diagram_model_event_at(model, 0).link.label),
                   "1. first") == 0 &&
                strcmp(diagram_model_text_at(model, diagram_model_event_at(model, 1).link.label),
                       "plain") == 0,
            "autonumber off resumes unnumbered messages without changing earlier labels");
    struct diagram_scene *scene = laid_out(model);
    require(label_bounds(scene, model, "local").width > 0 &&
                label_bounds(scene, model, "wide note").width > 0 &&
                label_bounds(scene, model, "option  [unavailable]").width > 0,
            "single and reversed-span notes and critical sections are all rendered");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
}
static void malformed_sequence_contract(void)
{
    const char *invalid[] = {"sequenceDiagram extra\nparticipant A",
                             "sequenceDiagram\nparticipant",
                             "sequenceDiagram\nA->>: missing",
                             "sequenceDiagram\nactivate",
                             "sequenceDiagram\nactivate A extra",
                             "sequenceDiagram\nNote middle A: wrong",
                             "sequenceDiagram\nNote left A: wrong",
                             "sequenceDiagram\nNote left of A,B: wrong",
                             "sequenceDiagram\nNote over : wrong",
                             "sequenceDiagram\nNote over A,B missing",
                             "sequenceDiagram\nloop x\nand y\nend",
                             "sequenceDiagram\npar x\nelse y\nend",
                             "sequenceDiagram\ncritical x\noption y\nend extra",
                             "sequenceDiagram\nautonumber x",
                             "sequenceDiagram\nautonumber 1000001",
                             "sequenceDiagram\nautonumber 1 x",
                             "sequenceDiagram\nautonumber 1 2 extra",
                             "sequenceDiagram\nparticipant A\nend extra"};
    for (size_t index = 0; index < sizeof invalid / sizeof invalid[0]; ++index)
        reject_source(invalid[index], DIAGRAM_INVALID);
    reject_source("sequenceDiagram\nparticipant A named Person", DIAGRAM_UNSUPPORTED);
    reject_source("sequenceDiagram\nA==>B: unknown arrow", DIAGRAM_UNSUPPORTED);
}
static void flow_statement_contract(void)
{
    struct diagram_model *model = parsed(
        " \t\r\n%% heading\r\nflowchart TB; A[\"quoted; %% literal\"] --> B[\"unknown &Bogus; "
        "#NotAnEntity; \\\\ path\"] %% trailing comment\r\n\r\nsubgraph Empty\nend\n");
    require(diagram_model_node_count(model) == 2 && diagram_model_link_count(model) == 1 &&
                strcmp(diagram_model_text_at(model, diagram_model_node_at(model, 0).label),
                       "quoted; %% literal") == 0,
            "comments and statement separators do not consume quoted label contents");
    struct diagram_scene *scene = laid_out(model);
    require(label_bounds(scene, model, "Empty").width > 0,
            "an empty subgraph retains its visible title beside ordinary nodes");
    diagram_scene_destroy(scene);
    diagram_model_destroy(model);
    const char *invalid[] = {"flowchart TB",
                             "flowchart TB\nsubgraph []\nend",
                             "flowchart TB\nA\nsubgraph A\nend",
                             "flowchart TB\nsubgraph G\nA\nend\nsubgraph G\nend",
                             "flowchart TB\nsubgraph G\nA\nend extra",
                             "flowchart TB\nA->B",
                             "flowchart TB\nA-..->B",
                             "flowchart TB\nA -==> B",
                             "flowchart TB\nA==.=>B",
                             "flowchart TB\nA-- label B",
                             "flowchart TB\nA -->|unclosed B"};
    for (size_t index = 0; index < sizeof invalid / sizeof invalid[0]; ++index)
        reject_source(invalid[index], DIAGRAM_INVALID);
    reject_source("flowchart TB\nsubgraph G\nA\nend\nG-->A", DIAGRAM_UNSUPPORTED);
    reject_source("flowchart TB\nA-->B %%{init: {}}%%", DIAGRAM_UNSUPPORTED);
    reject_source("flowchart TB\nA-------------------->B", DIAGRAM_TOO_LARGE);
    struct text_buffer *_Nullable nested = nullptr;
    require(text_buffer_create(&nested) == TEXT_BUFFER_ACCEPTED, "nested group boundary fixture");
    text_buffer_append_text(nested, "flowchart TB\n");
    for (size_t index = 0; index <= diagram_depth_limit; ++index)
    {
        char line[32];
        int length = snprintf(line, sizeof line, "subgraph G%zu\n", index);
        require(length > 0 && (size_t)length < sizeof line, "nested group identifier fits");
        text_buffer_append(nested, line, (size_t)length);
    }
    reject_source(text_buffer_bytes(nested), DIAGRAM_TOO_LARGE);
    text_buffer_destroy(nested);
}
static void quoted_pipe_labels(void)
{
    const struct
    {
        const char *source;
        const char *expected;
    } labels[] = {{"\"a|b\"", "a|b"},
                  {"\"ab\"", "ab"},
                  {"\"a|b|c\"", "a|b|c"},
                  {"\"a|\\\"b\\\"|c\"", "a|\"b\"|c"},
                  {"\"a|b; c\n[)} %%{literal}\"", "a|b; c\n[)} %%{literal}"},
                  {"\"a|b\\nnext\"", "a|b\nnext"},
                  {"\"a|b #59; &amp;\"", "a|b ; &"},
                  {"\"a|b\\\\end\"", "a|b\\end"},
                  {"plain [)}; %% literal", "plain [)}; %% literal"}};
    for (size_t index = 0; index < sizeof labels / sizeof labels[0]; ++index)
    {
        char source[256];
        int length = snprintf(source, sizeof source,
                              "flowchart TB\nA -->|%s| B -->|plain| C;\nC --> D[\"tail|x\"]\n",
                              labels[index].source);
        require(length > 0 && (size_t)length < sizeof source, "quoted pipe fixture fits");
        struct diagram_model *model = parsed(source);
        require(diagram_model_node_count(model) == 4 && diagram_model_link_count(model) == 3 &&
                    same_text(diagram_model_text_at(model, diagram_model_link_at(model, 0).label),
                              labels[index].expected) &&
                    same_text(diagram_model_text_at(model, diagram_model_link_at(model, 1).label),
                              "plain") &&
                    same_text(diagram_model_text_at(model, diagram_model_node_at(model, 3).label),
                              "tail|x"),
                "pipe labels retain content and do not consume later edges or node labels");
        diagram_model_destroy(model);
    }
    const char *invalid[] = {"flowchart TB\nA -->|\"a|b\" B", "flowchart TB\nA -->|\"a|b| B",
                             "flowchart TB\nA -->|\"a|b\\\"| B",
                             "flowchart TB\nA -->|\"a|b\"| B -->|tail C"};
    for (size_t index = 0; index < sizeof invalid / sizeof invalid[0]; ++index)
        reject_source(invalid[index], DIAGRAM_INVALID);
}
static void attachment_geometry(void)
{
    const enum diagram_shape shapes[] = {DIAGRAM_RECTANGLE,
                                         DIAGRAM_ROUNDED,
                                         DIAGRAM_STADIUM,
                                         DIAGRAM_SUBROUTINE,
                                         DIAGRAM_CYLINDER,
                                         DIAGRAM_CIRCLE,
                                         DIAGRAM_DOUBLE_CIRCLE,
                                         DIAGRAM_DIAMOND,
                                         DIAGRAM_HEXAGON,
                                         DIAGRAM_PARALLELOGRAM,
                                         DIAGRAM_PARALLELOGRAM_REVERSE,
                                         DIAGRAM_TRAPEZOID,
                                         DIAGRAM_TRAPEZOID_REVERSE,
                                         DIAGRAM_ASYMMETRIC,
                                         DIAGRAM_ACTOR};
    const int left_x[] = {17, 17, 17, 17, 17, 17, 17, 17, 17, 68, 66, 68, 66, 115, 17};
    const int right_x[] = {717, 717, 717, 717, 717, 717, 717, 717,
                           717, 668, 666, 666, 668, 717, 717};
    struct diagram_rect bounds = {17, 23, 700, 41};
    for (size_t index = 0; index < sizeof shapes / sizeof shapes[0]; ++index)
    {
        struct diagram_point left =
            diagram_geometry_attachment(bounds, shapes[index], (struct diagram_point){17, 43});
        struct diagram_point right =
            diagram_geometry_attachment(bounds, shapes[index], (struct diagram_point){717, 43});
        require(left.x == left_x[index] && right.x == right_x[index] && left.y == 43 &&
                    right.y == 43,
                "odd-height physical attachment follows each shape's contour");
        struct diagram_point top =
            diagram_geometry_attachment(bounds, shapes[index], (struct diagram_point){367, 23});
        struct diagram_point bottom =
            diagram_geometry_attachment(bounds, shapes[index], (struct diagram_point){367, 64});
        require(top.x == 367 && top.y == 23 && bottom.x == 367 && bottom.y == 64,
                "vertical side midpoints retain their established positions");
    }
}
static void collection_capacity_contract(void)
{
    struct diagram_model *_Nullable model = nullptr;
    require(diagram_model_create(DIAGRAM_FLOWCHART, &model) == DIAGRAM_ACCEPTED,
            "bounded text intern table");
    size_t id = 0;
    for (size_t index = 0; index < diagram_text_limit; ++index)
    {
        char label[24];
        int length = snprintf(label, sizeof label, "label%zu", index);
        require(length > 0 && (size_t)length < sizeof label &&
                    diagram_model_text(model, label, (size_t)length, &id) == DIAGRAM_ACCEPTED,
                "all allowed distinct labels can be interned");
    }
    require(diagram_model_text(model, "overflow", 8, &id) == DIAGRAM_TOO_LARGE &&
                diagram_model_text(model, "label0", 6, &id) == DIAGRAM_ACCEPTED && id == 0,
            "a full text table refuses new strings but still reuses existing strings");
    diagram_model_destroy(model);
    struct diagram_scene *_Nullable scene = nullptr;
    require(diagram_scene_create(&scene) == DIAGRAM_ACCEPTED, "bounded scene fixture");
    struct diagram_primitive item = {.kind = DIAGRAM_PRIMITIVE_SHAPE, .bounds = {0, 0, 1, 1}};
    for (size_t index = 0; index < diagram_primitive_limit; ++index)
        require(diagram_scene_append(scene, &item) == DIAGRAM_ACCEPTED,
                "scene grows through all allowed primitive capacities");
    require(diagram_scene_append(scene, &item) == DIAGRAM_TOO_LARGE &&
                diagram_scene_count(scene) == diagram_primitive_limit &&
                diagram_scene_seal(scene, (struct diagram_size){1, 1}) == DIAGRAM_ACCEPTED,
            "overflow preserves the complete accepted scene without appending a partial item");
    diagram_scene_destroy(scene);
}
/* #912: broken input never breaks the readers. Each seed document is damaged by a fixed
 * sequence of small edits (delete, replace, insert from the syntax's own characters) and read;
 * whatever the reader answers, it is one of the closed outcomes, a model exists exactly when the
 * answer is ACCEPTED, and an accepted model lays out (or is refused) and is destroyed cleanly.
 * The sequence comes from a fixed-seed generator, so every run reads the same inputs. */
static uint32_t mutation_state;
static unsigned mutation_accepted;

static uint32_t mutation_next(void)
{
    mutation_state = mutation_state * 1664525U + 1013904223U;
    return mutation_state >> 8;
}

static size_t mutation_edit(char *_Nonnull source, size_t length, size_t capacity)
{
    static const char alphabet[] = "{}[]()<>|*o.-:\"\n ~=xA1%;,#";
    size_t at = mutation_next() % (length + 1);
    char letter = alphabet[mutation_next() % (sizeof alphabet - 1)];
    uint32_t kind = mutation_next() % 3;
    if (kind == 0 && at < length)
    {
        memmove(source + at, source + at + 1, length - at);
        return length - 1;
    }
    if (kind == 1 && at < length)
    {
        source[at] = letter;
        return length;
    }
    if (length + 1 >= capacity)
        return length;
    memmove(source + at + 1, source + at, length - at + 1);
    source[at] = letter;
    return length + 1;
}

static void mutation_read(const char *_Nonnull source, size_t length)
{
    struct note_text *_Nullable text = nullptr;
    require(note_text_create(source, length, &text) == NOTE_TEXT_ACCEPTED, "mutated source");
    struct diagram_model *_Nullable model = nullptr;
    enum diagram_outcome outcome = mermaid_diagram_create(text, &model);
    require((outcome == DIAGRAM_ACCEPTED) == (model != nullptr) &&
                (outcome == DIAGRAM_ACCEPTED || outcome == DIAGRAM_INVALID ||
                 outcome == DIAGRAM_UNSUPPORTED || outcome == DIAGRAM_TOO_LARGE),
            "a damaged diagram gets one closed answer and no partial model");
    if (model != nullptr)
    {
        ++mutation_accepted;
        struct diagram_metrics *_Nullable metrics = nullptr;
        struct diagram_scene *_Nullable scene = nullptr;
        require(metrics_for(model, &metrics) == DIAGRAM_ACCEPTED, "metrics for a mutated model");
        enum diagram_outcome laid = diagram_layout_create(model, metrics, &scene);
        require((laid == DIAGRAM_ACCEPTED) == (scene != nullptr),
                "the layout of a mutated model is complete or refused");
        diagram_scene_destroy(scene);
        diagram_metrics_destroy(metrics);
        diagram_model_destroy(model);
    }
    note_text_destroy(text);
}

static void mutation_rounds(const char *_Nonnull seed, unsigned rounds)
{
    char source[1024];
    for (unsigned round = 0; round < rounds; ++round)
    {
        size_t length = strlen(seed);
        memcpy(source, seed, length + 1);
        unsigned edits = 1 + mutation_next() % 4;
        for (unsigned edit = 0; edit < edits; ++edit)
            length = mutation_edit(source, length, sizeof source);
        mutation_read(source, length);
    }
}

static void diagram_mutations(void)
{
    static const char *const seeds[] = {
        "flowchart TD\nA[Start] --> B{Check}\nB -->|yes| C((Done))\nsubgraph S\nC --> D\nend\n",
        "sequenceDiagram\nparticipant A\nA->>B: hello\nNote over A,B: text\nloop each\nB-->>A: ok\n"
        "end\n",
        "stateDiagram-v2\n[*] --> A\nA --> B : go\nstate B <<choice>>\nstate \"T\" as C {\n"
        "[*] --> D\nD --> [*]\n}\nB --> [*]\n",
        "erDiagram\nA ||--o{ B : has\nB }|..|| C : \"uses\"\nA {\n  int id PK \"note\"\n"
        "  string name\n}\n",
        "classDiagram\nclass A {\n  <<interface>>\n  +int x\n  +run() void\n}\nA <|-- B : is\n"
        "B *-- C\nC o-- D\nD ..> A\nB : +go()\n"};
    mutation_state = 20261117U;
    mutation_accepted = 0;
    for (size_t index = 0; index < sizeof seeds / sizeof seeds[0]; ++index)
        mutation_rounds(seeds[index], 3000);
    printf("diagram mutations: 15000 damaged inputs, %u still accepted and laid out\n",
           mutation_accepted);
    /* Both sides must be exercised: damage that is refused and damage that still reads. */
    require(mutation_accepted > 1000 && mutation_accepted < 14000,
            "the mutations reach both the refusals and the layout");
}

void run_diagram_tests(void)
{
    diagram_mutations();
    connected_shape_delimiters();
    quoted_shape_delimiters();
    quoted_pipe_labels();
    flow_statement_contract();
    collection_capacity_contract();
    model_construction_contract();
    model_reference_contract();
    metric_input_contract();
    scene_publication_contract();
    palette_key_contract();
    low_contrast_appearance();
    additional_sequence_contract();
    malformed_sequence_contract();
    palette_contrast();
    autonumber_boundary();
    flow_directions();
    flow_shapes();
    state_contract();
    state_composite();
    state_rejections();
    state_reuse();
    state_dots();
    er_contract();
    er_rejections();
    class_contract();
    class_rejections();
    pie_tests();
    gantt_tests();
    groups_and_cycles();
    edge_labels_remain_visible();
    horizontal_group_heading();
    group_heading_clearance();
    shaped_label_clearance();
    stadium_label_clearance();
    sloped_label_clearance();
    attachment_geometry();
    shared_text_roles();
    sequence_features();
    sequence_lifeline_headers();
    sequence_arrow_meaning();
    self_activation_arrival();
    sequence_activation_extents();
    sequence_activation_connections();
    rejected_grammar();
    escaped_labels();
    bounded_input();
    grammar_limits();
    media_limits();
    markdown_contract();
    allocation_contract(flow_source);
    allocation_contract(sequence_source);
    allocation_contract("pie showData title t\n\"a\" : 1\n\"b\" : 2.5\n");
    allocation_contract(gantt_source);
    markdown_allocation_contract("```mermaid\ngraph TB\nA --> B\n```\ntail");
    markdown_allocation_contract("> ```mermaid\n> graph TB\n> A --> B\ntail");
    markdown_allocation_contract("```mermaid\ngraph TB\nA --> B");
}
