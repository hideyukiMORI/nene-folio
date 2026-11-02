#include "allocation_probe.h"
#include "appearance_port.h"
#include "archive_assets.h"
#include "category_name.h"
#include "folio_state.h"
#include "folio_theme.h"
#include "image_bitmap.h"
#include "image_export.h"
#include "image_limits.h"
#include "image_port.h"
#include "markdown_image_copy.h"
#include "markdown_images.h"
#include "markdown_rtf.h"
#include "note_archive.h"
#include "note_text.h"
#include "persistence_port.h"
#include "regex_port.h"
#include "theme_palette.h"
#include "ui_font.h"
#include "unit_tests.h"
#include <stdlib.h>
#include <string.h>
struct image_adapter
{
    uint64_t ticket;
    enum image_phase phase;
    enum image_outcome asset_failure;
    size_t requests;
    size_t assets;
};
static struct image_adapter fake;
static const char *request_category = "A";
static enum rename_outcome rename_result = RENAME_COMPLETED;
static enum image_outcome request(struct image_adapter *_Nonnull adapter,
                                  const struct image_request *_Nonnull input,
                                  uint64_t *_Nonnull ticket)
{
    require(strcmp(input->category, request_category) == 0,
            "application supplies the Markdown directory");
    require(strcmp(image_source_target(input->source), "images/a.png") == 0,
            "one parsed image reference");
    *ticket = ++adapter->ticket;
    adapter->phase = IMAGE_PHASE_READING;
    ++adapter->requests;
    return IMAGE_PENDING;
}
static enum image_outcome poll(struct image_adapter *_Nonnull adapter, uint64_t ticket,
                               struct image_bitmap *_Nullable *_Nonnull out)
{
    (void)out;
    return ticket == adapter->ticket ? IMAGE_PENDING : IMAGE_CANCELLED;
}
static enum image_phase phase(struct image_adapter *_Nonnull adapter)
{
    return adapter->phase;
}
static void cancel(struct image_adapter *_Nonnull adapter)
{
    ++adapter->ticket;
    adapter->phase = IMAGE_PHASE_IDLE;
}
static enum image_outcome asset(struct image_adapter *_Nonnull adapter,
                                const struct image_request *_Nonnull input,
                                enum image_export_policy policy,
                                struct image_asset *_Nullable *_Nonnull out)
{
    ++adapter->assets;
    if (adapter->asset_failure != IMAGE_READY)
        return adapter->asset_failure;
    const char *target = image_source_target(input->source);
    if (strcmp(target, "images/missing.png") == 0)
        return IMAGE_FAILED;
    bool contained = image_source_recommended(input->source);
    if (!contained && policy != IMAGE_EXPORT_ALL_LOCAL)
        return IMAGE_SKIPPED;
    static const unsigned char bytes[] = {137, 'P', 'N', 'G', 0, 1, 2, 3};
    struct image_asset_input source = {.bytes = bytes,
                                       .length = sizeof bytes,
                                       .identity = target,
                                       .relative = contained ? target : ""};
    return image_asset_create(&source, out);
}
const struct image_port *_Nonnull test_image_port(void)
{
    static const struct image_port port = {.adapter = &fake,
                                           .request = request,
                                           .poll = poll,
                                           .phase = phase,
                                           .cancel = cancel,
                                           .asset = asset};
    return &port;
}
static struct note_text *_Nonnull text(const char *_Nonnull source)
{
    struct note_text *_Nullable body = nullptr;
    require(note_text_create(source, strlen(source), &body) == NOTE_TEXT_ACCEPTED,
            "image source text");
    return body;
}
static struct markdown_rtf *_Nonnull render(const struct note_text *_Nonnull body)
{
    struct markdown_rtf *_Nullable rendered = nullptr;
    require(markdown_rtf_create(body, folio_theme_palette(FOLIO_THEME_LIGHT)->rtf,
                                ui_font_face(FOLIO_LANGUAGE_JA),
                                &rendered) == MARKDOWN_RTF_CONVERTED,
            "image render");
    return rendered;
}
static void source_contract(void)
{
    static const struct
    {
        const char *_Nonnull input;
        const char *_Nonnull target;
        enum image_source_kind kind;
        bool recommended;
    } cases[] = {
        {"images/a.png", "images/a.png", IMAGE_SOURCE_LOCAL, true},
        {"./images/a%20b.png", "./images/a b.png", IMAGE_SOURCE_LOCAL, true},
        {"images/x%2520.png", "images/x%20.png", IMAGE_SOURCE_LOCAL, true},
        {"images/../a.png", "images/../a.png", IMAGE_SOURCE_LOCAL, false},
        {"C:/pics/a.png", "C:/pics/a.png", IMAGE_SOURCE_LOCAL, false},
        {"https://example.com/a.png?q=1", "https://example.com/a.png?q=1", IMAGE_SOURCE_HTTPS,
         false},
        {"http://example.com/a.png", "http://example.com/a.png", IMAGE_SOURCE_UNSUPPORTED, false},
        {"//server/a.png", "//server/a.png", IMAGE_SOURCE_UNSUPPORTED, false},
        {"C:relative.png", "C:relative.png", IMAGE_SOURCE_UNSUPPORTED, false},
        {"images/a.png:ads", "images/a.png:ads", IMAGE_SOURCE_UNSUPPORTED, false}};
    for (size_t index = 0; index < sizeof cases / sizeof cases[0]; ++index)
    {
        struct image_source *_Nullable source = nullptr;
        require(image_source_create(cases[index].input, (struct note_search_span){4, 8}, &source) ==
                    IMAGE_READY,
                "image reference has a typed classification");
        require(image_source_kind(source) == cases[index].kind &&
                    image_source_recommended(source) == cases[index].recommended &&
                    strcmp(image_source_target(source), cases[index].target) == 0,
                "local and HTTPS contract");
        struct image_source *_Nullable copy = nullptr;
        require(image_source_copy(source, &copy) == IMAGE_READY, "request owns a copy");
        image_source_destroy(source);
        require(strcmp(image_source_target(copy), cases[index].target) == 0,
                "copy decodes percent only once");
        image_source_destroy(copy);
    }
}
static void parser_contract(void)
{
    const char source[] =
        "`![skip](images/code.png)`\r\n\\![skip](images/escape.png)\r\n\r\n"
        "> - ![first](<C:/A%20B/a.png> \"title\")\r\n\r\n"
        "| a | b |\r\n| --- | --- |\r\n| ![second](images/a\\(1\\).png) | "
        "[![nested](https://example.com/x.png?q=1&amp;b=2)](https://example.com) |\r\n";
    struct note_text *body = text(source);
    struct markdown_rtf *rendered = render(body);
    const struct markdown_images *images = markdown_rtf_images(rendered);
    require(markdown_images_count(images) == 3,
            "code and escaped markers excluded, containers and linked image retained");
    const char *raw[] = {"C:/A%20B/a.png", "images/a\\(1\\).png",
                         "https://example.com/x.png?q=1&amp;b=2"};
    const char *targets[] = {"C:/A B/a.png", "images/a(1).png",
                             "https://example.com/x.png?q=1&b=2"};
    for (size_t index = 0; index < 3; ++index)
    {
        const struct image_source *image = markdown_images_at(images, index);
        struct note_search_span span = image_source_span(image);
        require(span.end - span.start == strlen(raw[index]) &&
                    memcmp(source + span.start, raw[index], strlen(raw[index])) == 0,
                "reference spans address original bytes across CRLF, list and table wrappers");
        require(strcmp(image_source_target(image), targets[index]) == 0,
                "destination is decoded once");
    }
    require(markdown_images_advisory(images), "external local image triggers quiet advice");
    require(strstr(markdown_rtf_text(rendered), "<nenefolio:image:0>") != nullptr,
            "native identity marker");
    struct image_relocation relocation = {.source =
                                              image_source_span(markdown_images_at(images, 0)),
                                          .destination = "images/new space.png"};
    struct note_text *_Nullable copy = nullptr;
    require(markdown_image_copy_create(body, &relocation, 1, &copy) == NOTE_TEXT_ACCEPTED,
            "export copy");
    require(strstr(note_text_bytes(copy), "> - ![first](<images/new%20space.png> \"title\")\r\n") !=
                nullptr,
            "only destination changes; angle, title, container and CRLF survive");
    require(strcmp(note_text_bytes(body), source) == 0, "original Markdown remains byte exact");
    note_text_destroy(copy);
    markdown_rtf_destroy(rendered);
    note_text_destroy(body);
}
static void image_limits(void)
{
    static const unsigned char pixels[] = {0, 32, 64, 255, 128, 64, 32, 128};
    struct image_bitmap *_Nullable bitmap = nullptr;
    require(image_bitmap_create(pixels, (struct image_size){0, 1}, &bitmap) == IMAGE_TOO_LARGE &&
                bitmap == nullptr,
            "zero dimension rejected");
    require(image_bitmap_create(pixels, (struct image_size){2049, 1}, &bitmap) == IMAGE_TOO_LARGE,
            "display dimension cap");
    require(image_bitmap_create(pixels, (struct image_size){2, 1}, &bitmap) == IMAGE_READY,
            "BGRA bitmap");
    struct image_bitmap *_Nullable copy = nullptr;
    require(image_bitmap_clone(bitmap, &copy) == IMAGE_READY, "cache bitmap clone");
    require(image_bitmap_length(copy) == image_bitmap_length(bitmap) &&
                image_bitmap_size(copy).width == 2 &&
                memcmp(image_bitmap_bytes(copy), image_bitmap_bytes(bitmap),
                       image_bitmap_length(copy)) == 0,
            "cache retains exact alpha-capable BMP representation");
    image_bitmap_destroy(copy);
    image_bitmap_destroy(bitmap);
    const char line[] = "![x](images/a.png)\n\n";
    char *source = malloc((sizeof line - 1) * 260 + 1);
    require(source != nullptr, "many images fixture");
    for (size_t index = 0; index < 260; ++index)
        memcpy(source + index * (sizeof line - 1), line, sizeof line - 1);
    source[(sizeof line - 1) * 260] = '\0';
    struct note_text *body = text(source);
    struct markdown_rtf *rendered = render(body);
    require(markdown_images_count(markdown_rtf_images(rendered)) == 260,
            "export collects beyond display cap");
    require(strstr(markdown_rtf_text(rendered), "<nenefolio:image:255>") != nullptr &&
                strstr(markdown_rtf_text(rendered), "<nenefolio:image:256>") == nullptr,
            "only 256 native placeholders");
    markdown_rtf_destroy(rendered);
    note_text_destroy(body);
    free(source);
}
static void export_contract(void)
{
    const char source[] = "![inside](images/a.png)\r\n![same](images/a.png)\r\n"
                          "![one](../one/a.png)\r\n![two](../two/a.png)\r\n"
                          "![web](https://example.com/a.png)\r\n";
    struct note_text *body = text(source);
    struct archive_note entry = {.category = "A", .name = "one", .body = body};
    struct image_export *_Nullable images = nullptr;
    fake.assets = 0;
    require(image_export_create(test_image_port(), IMAGE_EXPORT_UNDECIDED, 1, &images) ==
                FOLIO_STATE_READY,
            "prepare export before outside-image choice");
    require(image_export_append(images, &entry) == FOLIO_STATE_READY &&
                image_export_external(images),
            "outside references need an explicit choice");
    require(archive_assets_count(image_export_assets(images)) == 1 && entry.body == body,
            "automatic inclusion is images/ only, deduplicated, no source rewrite");
    image_export_destroy(images);
    images = nullptr;
    require(image_export_create(test_image_port(), IMAGE_EXPORT_ALL_LOCAL, 1, &images) ==
                FOLIO_STATE_READY,
            "explicit local inclusion");
    require(image_export_append(images, &entry) == FOLIO_STATE_READY, "copy local references");
    const struct archive_assets *files = image_export_assets(images);
    require(archive_assets_count(files) == 3,
            "same basename from different directories never overwrites");
    require(strcmp(archive_assets_at(files, 1)->path, archive_assets_at(files, 2)->path) != 0,
            "external image destinations are distinct");
    require(strstr(note_text_bytes(entry.body), "https://example.com/a.png") != nullptr &&
                fake.assets == 8,
            "HTTPS remains a URL and is never downloaded for export");
    require(strcmp(note_text_bytes(body), source) == 0, "original source survives inclusion");
    struct note_archive *_Nullable archive = nullptr;
    require(note_archive_create(&entry, 1, files, &archive) == NOTE_ARCHIVE_READY,
            "binary images share the ZIP path");
    require(note_archive_length(archive) > note_text_length(entry.body),
            "ZIP contains image records");
    note_archive_destroy(archive);
    image_export_destroy(images);
    note_text_destroy(body);
}
static void export_failures(void)
{
    struct note_text *body = text("![lost](images/missing.png)");
    struct archive_note entry = {.category = "A", .name = "one", .body = body};
    struct image_export *_Nullable images = nullptr;
    require(image_export_create(test_image_port(), IMAGE_EXPORT_CONTAINED, 1, &images) ==
                FOLIO_STATE_READY,
            "missing image export");
    require(image_export_append(images, &entry) == FOLIO_STATE_EXPORT_IMAGE_UNREADABLE,
            "required image is never silently omitted");
    image_export_destroy(images);
    note_text_destroy(body);
    struct archive_assets *_Nullable files = nullptr;
    require(archive_assets_create(&files) == NOTE_ARCHIVE_READY, "archive paths");
    struct archive_file file = {.path = "A/images/a.png", .bytes = "x", .length = 1};
    require(archive_assets_append(files, file) == NOTE_ARCHIVE_READY, "safe image path");
    file.path = "a/IMAGES/A.png";
    require(archive_assets_append(files, file) == NOTE_ARCHIVE_INVALID, "case collision refused");
    file.path = "A/../escape.png";
    require(archive_assets_append(files, file) == NOTE_ARCHIVE_INVALID, "ZIP traversal refused");
    file.path = "A/images/";
    require(archive_assets_append(files, file) == NOTE_ARCHIVE_INVALID, "empty leaf refused");
    archive_assets_destroy(files);
}
static enum persistence_outcome read_image_note(struct persistence_adapter *_Nonnull adapter,
                                                const char *_Nonnull category,
                                                const char *_Nonnull name,
                                                struct note_text *_Nullable *_Nonnull out)
{
    (void)adapter;
    (void)category;
    (void)name;
    const char source[] = "![x](images/a.png)";
    return note_text_create(source, sizeof source - 1, out) == NOTE_TEXT_ACCEPTED
               ? PERSISTENCE_LOADED
               : PERSISTENCE_OUT_OF_MEMORY;
}
static enum rename_outcome image_rename(struct persistence_adapter *_Nonnull adapter,
                                        const struct rename_plan *_Nonnull plan,
                                        enum rename_attempt attempt)
{
    (void)adapter;
    (void)plan;
    (void)attempt;
    return rename_result;
}
static void rename_origin(struct folio_state *_Nonnull state, uint64_t *_Nonnull ticket)
{
    struct category_name *_Nullable name = nullptr;
    require(category_name_create("renamed", 7, &name) == CATEGORY_NAME_ACCEPTED,
            "category rename target");
    struct category_rename_target target = {.category = 0, .name = name};
    uint64_t revision = folio_state_image_origin_revision(state);
    rename_result = RENAME_PENDING;
    require(folio_state_rename_category(state, &target, u"", 0) == FOLIO_STATE_RENAME_PENDING &&
                folio_state_image_origin_revision(state) > revision,
            "rename of another category invalidates relative references too");
    struct image_bitmap *_Nullable bitmap = nullptr;
    require(folio_state_poll_image(state, *ticket, &bitmap) == IMAGE_CANCELLED &&
                folio_state_request_image(state, 0, ticket) == IMAGE_CANCELLED,
            "pending directory rename rejects old and new image work");
    revision = folio_state_image_origin_revision(state);
    rename_result = RENAME_COMPLETED;
    require(folio_state_retry_rename(state) == FOLIO_STATE_READY &&
                folio_state_image_origin_revision(state) > revision &&
                folio_state_request_image(state, 0, ticket) == IMAGE_PENDING,
            "completed rename permits image retry with new revision");
    category_name_destroy(name);
}
static void state_cancellation(void)
{
    struct persistence_adapter *adapter = test_adapter_create(
        "{\"version\":1,\"categories\":[{\"name\":\"A\",\"color\":\"#112233\",\"expanded\":true}]}",
        "{\"version\":1,\"notes\":[\"one\",\"two\",\"three\"]}");
    struct persistence_port disk = test_adapter_port(adapter);
    static const char *const empty[] = {nullptr};
    test_adapter_second_notes(adapter, "B", "{\"version\":1,\"notes\":[]}", empty);
    disk.read_note = read_image_note;
    disk.apply_rename = image_rename;
    struct appearance_port appearance = test_appearance_port();
    struct regex_port regex = test_regex_port();
    struct folio_state *_Nullable state = nullptr;
    require(folio_state_create(test_ports(&disk, &appearance, &regex), &state) == FOLIO_STATE_READY,
            "image application state");
    uint64_t ticket = 0;
    require(folio_state_request_image(state, 0, &ticket) == IMAGE_CANCELLED,
            "no-document request rejected");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select image note");
    require(folio_state_request_image(state, 0, &ticket) == IMAGE_PENDING &&
                folio_state_image_phase(state) == IMAGE_PHASE_READING,
            "typed background request");
    struct image_bitmap *_Nullable bitmap = nullptr;
    require(folio_state_poll_image(state, ticket, &bitmap) == IMAGE_PENDING && bitmap == nullptr,
            "pending image never blocks body");
    uint64_t revision = folio_state_image_origin_revision(state);
    require(folio_state_move_note(state, (struct note_ref){0, 0}, (struct note_ref){1, 0}) ==
                    FOLIO_STATE_READY &&
                folio_state_image_origin_revision(state) > revision,
            "selected note movement changes relative image origin");
    require(folio_state_poll_image(state, ticket, &bitmap) == IMAGE_CANCELLED,
            "old directory request cancelled after move");
    request_category = "B";
    require(folio_state_request_image(state, 0, &ticket) == IMAGE_PENDING,
            "moved image resolves against new Markdown directory");
    rename_origin(state, &ticket);
    request_category = "A";
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "edit while image is pending");
    require(folio_state_poll_image(state, ticket, &bitmap) == IMAGE_CANCELLED &&
                folio_state_request_image(state, 0, &ticket) == IMAGE_CANCELLED,
            "edit cancels and rejects old work");
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
}
static enum folio_state_outcome allocation_export(const struct note_text *_Nonnull body)
{
    struct image_export *_Nullable images = nullptr;
    enum folio_state_outcome outcome =
        image_export_create(test_image_port(), IMAGE_EXPORT_ALL_LOCAL, 1, &images);
    struct archive_note entry = {.category = "A", .name = "one", .body = body};
    if (outcome == FOLIO_STATE_READY)
        outcome = image_export_append(images, &entry);
    struct note_archive *_Nullable archive = nullptr;
    if (outcome == FOLIO_STATE_READY &&
        note_archive_create(&entry, 1, image_export_assets(images), &archive) != NOTE_ARCHIVE_READY)
        outcome = FOLIO_STATE_OUT_OF_MEMORY;
    note_archive_destroy(archive);
    image_export_destroy(images);
    return outcome;
}
static void allocation_contract(void)
{
    struct note_text *body =
        text("> ![x](<C:/A%20B/a.png> \"title\")\r\n\r\n![local](images/a.png)");
    size_t allocations = 0;
    for (size_t nth = 0; nth <= allocations; ++nth)
    {
        allocation_probe_fail_at(nth);
        enum folio_state_outcome outcome = allocation_export(body);
        if (nth == 0)
            allocations = allocation_probe_calls();
        allocation_probe_fail_at(0);
        require(nth == 0 ? outcome == FOLIO_STATE_READY : outcome == FOLIO_STATE_OUT_OF_MEMORY,
                "image and ZIP allocation failure never exposes a partial result");
    }
    note_text_destroy(body);
}
void run_image_tests(void)
{
    source_contract();
    parser_contract();
    image_limits();
    export_contract();
    export_failures();
    state_cancellation();
    allocation_contract();
}
