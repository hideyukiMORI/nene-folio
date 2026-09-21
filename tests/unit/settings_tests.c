/* core の folio_settings（ADR 0025）。版 1 の往復・既定値・拒む形・未知の版を確かめる。 */
#include "folio_settings.h"
#include "json_writer.h"
#include "unit_tests.h"

#include <string.h>

static const char *const version_one = "{\n  \"version\": 1,\n  \"number\": true\n}";

static void expect_refusal(const char *_Nonnull text, enum folio_settings_outcome expected,
                           const char *_Nonnull description)
{
    struct folio_settings *settings = nullptr;
    require(folio_settings_parse(text, strlen(text), &settings) == expected, description);
    require(settings == nullptr, "a refused document leaves no settings");
}

static void verify_default(void)
{
    struct folio_settings *settings = nullptr;
    require(folio_settings_default(&settings) == FOLIO_SETTINGS_READY, "the default is available");
    require(!folio_settings_number(settings), "the default hides the line numbers");
    folio_settings_destroy(settings);
}

/* 書いた文書をもう一度読んで同じ値になるか（キーの順序と整形も版 1 の形のまま）。 */
static void verify_round_trip(bool number)
{
    struct folio_settings *settings = nullptr;
    require(folio_settings_default(&settings) == FOLIO_SETTINGS_READY, "default");
    struct folio_settings *changed = nullptr;
    require(folio_settings_with_number(settings, number, &changed) == FOLIO_SETTINGS_READY,
            "a derived copy carries the new value");
    folio_settings_destroy(settings);
    require(folio_settings_number(changed) == number, "the copy holds the asked value");
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "writer");
    folio_settings_write(changed, writer);
    folio_settings_destroy(changed);
    require(json_writer_finish(writer) == JSON_WRITER_ACCEPTED, "the document is complete");
    struct folio_settings *reread = nullptr;
    require(folio_settings_parse(json_writer_text(writer), json_writer_length(writer), &reread) ==
                FOLIO_SETTINGS_READY,
            "what was written parses back");
    require(folio_settings_number(reread) == number, "the value survives the round trip");
    folio_settings_destroy(reread);
    json_writer_destroy(writer);
}

static void verify_parse(void)
{
    struct folio_settings *settings = nullptr;
    require(folio_settings_parse(version_one, strlen(version_one), &settings) ==
                FOLIO_SETTINGS_READY,
            "version 1 is accepted");
    require(folio_settings_number(settings), "the stored value is read back");
    folio_settings_destroy(settings);
}

/* 黙って既定値へ落とさない形（決定 1）。無い・壊れているは呼び出し側が区別する。 */
static void verify_refusals(void)
{
    expect_refusal("{\"version\": 2, \"number\": true}", FOLIO_SETTINGS_UNSUPPORTED_VERSION,
                   "a newer version is refused, not downgraded");
    expect_refusal("{\"version\": 0, \"number\": true}", FOLIO_SETTINGS_UNSUPPORTED_VERSION,
                   "version 0 is not version 1");
    expect_refusal("{\"version\": 1}", FOLIO_SETTINGS_MALFORMED, "a missing key is refused");
    expect_refusal("{\"number\": true}", FOLIO_SETTINGS_MALFORMED, "a missing version is refused");
    expect_refusal("{\"version\": 1, \"number\": true, \"theme\": \"dark\"}",
                   FOLIO_SETTINGS_MALFORMED, "an unknown key is refused (decision 1)");
    expect_refusal("{\"version\": 1, \"number\": true, \"number\": false}",
                   FOLIO_SETTINGS_MALFORMED, "a duplicated key is refused");
    expect_refusal("{\"version\": 1, \"number\": 1}", FOLIO_SETTINGS_MALFORMED,
                   "a number where a boolean belongs is refused");
    expect_refusal("{\"version\": 1, \"number\": null}", FOLIO_SETTINGS_MALFORMED,
                   "null is not a boolean, and it does not fall back to the default");
    expect_refusal("{\"version\": 1, \"number\": {\"value\": true}}", FOLIO_SETTINGS_MALFORMED,
                   "a nested object where a boolean belongs is refused");
    expect_refusal("{\"version\": \"1\", \"number\": true}", FOLIO_SETTINGS_MALFORMED,
                   "a string version is refused");
    expect_refusal("{\"number\": true, \"version\": 1}", FOLIO_SETTINGS_MALFORMED,
                   "the key order is part of the shape");
    expect_refusal("", FOLIO_SETTINGS_MALFORMED, "an empty file is refused");
    expect_refusal("{\"version\": 1, \"number\": true", FOLIO_SETTINGS_MALFORMED,
                   "an unterminated document is refused");
    expect_refusal("{\"version\": 1, \"number\": true} trailing", FOLIO_SETTINGS_MALFORMED,
                   "trailing text is refused");
    expect_refusal("[1]", FOLIO_SETTINGS_MALFORMED, "an array is not the settings document");
}

void run_settings_tests(void)
{
    verify_default();
    verify_parse();
    verify_round_trip(true);
    verify_round_trip(false);
    verify_refusals();
}
