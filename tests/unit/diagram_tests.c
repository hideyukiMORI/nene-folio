#include "allocation_probe.h"
#include "diagram_geometry.h"
#include "diagram_layout.h"
#include "diagram_palette.h"
#include "diagram_style.h"
#include "folio_theme.h"
#include "markdown_media.h"
#include "markdown_rtf.h"
#include "mermaid_diagram.h"
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
    static const char *const unsupported[] = {"pie\ntitle x",
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
    require(markdown_rtf_create(text, folio_theme_palette(FOLIO_THEME_LIGHT)->rtf,
                                ui_font_face(FOLIO_LANGUAGE_JA), &rtf) == MARKDOWN_RTF_CONVERTED,
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
            text, folio_theme_palette(FOLIO_THEME_LIGHT)->rtf, "Noto Sans JP", &rtf);
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
    const struct diagram_node bad_nodes[] = {{1, 0, diagram_none, DIAGRAM_RECTANGLE},
                                             {0, 1, diagram_none, DIAGRAM_RECTANGLE},
                                             {0, 0, 0, DIAGRAM_RECTANGLE}};
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
void run_diagram_tests(void)
{
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
    sequence_arrow_meaning();
    self_activation_arrival();
    sequence_activation_extents();
    rejected_grammar();
    escaped_labels();
    bounded_input();
    grammar_limits();
    media_limits();
    markdown_contract();
    allocation_contract(flow_source);
    allocation_contract(sequence_source);
    markdown_allocation_contract("```mermaid\ngraph TB\nA --> B\n```\ntail");
    markdown_allocation_contract("> ```mermaid\n> graph TB\n> A --> B\ntail");
    markdown_allocation_contract("```mermaid\ngraph TB\nA --> B");
}
