#include "allocation_probe.h"
#include "json_writer.h"
#include "startup_settings.h"
#include "storage_path.h"
#include "unit_tests.h"
#include <string.h>

static void expect_path(const char *_Nonnull source, const char *_Nonnull normalized)
{
    struct storage_path *path = nullptr;
    require(storage_path_create(source, strlen(source), &path) == STORAGE_PATH_READY,
            "absolute storage path is accepted");
    require(same_text(storage_path_text(path), normalized), "storage path spelling is normalized");
    require(storage_path_length(path) == strlen(normalized), "normalized path length");
    struct storage_path *copy = nullptr;
    require(storage_path_copy(path, &copy) == STORAGE_PATH_READY && storage_path_equal(path, copy),
            "path copy owns the same value");
    storage_path_destroy(path);
    require(same_text(storage_path_text(copy), normalized), "copy survives its source");
    storage_path_destroy(copy);
}

static void verify_paths(void)
{
    expect_path("c:/notes/", "C:\\notes");
    expect_path("D:/", "D:\\");
    expect_path("\\\\server\\share\\folder", "\\\\server\\share\\folder");
    expect_path("//server/share/", "\\\\server\\share");
    expect_path("D:/日本語/ノート", "D:\\日本語\\ノート");
    static const char *const invalid[] = {"",
                                          "a",
                                          "data",
                                          "C:notes",
                                          "/notes",
                                          "1:/notes",
                                          "//s",
                                          "//s/",
                                          "//s//share",
                                          "D:/a//b",
                                          "D:/.",
                                          "D:/..",
                                          "D:/a/../b",
                                          "D:/a.",
                                          "D:/a /b",
                                          "D:/bad:*?",
                                          "D:/\"",
                                          "D:/<",
                                          "D:/>|",
                                          "D:/\x01",
                                          "D:/\x7f",
                                          "D:/\xc0\xaf",
                                          "\\\\?\\D:\\notes"};
    for (size_t at = 0; at < sizeof invalid / sizeof invalid[0]; ++at)
    {
        struct storage_path *path = nullptr;
        require(storage_path_create(invalid[at], strlen(invalid[at]), &path) ==
                        STORAGE_PATH_INVALID &&
                    path == nullptr,
                "invalid path is rejected without publication");
    }
    const char embedded[] = "D:/a\0b";
    struct storage_path *path = nullptr;
    require(storage_path_create(embedded, sizeof embedded - 1, &path) == STORAGE_PATH_INVALID,
            "embedded NUL cannot truncate a configured directory");
    char large[storage_path_byte_limit + 2];
    memset(large, 'a', sizeof large);
    memcpy(large, "D:/", 3);
    require(storage_path_create(large, storage_path_byte_limit, &path) == STORAGE_PATH_READY,
            "maximum UTF-8 storage path is accepted");
    struct storage_path *unchanged = path;
    require(storage_path_create(large, storage_path_byte_limit + 1, &unchanged) ==
                    STORAGE_PATH_INVALID &&
                unchanged == path,
            "overlong path leaves an existing output untouched");
    storage_path_destroy(path);
}

static void round_trip(const char *_Nonnull document, const char *_Nullable expected)
{
    struct startup_settings *settings = nullptr;
    require(startup_settings_parse(document, strlen(document), &settings) == STARTUP_SETTINGS_READY,
            "version one startup settings load");
    const struct storage_path *path = startup_settings_directory(settings);
    require(expected == nullptr ? path == nullptr
                                : path != nullptr && same_text(storage_path_text(path), expected),
            "startup root is explicit or default");
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "startup writer created");
    startup_settings_write(settings, writer);
    require(json_writer_finish(writer) == JSON_WRITER_ACCEPTED, "startup writer finishes");
    struct startup_settings *again = nullptr;
    require(startup_settings_parse(json_writer_text(writer), json_writer_length(writer), &again) ==
                    STARTUP_SETTINGS_READY &&
                startup_settings_equal(settings, again),
            "startup settings survive canonical encoding");
    startup_settings_destroy(settings);
    startup_settings_destroy(again);
    json_writer_destroy(writer);
}

static void reject_document(const char *_Nonnull document, enum startup_settings_outcome result)
{
    struct startup_settings *out = nullptr;
    require(startup_settings_parse(document, strlen(document), &out) == result && out == nullptr,
            "malformed or unsupported startup settings do not become defaults");
}

static void verify_codec(void)
{
    round_trip("{\"version\":1,\"directory\":\"\"}", nullptr);
    round_trip("{\"version\":1,\"directory\":\"d:/日本語\"}", "D:\\日本語");
    round_trip("{\"version\":1,\"directory\":\"\\\\\\\\server\\\\share\"}", "\\\\server\\share");
    static const char *const malformed[] = {
        "",
        "[]",
        "{}",
        "{\"directory\":\"D:/x\",\"version\":1}",
        "{\"version\":true,\"directory\":\"\"}",
        "{\"version\":1}",
        "{\"version\":1,\"directory\":null}",
        "{\"version\":1,\"directory\":\"relative\"}",
        "{\"version\":1,\"directory\":\"D:/x\",\"extra\":0}",
        "{\"version\":1,\"directory\":\"D:/x\",\"directory\":\"D:/y\"}",
        "{\"version\":1,\"directory\":\"D:/x\"}{}",
        "{\"version\":1,\"directory\":\"D:/x",
        "{\"version\":1,\"directory\":\"D:/a\\u0000b\"}"};
    for (size_t at = 0; at < sizeof malformed / sizeof malformed[0]; ++at)
        reject_document(malformed[at], STARTUP_SETTINGS_MALFORMED);
    reject_document("{\"version\":2,\"directory\":\"D:/x\"}", STARTUP_SETTINGS_UNSUPPORTED_VERSION);
    reject_document("{\"version\":0}", STARTUP_SETTINGS_UNSUPPORTED_VERSION);
    struct startup_settings *empty = nullptr;
    struct startup_settings *custom = nullptr;
    require(startup_settings_create(nullptr, &empty) == STARTUP_SETTINGS_READY, "default settings");
    const char *source = "{\"version\":1,\"directory\":\"D:/x\"}";
    require(startup_settings_parse(source, strlen(source), &custom) == STARTUP_SETTINGS_READY,
            "custom settings");
    require(!startup_settings_equal(empty, custom) && !startup_settings_equal(custom, empty),
            "default and custom roots differ in both comparison orders");
    startup_settings_destroy(empty);
    startup_settings_destroy(custom);
}

static void verify_codec_allocation(void)
{
    const char *text = "{\"version\":1,\"directory\":\"D:/notes\"}";
    struct startup_settings *settings = nullptr;
    allocation_probe_fail_at(0);
    require(startup_settings_parse(text, strlen(text), &settings) == STARTUP_SETTINGS_READY,
            "allocation baseline parses");
    size_t calls = allocation_probe_calls();
    startup_settings_destroy(settings);
    for (size_t nth = 1; nth <= calls; ++nth)
    {
        settings = nullptr;
        allocation_probe_fail_at(nth);
        enum startup_settings_outcome result =
            startup_settings_parse(text, strlen(text), &settings);
        allocation_probe_fail_at(0);
        require(result == STARTUP_SETTINGS_OUT_OF_MEMORY && settings == nullptr,
                "every startup codec allocation failure leaves output unowned");
    }
}

void run_storage_tests(void)
{
    verify_paths();
    verify_codec();
    verify_codec_allocation();
}
