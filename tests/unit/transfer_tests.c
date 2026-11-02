#include "allocation_probe.h"
#include "appearance_port.h"
#include "folio_state.h"
#include "note_archive.h"
#include "note_text.h"
#include "persisted_size_limit.h"
#include "persistence_port.h"
#include "regex_port.h"
#include "storage_path.h"
#include "transfer_port.h"
#include "unit_tests.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static enum persistence_outcome read_result = PERSISTENCE_LOADED;
static enum persistence_outcome write_result = PERSISTENCE_STORED;
static size_t reads;
static size_t writes;
static char output[4096];
static size_t output_length;
static struct persistence_port source_port;
static enum persistence_outcome create_result;
static enum persistence_outcome ledger_result;
static enum persistence_outcome saved_read_result;

static enum persistence_outcome create_import(struct persistence_adapter *_Nonnull adapter,
                                              const char *_Nonnull category,
                                              const char *_Nonnull name,
                                              const struct note_text *_Nonnull body)
{
    return create_result == PERSISTENCE_STORED
               ? source_port.create_note(adapter, category, name, body)
               : create_result;
}

static enum persistence_outcome write_index(struct persistence_adapter *_Nonnull adapter,
                                            const char *_Nonnull category,
                                            const struct note_ledger *_Nonnull ledger)
{
    return ledger_result == PERSISTENCE_STORED
               ? source_port.write_note_ledger(adapter, category, ledger)
               : ledger_result;
}

static enum persistence_outcome read_saved(struct persistence_adapter *_Nonnull adapter,
                                           const char *_Nonnull category, const char *_Nonnull name,
                                           struct note_text *_Nullable *_Nonnull out)
{
    return saved_read_result == PERSISTENCE_LOADED
               ? source_port.read_note(adapter, category, name, out)
               : saved_read_result;
}

static enum persistence_outcome read_source(const struct storage_path *_Nonnull path,
                                            struct note_text *_Nullable *_Nonnull out)
{
    (void)path;
    reads += 1;
    if (read_result != PERSISTENCE_LOADED)
        return read_result;
    return note_text_create("# imported\r\n", 12, out) == NOTE_TEXT_ACCEPTED
               ? PERSISTENCE_LOADED
               : PERSISTENCE_OUT_OF_MEMORY;
}

static enum persistence_outcome write_note(const struct storage_path *_Nonnull path,
                                           const struct note_text *_Nonnull body)
{
    (void)path;
    writes += 1;
    output_length = note_text_file_length(body);
    require(output_length < sizeof output, "test output capacity");
    memcpy(output, note_text_file_bytes(body), output_length);
    output[output_length] = '\0';
    return write_result;
}

static enum persistence_outcome write_archive(const struct storage_path *_Nonnull path,
                                              const struct note_archive *_Nonnull archive)
{
    (void)path;
    writes += 1;
    output_length = note_archive_length(archive);
    require(output_length < sizeof output, "test ZIP capacity");
    memcpy(output, note_archive_bytes(archive), output_length);
    return write_result;
}

const struct transfer_port *_Nonnull test_transfer_port(void)
{
    static const struct transfer_port port = {
        .read = read_source, .write_note = write_note, .write_archive = write_archive};
    return &port;
}

static struct storage_path *_Nonnull accepted_path(const char *_Nonnull text)
{
    struct storage_path *path = nullptr;
    require(storage_path_create(text, strlen(text), &path) == STORAGE_PATH_READY, "transfer path");
    return path;
}

static struct folio_state *_Nonnull state_for(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port persistence = test_adapter_port(adapter);
    source_port = persistence;
    create_result = PERSISTENCE_STORED;
    ledger_result = PERSISTENCE_STORED;
    saved_read_result = PERSISTENCE_LOADED;
    persistence.create_note = create_import;
    persistence.write_note_ledger = write_index;
    persistence.read_note = read_saved;
    struct appearance_port appearance = test_appearance_port();
    struct regex_port regex = test_regex_port();
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&persistence, &appearance, &regex), &state) ==
                FOLIO_STATE_READY,
            "transfer state");
    return state;
}

static struct persistence_adapter *_Nonnull adapter_for(void)
{
    return test_adapter_create(
        "{\"version\":1,\"categories\":[{\"name\":\"A\",\"color\":\"#112233\",\"expanded\":true}]}",
        "{\"version\":1,\"notes\":[\"one\",\"two\",\"three\"]}");
}

static uint32_t number(const char *_Nonnull bytes)
{
    return (uint32_t)(unsigned char)bytes[0] | ((uint32_t)(unsigned char)bytes[1] << 8) |
           ((uint32_t)(unsigned char)bytes[2] << 16) | ((uint32_t)(unsigned char)bytes[3] << 24);
}

static void verify_archive(void)
{
    struct note_text *body = nullptr;
    require(note_text_create("123456789", 9, &body) == NOTE_TEXT_ACCEPTED, "CRC reference body");
    struct archive_note note = {.category = "旅", .name = "予定", .body = body};
    struct note_archive *archive = nullptr;
    require(note_archive_create(&note, 1, nullptr, &archive) == NOTE_ARCHIVE_READY, "Unicode ZIP");
    const char *bytes = note_archive_bytes(archive);
    require(number(bytes) == UINT32_C(0x04034b50) && number(bytes + 14) == UINT32_C(0xcbf43926),
            "ZIP local record and independent CRC check vector");
    require((unsigned char)bytes[7] == 8 && memcmp(bytes + 30, "旅/予定.md", 13) == 0,
            "ZIP uses UTF-8 flag and relative category path");
    size_t end = note_archive_length(archive) - 22;
    require(number(bytes + end) == UINT32_C(0x06054b50) && (unsigned char)bytes[end + 8] == 1,
            "ZIP complete central directory");
    note_archive_destroy(archive);
    archive = nullptr;
    require(note_archive_create(&note, 0, nullptr, &archive) == NOTE_ARCHIVE_READY &&
                note_archive_length(archive) == 22,
            "empty export is an empty ZIP");
    note_archive_destroy(archive);
    archive = nullptr;
    require(note_archive_create(&note, 65535, nullptr, &archive) == NOTE_ARCHIVE_TOO_LARGE &&
                archive == nullptr,
            "ZIP32 count limit checked before entries are read");
    note.category = "../escape";
    require(note_archive_create(&note, 1, nullptr, &archive) == NOTE_ARCHIVE_INVALID &&
                archive == nullptr,
            "no path traversal in archive");
    note.category = "A";
    note.name = "NUL";
    require(note_archive_create(&note, 1, nullptr, &archive) == NOTE_ARCHIVE_INVALID,
            "unsafe note rejected");
    note_text_destroy(body);
}

static void verify_archive_byte_limit(void)
{
    char *bytes = malloc(persisted_size_limit);
    require(bytes != nullptr, "archive limit fixture allocation");
    memset(bytes, 'x', persisted_size_limit);
    struct note_text *body = nullptr;
    require(note_text_create(bytes, persisted_size_limit, &body) == NOTE_TEXT_ACCEPTED,
            "maximum allowed individual note");
    free(bytes);
    struct archive_note notes[8];
    for (size_t index = 0; index < 8; ++index)
        notes[index] = (struct archive_note){.category = "A", .name = "one", .body = body};
    struct note_archive *archive = nullptr;
    require(note_archive_create(notes, 8, nullptr, &archive) == NOTE_ARCHIVE_TOO_LARGE &&
                archive == nullptr,
            "ZIP byte limit includes framing in addition to eight 16MiB bodies");
    note_text_destroy(body);
}

static void verify_archive_allocation(void)
{
    struct note_text *body = nullptr;
    require(note_text_create("body", 4, &body) == NOTE_TEXT_ACCEPTED, "archive allocation input");
    struct archive_note note = {.category = "A", .name = "one", .body = body};
    size_t allocations = 0;
    for (size_t nth = 0; nth <= allocations; ++nth)
    {
        struct note_archive *archive = nullptr;
        allocation_probe_fail_at(nth);
        enum note_archive_outcome result = note_archive_create(&note, 1, nullptr, &archive);
        if (nth == 0)
            allocations = allocation_probe_calls();
        allocation_probe_fail_at(0);
        require(nth == 0 ? result == NOTE_ARCHIVE_READY
                         : result == NOTE_ARCHIVE_OUT_OF_MEMORY && archive == nullptr,
                "archive never exposes partial memory on allocation failure");
        note_archive_destroy(archive);
    }
    note_text_destroy(body);
}

static void verify_import(void)
{
    struct persistence_adapter *adapter = adapter_for();
    struct folio_state *state = state_for(adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "selected note before import");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "editing before import");
    size_t before = folio_state_note_count(state);
    struct storage_path *paths[] = {
        accepted_path("D:/source/ONE.md"), accepted_path("D:/source/new.MD"),
        accepted_path("D:/source/refused.txt"), accepted_path("D:/source/later.md")};
    struct note_import_request request = {.category = 0, .paths = paths, .count = 4};
    reads = 0;
    read_result = PERSISTENCE_LOADED;
    struct note_import_result result = folio_state_import_notes(state, &request);
    require(result.added == 1 && result.skipped == 1 && result.remaining == 2 &&
                result.outcome == FOLIO_STATE_INVALID_NAME && reads == 1,
            "batch counts duplicate, completion and unprocessed suffix");
    require(folio_state_note_count(state) == before + 1 &&
                same_text(folio_state_pane_title(state).note, "one") &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                test_adapter_body_publications(adapter) == 0,
            "import appends without replacing current editor or creating history");
    request.category = 999;
    result = folio_state_import_notes(state, &request);
    require(result.outcome == FOLIO_STATE_NO_SUCH_CATEGORY && result.added == 0,
            "invalid destination leaves all unprocessed");
    request.category = 0;
    request.paths = &paths[3];
    request.count = 1;
    read_result = PERSISTENCE_MALFORMED;
    result = folio_state_import_notes(state, &request);
    require(result.outcome == FOLIO_STATE_NOTE_MALFORMED && result.remaining == 1,
            "malformed external content is not imported");
    read_result = PERSISTENCE_ABSENT;
    require(folio_state_import_notes(state, &request).outcome == FOLIO_STATE_NOTE_UNREADABLE,
            "missing source is not a successful empty import");
    read_result = PERSISTENCE_LOADED;
    for (size_t index = 0; index < 4; ++index)
        storage_path_destroy(paths[index]);
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
}

static void verify_export(void)
{
    struct persistence_adapter *adapter = adapter_for();
    struct folio_state *state = state_for(adapter);
    struct storage_path *path = accepted_path("D:/exports/result.md");
    struct note_export_request request = {.scope = NOTE_EXPORT_CURRENT,
                                          .destination = path,
                                          .units = u"# unsaved\rline",
                                          .count = 14};
    writes = 0;
    write_result = PERSISTENCE_STORED;
    require(folio_state_export_notes(state, &request) == FOLIO_STATE_NOTHING_SELECTED &&
                writes == 0,
            "no current note has no output");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "export current selection");
    require(folio_state_export_notes(state, &request) == FOLIO_STATE_READY, "view export succeeds");
    require(same_text(output, folio_state_pane_text(state)), "view export is original Markdown");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "edit export source");
    require(folio_state_export_notes(state, &request) == FOLIO_STATE_READY &&
                strstr(output, "# unsaved") != nullptr &&
                test_adapter_body_publications(adapter) == 0 &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "unsaved editor exported without saving or leaving edit");
    write_result = PERSISTENCE_NAME_TAKEN;
    require(folio_state_export_notes(state, &request) == FOLIO_STATE_EXPORT_EXISTS,
            "existing external file is a distinct refusal");
    write_result = PERSISTENCE_UNWRITABLE;
    require(folio_state_export_notes(state, &request) == FOLIO_STATE_EXPORT_FAILED,
            "export write failure is not success");
    write_result = PERSISTENCE_STORED;
    request.scope = NOTE_EXPORT_CATEGORY;
    request.category = 999;
    require(folio_state_export_notes(state, &request) == FOLIO_STATE_NO_SUCH_CATEGORY,
            "invalid category is refused");
    request.category = 0;
    require(folio_state_export_notes(state, &request) == FOLIO_STATE_READY &&
                (unsigned char)output[output_length - 14] == 3,
            "category ZIP has three saved notes");
    request.scope = NOTE_EXPORT_ALL;
    require(folio_state_export_notes(state, &request) == FOLIO_STATE_READY &&
                (unsigned char)output[output_length - 14] == 9,
            "all ZIP covers all categories");
    storage_path_destroy(path);
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
}

static void verify_import_failures(void)
{
    struct persistence_adapter *adapter = adapter_for();
    struct folio_state *state = state_for(adapter);
    struct storage_path *paths[] = {accepted_path("D:/source/new.md"),
                                    accepted_path("D:/source/next.md")};
    struct note_import_request request = {.category = 0, .paths = paths, .count = 2};
    size_t before = folio_state_note_count(state);
    create_result = PERSISTENCE_NAME_TAKEN;
    struct note_import_result result = folio_state_import_notes(state, &request);
    require(result.skipped == 2 && result.added == 0 && result.remaining == 0 &&
                folio_state_note_count(state) == before,
            "disk race collision skips without replacing");
    create_result = PERSISTENCE_UNWRITABLE;
    result = folio_state_import_notes(state, &request);
    require(result.outcome == FOLIO_STATE_NOTE_STORE_FAILED && result.remaining == 2 &&
                folio_state_note_count(state) == before,
            "failed publication leaves index and suffix intact");
    create_result = PERSISTENCE_STORED;
    ledger_result = PERSISTENCE_UNWRITABLE;
    result = folio_state_import_notes(state, &request);
    require(result.outcome == FOLIO_STATE_LEDGER_STALE && result.added == 1 &&
                result.remaining == 1 && folio_state_note_count(state) == before + 1,
            "partial batch counts durable note despite stale index");
    result = folio_state_import_notes(state, &request);
    require(result.outcome == FOLIO_STATE_LEDGER_UNSYNCED && result.remaining == 2 &&
                result.added == 0,
            "pending index prevents another batch");
    ledger_result = PERSISTENCE_STORED;
    result = folio_state_import_notes(state, &request);
    require(result.outcome == FOLIO_STATE_READY && result.skipped == 1 && result.added == 1 &&
                result.remaining == 0,
            "retry repairs index and skips already published note");
    storage_path_destroy(paths[0]);
    storage_path_destroy(paths[1]);
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
}

static void verify_import_search(void)
{
    struct persistence_adapter *adapter = adapter_for();
    struct folio_state *state = state_for(adapter);
    struct storage_path *path = accepted_path("D:/source/new.md");
    struct note_import_request request = {.category = 0, .paths = &path, .count = 1};
    require(test_index_filter_complete(state, u"imported", 8) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0,
            "warm corpus excludes new external body");
    require(folio_state_import_notes(state, &request).outcome == FOLIO_STATE_FILTERED,
            "filtered import is refused");
    require(folio_state_command_availability(state, FOLIO_COMMAND_EXPORT_CATEGORY,
                                             FOLIO_COMMAND_TARGET_CATEGORY,
                                             (struct note_ref){.category = 0}) == FOLIO_STATE_READY,
            "category export remains available for saved notes while filtering");
    struct note_export_request export = {.scope = NOTE_EXPORT_CATEGORY,
                                         .category = 0,
                                         .destination = path,
                                         .units = u"",
                                         .count = 0};
    writes = 0;
    require(folio_state_export_notes(state, &export) == FOLIO_STATE_READY && writes == 1 &&
                (unsigned char)output[output_length - 14] == 3,
            "zero displayed matches still exports the complete saved category");
    require(test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY, "clear filter");
    require(folio_state_import_notes(state, &request).added == 1, "append to warm corpus");
    require(test_index_filter_complete(state, u"imported", 8) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1,
            "imported content enters canonical search cache");
    storage_path_destroy(path);
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
}

static void verify_export_boundaries(void)
{
    struct persistence_adapter *adapter = adapter_for();
    struct folio_state *state = state_for(adapter);
    struct storage_path *path = accepted_path("D:/exports/result.md");
    struct note_export_request request = {
        .scope = NOTE_EXPORT_ALL, .destination = path, .units = u"draft", .count = 5};
    writes = 0;
    saved_read_result = PERSISTENCE_UNREADABLE;
    require(folio_state_export_notes(state, &request) == FOLIO_STATE_NOTE_UNREADABLE && writes == 0,
            "unreadable saved note prevents publishing incomplete archive");
    saved_read_result = PERSISTENCE_LOADED;
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "unnamed export source");
    request.scope = NOTE_EXPORT_CURRENT;
    require(folio_state_export_notes(state, &request) == FOLIO_STATE_READY &&
                same_text(output, "draft") &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_unsaved(state, request.units, request.count) == FOLIO_NOTE_CHANGED,
            "exported draft remains unnamed and unsaved");
    const char16_t invalid[] = {0xD800};
    request.units = invalid;
    request.count = 1;
    writes = 0;
    require(folio_state_export_notes(state, &request) == FOLIO_STATE_NOTE_MALFORMED && writes == 0,
            "malformed editor input does not produce an external file");
    storage_path_destroy(path);
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
}

static void verify_import_allocation(bool warm)
{
    size_t allocations = 0;
    for (size_t nth = 0; nth <= allocations; ++nth)
    {
        struct persistence_adapter *adapter = adapter_for();
        struct folio_state *state = state_for(adapter);
        if (warm)
        {
            require(test_index_filter_complete(state, u"imported", 8) == FOLIO_STATE_READY,
                    "prepare searchable import allocation fixture");
            require(test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY,
                    "clear searchable import allocation fixture");
        }
        struct storage_path *path = accepted_path("D:/source/new.md");
        struct note_import_request request = {.category = 0, .paths = &path, .count = 1};
        size_t before = folio_state_note_count(state);
        allocation_probe_fail_at(nth);
        struct note_import_result result = folio_state_import_notes(state, &request);
        if (nth == 0)
            allocations = allocation_probe_calls();
        allocation_probe_fail_at(0);
        require(nth == 0 ? result.added == 1 : result.outcome == FOLIO_STATE_OUT_OF_MEMORY,
                "import allocation failure is typed");
        require(folio_state_note_count(state) == before + result.added,
                "import index matches publication result");
        storage_path_destroy(path);
        folio_state_destroy(state);
        test_adapter_destroy(adapter);
    }
}

static void verify_export_allocation(void)
{
    size_t allocations = 0;
    for (size_t nth = 0; nth <= allocations; ++nth)
    {
        struct persistence_adapter *adapter = adapter_for();
        struct folio_state *state = state_for(adapter);
        struct storage_path *path = accepted_path("D:/exports/result.zip");
        struct note_export_request request = {.scope = NOTE_EXPORT_CATEGORY,
                                              .category = 0,
                                              .destination = path,
                                              .units = u"",
                                              .count = 0};
        writes = 0;
        allocation_probe_fail_at(nth);
        enum folio_state_outcome result = folio_state_export_notes(state, &request);
        if (nth == 0)
            allocations = allocation_probe_calls();
        allocation_probe_fail_at(0);
        require(nth == 0 ? result == FOLIO_STATE_READY && writes == 1
                         : result == FOLIO_STATE_OUT_OF_MEMORY && writes == 0,
                "archive failure never publishes a partial ZIP");
        storage_path_destroy(path);
        folio_state_destroy(state);
        test_adapter_destroy(adapter);
    }
}

void run_transfer_tests(void)
{
    verify_archive();
    verify_archive_byte_limit();
    verify_archive_allocation();
    verify_import();
    verify_export();
    verify_import_failures();
    verify_import_search();
    verify_export_boundaries();
    verify_import_allocation(false);
    verify_import_allocation(true);
    verify_export_allocation();
}
