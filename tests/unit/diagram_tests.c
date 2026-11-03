#include "allocation_probe.h"
#include "diagram_geometry.h"
#include "diagram_layout.h"
#include "diagram_palette.h"
#include "folio_theme.h"
#include "markdown_media.h"
#include "markdown_rtf.h"
#include "mermaid_diagram.h"
#include "text_buffer.h"
#include "theme_contrast.h"
#include "ui_font.h"
#include "unit_tests.h"
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
void run_diagram_tests(void)
{
    palette_contrast();
    autonumber_boundary();
    flow_directions();
    flow_shapes();
    groups_and_cycles();
    edge_labels_remain_visible();
    horizontal_group_heading();
    group_heading_clearance();
    shaped_label_clearance();
    shared_text_roles();
    sequence_features();
    sequence_arrow_meaning();
    self_activation_arrival();
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
