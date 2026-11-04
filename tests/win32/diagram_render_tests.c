/* #539: real GDI metrics, bitmap output, cancellation and shared cache. */
#include "diagram_canvas.h"
#include "diagram_geometry.h"
#include "diagram_native_shape.h"
#include "diagram_render.h"
#include "folio_theme.h"
#include "font_bundle.h"
#include "image_adapter.h"
#include "image_cache.h"
#include "mermaid_diagram.h"
#include "note_text.h"
#include "storage_path.h"
#include "utf8_text.h"
#include <objbase.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static unsigned checks;
static enum image_phase current_phase;
static enum image_phase stop_phase;
static unsigned phases[7];
static const char flow[] =
    "flowchart TB\nsubgraph Input [入力と検証]\nA([開始]) --> B{入力は有効?}\nend\n"
    "B -->|はい| C[処理する]\nB -->|いいえ| D[修正する]\nD --> B\nC --> E[(保存)]\n";
static const char sequence[] =
    "sequenceDiagram\nautonumber\nactor U as 利用者\nparticipant A as アプリ\n"
    "participant D as データ\nU->>+A: ノートを開く\nA->>+D: 読み込み\n"
    "D-->>-A: 本文\nNote over A,D: 本文を先に表示\nalt 図を含む\n"
    "A->>+A: 図を描画\nA-->>-A: 完了\nelse 本文のみ\nA->A: 続行\nend\n"
    "A-->>-U: 表示完了\n";
static const char parts[] =
    "flowchart TB\nA[処理する] --> B(利用者が入力した内容を確認してから保存する)\n"
    "B --> C[(保存する)]\nC --> D[(二行のラベル<br>二行目も読み取る)]\n"
    "D --> E[(三行のラベル<br>二行目も読み取る<br>三行目も読み取る)]\n"
    "E --> F((二行のラベル<br>二行目も読み取る))\n"
    "F --> G{利用者が入力した内容を確認してから保存する}\n"
    "G ==>|実行する| H[[Process data]]\nH -.-> I([完了])\n";
static void require(bool value, const char *_Nonnull label)
{
    ++checks;
    if (!value)
    {
        fprintf(stderr, "FAIL: %s\n", label);
        exit(EXIT_FAILURE);
    }
}
static struct note_text *_Nonnull text_for(const char *_Nonnull source)
{
    struct note_text *_Nullable text = nullptr;
    require(note_text_create(source, strlen(source), &text) == NOTE_TEXT_ACCEPTED, "source");
    return text;
}
static struct diagram_request request_for(const struct note_text *_Nonnull source,
                                          enum folio_theme theme)
{
    return (struct diagram_request){.source = source,
                                    .palette = diagram_palette_for(folio_theme_palette(theme)),
                                    .face = "Noto Sans JP"};
}
static bool cancelled(const struct image_work *_Nonnull work)
{
    (void)work;
    return stop_phase != IMAGE_PHASE_IDLE && stop_phase == current_phase;
}
static void progress(const struct image_work *_Nonnull work, enum image_phase phase)
{
    (void)work;
    current_phase = phase;
    ++phases[(size_t)phase];
}
static void write_bitmap(const struct image_bitmap *_Nonnull bitmap, const char *_Nonnull directory,
                         const char *_Nonnull name)
{
    char path[2048];
    require(sprintf_s(path, sizeof path, "%s/%s.bmp", directory, name) > 0, "artifact path");
    wchar_t wide[2048];
    require(MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 2048) > 0, "artifact unicode");
    HANDLE file = CreateFileW(wide, GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
    require(file != INVALID_HANDLE_VALUE, "new bitmap artifact");
    DWORD written = 0;
    size_t length = image_bitmap_length(bitmap);
    require(WriteFile(file, image_bitmap_bytes(bitmap), (DWORD)length, &written, nullptr) &&
                written == length,
            "complete bitmap artifact");
    CloseHandle(file);
}
static struct image_bitmap *_Nonnull render(const struct diagram_request *_Nonnull request,
                                            const struct image_work *_Nonnull work)
{
    struct image_bitmap *_Nullable bitmap = nullptr;
    require(diagram_render_read(request, work, &bitmap) == IMAGE_READY, "native diagram render");
    struct image_size size = image_bitmap_size(bitmap);
    require(size.width > 80 && size.height > 80 &&
                (size_t)size.width * size.height <= 4 * 1024 * 1024,
            "bounded bitmap dimensions");
    return bitmap;
}
static void render_cases(const struct image_work *_Nonnull work, const char *_Nullable output)
{
    const char *sources[] = {flow, sequence, parts};
    const char *names[] = {"flow-light",    "flow-dark",   "sequence-light",
                           "sequence-dark", "parts-light", "parts-dark"};
    const enum folio_theme themes[] = {FOLIO_THEME_LIGHT, FOLIO_THEME_DARK};
    for (size_t index = 0; index < 3; ++index)
    {
        struct note_text *source = text_for(sources[index]);
        for (size_t theme = 0; theme < 2; ++theme)
        {
            struct diagram_request request = request_for(source, themes[theme]);
            memset(phases, 0, sizeof phases);
            struct image_bitmap *bitmap = render(&request, work);
            require(phases[IMAGE_PHASE_DIAGRAM_PARSING] == 1 &&
                        phases[IMAGE_PHASE_DIAGRAM_LAYOUT] == 1 &&
                        phases[IMAGE_PHASE_DIAGRAM_DRAWING] == 1,
                    "three processing phases");
            if (output != nullptr)
                write_bitmap(bitmap, output, names[index * 2 + theme]);
            image_bitmap_destroy(bitmap);
        }
        note_text_destroy(source);
    }
}
static void measured_roles(const struct image_work *_Nonnull work)
{
    struct note_text *source =
        text_for("flowchart TB\nA[同じ] -->|同じ| B[利用者が入力した内容を確認してから保存する]");
    struct diagram_model *_Nullable model = nullptr;
    require(mermaid_diagram_create(source, &model) == DIAGRAM_ACCEPTED, "role measurement model");
    struct diagram_request request = request_for(source, FOLIO_THEME_LIGHT);
    struct diagram_canvas *_Nullable canvas = nullptr;
    require(diagram_canvas_create(&request, &canvas) == IMAGE_READY, "two measured fonts");
    struct diagram_metrics *_Nullable metrics = nullptr;
    require(diagram_canvas_measure(canvas, model, work, &metrics) == IMAGE_READY,
            "actual role-specific measurements");
    size_t label = diagram_model_node_at(model, 0).label;
    struct diagram_size body = diagram_metrics_at(metrics, label, DIAGRAM_TEXT_BODY);
    struct diagram_size caption = diagram_metrics_at(metrics, label, DIAGRAM_TEXT_CAPTION);
    require(caption.width < body.width && caption.height <= body.height,
            "the same interned label has distinct native body and caption metrics");
    struct diagram_size wrapped =
        diagram_metrics_at(metrics, diagram_model_node_at(model, 1).label, DIAGRAM_TEXT_BODY);
    require(wrapped.width <= 200 && wrapped.height > body.height,
            "Japanese labels wrap at the measured 200 logical pixels");
    printf("native text line heights: body=%d caption=%d; wrapped=%dx%d\n", body.height,
           caption.height, wrapped.width, wrapped.height);
    diagram_metrics_destroy(metrics);
    diagram_canvas_destroy(canvas);
    diagram_model_destroy(model);
    note_text_destroy(source);
}
static bool label_inside_shape(enum diagram_shape shape, struct diagram_rect box,
                               struct diagram_rect label)
{
    HDC dc = CreateCompatibleDC(nullptr);
    require(dc != nullptr && BeginPath(dc), "native shape path");
    struct diagram_primitive primitive = {.shape = shape};
    RECT bounds = {box.x * 2, box.y * 2, (box.x + box.width) * 2, (box.y + box.height) * 2};
    diagram_native_shape_draw(dc, &primitive, bounds, 2);
    require(EndPath(dc), "closed native shape path");
    HRGN outline = PathToRegion(dc);
    DeleteDC(dc);
    /* Expand the label by one logical pixel to require clearance from the native stroke. */
    HRGN text = CreateRectRgn((label.x - 1) * 2, (label.y - 1) * 2, (label.x + label.width + 1) * 2,
                              (label.y + label.height + 1) * 2);
    HRGN difference = CreateRectRgn(0, 0, 0, 0);
    require(outline != nullptr && text != nullptr && difference != nullptr, "native shape regions");
    int result = CombineRgn(difference, text, outline, RGN_DIFF);
    RECT outside = {0};
    require(result != ERROR && GetRgnBox(difference, &outside) != ERROR,
            "native shape region difference");
    printf("shape=%u box=%dx%d label=(%d,%d,%d,%d) outside=(%ld,%ld,%ld,%ld) region=%d\n",
           (unsigned)shape, box.width, box.height, label.x, label.y, label.width, label.height,
           outside.left, outside.top, outside.right, outside.bottom, result);
    DeleteObject(difference);
    DeleteObject(text);
    DeleteObject(outline);
    return result == NULLREGION;
}
static bool shape_label(const struct diagram_request *_Nonnull request,
                        const struct image_work *_Nonnull work, const char *_Nullable output,
                        const char *_Nonnull name)
{
    struct image_bitmap *bitmap = render(request, work);
    if (output != nullptr)
        write_bitmap(bitmap, output, name);
    image_bitmap_destroy(bitmap);
    struct diagram_model *_Nullable model = nullptr;
    require(mermaid_diagram_create(request->source, &model) == DIAGRAM_ACCEPTED, "shape model");
    struct diagram_canvas *_Nullable canvas = nullptr;
    require(diagram_canvas_create(request, &canvas) == IMAGE_READY, "shape native font");
    struct diagram_metrics *_Nullable metrics = nullptr;
    require(diagram_canvas_measure(canvas, model, work, &metrics) == IMAGE_READY,
            "shape actual multiline metrics");
    struct diagram_node node = diagram_model_node_at(model, 0);
    struct diagram_size text = diagram_metrics_at(metrics, node.label, DIAGRAM_TEXT_BODY);
    struct diagram_size size = diagram_geometry_node(text, node.shape);
    struct diagram_rect box = {0, 0, size.width, size.height};
    struct diagram_rect label = diagram_geometry_label(box, text, node.shape);
    printf("%s: ", name);
    bool contained = label_inside_shape(node.shape, box, label);
    diagram_metrics_destroy(metrics);
    diagram_canvas_destroy(canvas);
    diagram_model_destroy(model);
    return contained;
}
static void stadium_labels(const struct image_work *_Nonnull work, const char *_Nullable output)
{
    const char *sources[] = {
        "flowchart TB\nA([\"あいうえおかきくけこさしすせ\"])",
        "flowchart TB\nA([\"あいうえおかきくけこさしすせ<br/>あいうえおかきくけこさしすせ<br/>"
        "あいうえおかきくけこさしすせ<br/>あいうえおかきくけこさしすせ<br/>"
        "あいうえおかきくけこさしすせ<br/>あいうえおかきくけこさしすせ<br/>"
        "あいうえおかきくけこさしすせ<br/>あいうえおかきくけこさしすせ\"])",
        "flowchart TB\nA([\"あ<br/>い<br/>う<br/>え<br/>お<br/>か<br/>き<br/>く<br/>け<br/>こ\"])"};
    const char *names[] = {"stadium-single-light", "stadium-single-dark", "stadium-light",
                           "stadium-dark",         "stadium-tall-light",  "stadium-tall-dark"};
    const enum folio_theme themes[] = {FOLIO_THEME_LIGHT, FOLIO_THEME_DARK};
    for (size_t index = 0; index < sizeof sources / sizeof sources[0]; ++index)
    {
        struct note_text *source = text_for(sources[index]);
        for (size_t theme = 0; theme < 2; ++theme)
        {
            struct diagram_request request = request_for(source, themes[theme]);
            require(shape_label(&request, work, output, names[index * 2 + theme]),
                    "multiline stadium label stays inside the curved border");
        }
        note_text_destroy(source);
    }
}
static struct note_text *_Nonnull polygon_source(size_t shape, size_t lines)
{
    const char *openings[] = {"{{", "[/", "[\\", "[/", "[\\", ">"};
    const char *closings[] = {"}}", "/]", "\\]", "\\]", "/]", "]"};
    char source[1024] = "flowchart TB\nA";
    require(strcat_s(source, sizeof source, openings[shape]) == 0 &&
                strcat_s(source, sizeof source, "\"") == 0,
            "polygon fixture opening");
    for (size_t line = 0; line < lines; ++line)
    {
        require(strcat_s(source, sizeof source, "あいうえおかきくけこさしすせ") == 0,
                "polygon fixture line");
        if (line + 1 < lines)
            require(strcat_s(source, sizeof source, "<br/>") == 0, "polygon fixture break");
    }
    require(strcat_s(source, sizeof source, "\"") == 0 &&
                strcat_s(source, sizeof source, closings[shape]) == 0,
            "polygon fixture closing");
    return text_for(source);
}
static size_t polygon_label(const struct image_work *_Nonnull work, const char *_Nullable output,
                            size_t shape, size_t lines)
{
    struct note_text *source = polygon_source(shape, lines);
    const enum folio_theme themes[] = {FOLIO_THEME_LIGHT, FOLIO_THEME_DARK};
    size_t outside = 0;
    for (size_t theme = 0; theme < 2; ++theme)
    {
        char name[64];
        require(sprintf_s(name, sizeof name, "polygon-%zu-%zu-%zu", shape, lines, theme) > 0,
                "polygon artifact name");
        struct diagram_request request = request_for(source, themes[theme]);
        if (!shape_label(&request, work, output, name))
            ++outside;
    }
    note_text_destroy(source);
    return outside;
}
static void polygon_labels(const struct image_work *_Nonnull work, const char *_Nullable output)
{
    size_t outside = 0;
    for (size_t shape = 0; shape < 6; ++shape)
    {
        outside += polygon_label(work, output, shape, 1);
        outside += polygon_label(work, output, shape, 8);
    }
    printf("polygon labels outside the native border: %zu of 24\n", outside);
    require(outside == 0, "polygon labels stay inside the sloping native border");
}
static bool label_tests(const struct image_work *_Nonnull work, int argc,
                        char *_Nonnull *_Nonnull argv)
{
    bool stadium_only = argc >= 2 && strcmp(argv[1], "--stadium") == 0;
    bool polygons_only = argc >= 2 && strcmp(argv[1], "--polygons") == 0;
    bool selected = stadium_only || polygons_only;
    const char *output = selected && argc == 3 ? argv[2] : nullptr;
    if (!polygons_only)
        stadium_labels(work, output);
    if (!stadium_only)
        polygon_labels(work, output);
    return selected;
}
static void cancellation(const struct image_work *_Nonnull work)
{
    struct note_text *source = text_for(sequence);
    struct diagram_request request = request_for(source, FOLIO_THEME_LIGHT);
    const enum image_phase stages[] = {IMAGE_PHASE_DIAGRAM_PARSING, IMAGE_PHASE_DIAGRAM_LAYOUT,
                                       IMAGE_PHASE_DIAGRAM_DRAWING};
    for (size_t index = 0; index < 3; ++index)
    {
        current_phase = IMAGE_PHASE_IDLE;
        stop_phase = stages[index];
        struct image_bitmap *_Nullable bitmap = nullptr;
        require(diagram_render_read(&request, work, &bitmap) == IMAGE_CANCELLED &&
                    bitmap == nullptr,
                "each processing phase cancels without publishing");
    }
    stop_phase = IMAGE_PHASE_IDLE;
    note_text_destroy(source);
}
static enum image_outcome wait_for(const struct image_port *_Nonnull port, uint64_t ticket,
                                   struct image_bitmap *_Nullable *_Nonnull out)
{
    HANDLE timer = CreateWaitableTimerW(nullptr, TRUE, nullptr);
    require(timer != nullptr, "worker deadline timer");
    LARGE_INTEGER due = {.QuadPart = -150000000};
    require(SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE), "relative deadline");
    enum image_outcome result = IMAGE_PENDING;
    while (result == IMAGE_PENDING && WaitForSingleObject(timer, 0) == WAIT_TIMEOUT)
    {
        result = port->poll(port->adapter, ticket, out);
        if (result == IMAGE_PENDING)
            Sleep(1);
    }
    CloseHandle(timer);
    return result;
}
static void worker_contract(const struct image_port *_Nonnull port)
{
    struct note_text *source = text_for(flow);
    struct diagram_request request = request_for(source, FOLIO_THEME_LIGHT);
    char face[64] = "Noto Sans JP";
    request.face = face;
    uint64_t first = 0, second = 0;
    require(port->diagram(port->adapter, &request, &first) == IMAGE_PENDING, "worker submission");
    port->cancel(port->adapter);
    struct image_bitmap *_Nullable bitmap = nullptr;
    require(port->poll(port->adapter, first, &bitmap) == IMAGE_CANCELLED && bitmap == nullptr,
            "cancelled diagram generation cannot publish");
    require(port->diagram(port->adapter, &request, &second) == IMAGE_PENDING && second != first,
            "new diagram generation");
    note_text_destroy(source);
    memset(face, 'x', sizeof face - 1);
    request.palette.background.red ^= 255;
    require(wait_for(port, second, &bitmap) == IMAGE_READY,
            "worker owns source font and palette before caller releases them");
    image_bitmap_destroy(bitmap);
}
static void cache_contract(const struct image_work *_Nonnull work)
{
    struct note_text *source = text_for(flow);
    struct diagram_request request = request_for(source, FOLIO_THEME_LIGHT);
    struct image_job original = {0};
    require(image_job_diagram(&original, &request) == IMAGE_READY, "original key");
    struct image_bitmap *bitmap = render(&request, work);
    struct image_cache *_Nullable cache = nullptr;
    require(image_cache_create(&cache) == IMAGE_READY, "shared cache");
    image_cache_store(cache, &original, nullptr, bitmap);
    struct image_bitmap *_Nullable hit = nullptr;
    original.ticket = 99;
    require(image_cache_get(cache, &original, nullptr, &hit) == IMAGE_READY &&
                image_bitmap_length(hit) == image_bitmap_length(bitmap) &&
                memcmp(image_bitmap_bytes(hit), image_bitmap_bytes(bitmap),
                       image_bitmap_length(bitmap)) == 0,
            "exact warm bitmap independent of ticket");
    image_bitmap_destroy(hit);
    for (size_t index = 0; index < 3; ++index)
    {
        struct image_job changed = {0};
        require(image_job_clone(&changed, &original) == IMAGE_READY, "changed cache key");
        if (index == 0)
            changed.palette.background.red ^= 1;
        if (index == 1)
            strcpy_s(changed.face, sizeof changed.face, "Yu Gothic UI");
        if (index == 2)
        {
            note_text_destroy(changed.diagram);
            changed.diagram = text_for(sequence);
        }
        hit = nullptr;
        require(image_cache_get(cache, &changed, nullptr, &hit) != IMAGE_READY && hit == nullptr,
                "palette font and exact source invalidate cache");
        image_job_destroy(&changed);
    }
    image_cache_destroy(cache);
    image_bitmap_destroy(bitmap);
    image_job_destroy(&original);
    note_text_destroy(source);
}
static struct storage_path *_Nonnull root_path(void)
{
    wchar_t path[2048];
    DWORD length = GetModuleFileNameW(nullptr, path, 2048);
    require(length > 0 && length < 2048, "test executable location");
    wchar_t *_Nullable slash = wcsrchr(path, L'\\');
    require(slash != nullptr, "test root");
    *slash = L'\0';
    struct utf8_text *_Nullable utf8 = nullptr;
    require(utf8_text_create(path, wcslen(path), &utf8) == UTF8_TEXT_CONVERTED, "root UTF8");
    struct storage_path *_Nullable root = nullptr;
    require(storage_path_create(utf8_text_bytes(utf8), utf8_text_length(utf8), &root) ==
                STORAGE_PATH_READY,
            "storage root");
    utf8_text_destroy(utf8);
    return root;
}
int main(int argc, char *_Nonnull *_Nonnull argv)
{
    require(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)), "COM");
    struct font_bundle *_Nullable fonts = nullptr;
    require(font_bundle_register(&fonts) == FONT_BUNDLE_READY, "actual bundled fonts");
    struct storage_path *root = root_path();
    struct image_adapter *_Nullable adapter = nullptr;
    require(image_adapter_create(root, &adapter) == IMAGE_READY, "single media worker");
    struct image_port port = image_adapter_port(adapter);
    struct image_work work = {.adapter = adapter, .cancelled = cancelled, .progress = progress};
    if (!label_tests(&work, argc, argv))
    {
        measured_roles(&work);
        render_cases(&work, argc == 2 ? argv[1] : nullptr);
        cancellation(&work);
        worker_contract(&port);
        cache_contract(&work);
    }
    image_adapter_destroy(adapter);
    storage_path_destroy(root);
    font_bundle_destroy(fonts);
    CoUninitialize();
    printf("diagram render: %u checks passed\n", checks);
    return EXIT_SUCCESS;
}
