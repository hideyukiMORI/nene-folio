/* core の folio_settings（ADR 0025・ADR 0031 の決定 1・2）。版 1 からの移行・版 2 の往復・
 * 既定値・拒む形・未知の版と、folio_theme_resolve の 6 通りを確かめる。 */
#include "folio_settings.h"
#include "folio_theme.h"
#include "json_writer.h"
#include "unit_tests.h"

#include <string.h>

static const char *const version_one = "{\n  \"version\": 1,\n  \"number\": true\n}";
static const char *const version_two =
    "{\n  \"version\": 2,\n  \"number\": true,\n  \"theme\": \"dark\"\n}";

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
    require(folio_settings_theme(settings) == FOLIO_THEME_CHOICE_SYSTEM,
            "the default follows the operating system");
    folio_settings_destroy(settings);
}

/* 書いた文書をもう一度読んで同じ値になるか（キーの順序と整形も版 2 の形のまま）。 */
static void verify_round_trip(bool number, enum folio_theme_choice theme)
{
    struct folio_settings *settings = nullptr;
    require(folio_settings_default(&settings) == FOLIO_SETTINGS_READY, "default");
    struct folio_settings *numbered = nullptr;
    require(folio_settings_with_number(settings, number, &numbered) == FOLIO_SETTINGS_READY,
            "a derived copy carries the new value");
    folio_settings_destroy(settings);
    struct folio_settings *changed = nullptr;
    require(folio_settings_with_theme(numbered, theme, &changed) == FOLIO_SETTINGS_READY,
            "a derived copy carries the new theme");
    require(folio_settings_number(numbered) == number && folio_settings_number(changed) == number,
            "with_theme keeps the other value");
    folio_settings_destroy(numbered);
    require(folio_settings_number(changed) == number && folio_settings_theme(changed) == theme,
            "the copy holds the asked values");
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "writer");
    folio_settings_write(changed, writer);
    folio_settings_destroy(changed);
    require(json_writer_finish(writer) == JSON_WRITER_ACCEPTED, "the document is complete");
    struct folio_settings *reread = nullptr;
    require(folio_settings_parse(json_writer_text(writer), json_writer_length(writer), &reread) ==
                FOLIO_SETTINGS_READY,
            "what was written parses back");
    require(folio_settings_number(reread) == number && folio_settings_theme(reread) == theme,
            "the values survive the round trip");
    folio_settings_destroy(reread);
    json_writer_destroy(writer);
}

/* 版 1 は読めて theme に既定値が入る。書き戻すと版 2 になる（移行・決定 1）。 */
static void verify_migration(void)
{
    struct folio_settings *settings = nullptr;
    require(folio_settings_parse(version_one, strlen(version_one), &settings) ==
                FOLIO_SETTINGS_READY,
            "version 1 is still accepted");
    require(folio_settings_number(settings) &&
                folio_settings_theme(settings) == FOLIO_THEME_CHOICE_SYSTEM,
            "version 1 keeps number and gains the default theme");
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "writer");
    folio_settings_write(settings, writer);
    folio_settings_destroy(settings);
    require(json_writer_finish(writer) == JSON_WRITER_ACCEPTED, "the document is complete");
    require(strstr(json_writer_text(writer), "\"version\": 2") != nullptr &&
                strstr(json_writer_text(writer), "\"theme\": \"system\"") != nullptr,
            "writing always produces version 2");
    json_writer_destroy(writer);
}

static void verify_parse(void)
{
    struct folio_settings *settings = nullptr;
    require(folio_settings_parse(version_two, strlen(version_two), &settings) ==
                FOLIO_SETTINGS_READY,
            "version 2 is accepted");
    require(folio_settings_number(settings) &&
                folio_settings_theme(settings) == FOLIO_THEME_CHOICE_DARK,
            "the stored values are read back");
    folio_settings_destroy(settings);
    const char *light = "{\"version\": 2, \"number\": false, \"theme\": \"light\"}";
    require(folio_settings_parse(light, strlen(light), &settings) == FOLIO_SETTINGS_READY &&
                folio_settings_theme(settings) == FOLIO_THEME_CHOICE_LIGHT,
            "light is one of the three words");
    folio_settings_destroy(settings);
    const char *system = "{\"version\": 2, \"number\": true, \"theme\": \"system\"}";
    require(folio_settings_parse(system, strlen(system), &settings) == FOLIO_SETTINGS_READY &&
                folio_settings_theme(settings) == FOLIO_THEME_CHOICE_SYSTEM,
            "system is one of the three words");
    folio_settings_destroy(settings);
}

/* 黙って既定値へ落とさない形（決定 1）。無い・壊れているは呼び出し側が区別する。 */
static void verify_refusals(void)
{
    expect_refusal("{\"version\": 3, \"number\": true, \"theme\": \"dark\"}",
                   FOLIO_SETTINGS_UNSUPPORTED_VERSION,
                   "a newer version is refused, not downgraded");
    expect_refusal("{\"version\": 0, \"number\": true}", FOLIO_SETTINGS_UNSUPPORTED_VERSION,
                   "version 0 is not a known version");
    expect_refusal("{\"version\": 2, \"number\": true}", FOLIO_SETTINGS_MALFORMED,
                   "a missing key is refused");
    expect_refusal("{\"version\": 1, \"number\": true, \"theme\": \"dark\"}",
                   FOLIO_SETTINGS_MALFORMED, "version 1 does not carry a theme (decision 1)");
    expect_refusal("{\"number\": true}", FOLIO_SETTINGS_MALFORMED, "a missing version is refused");
    expect_refusal("{\"version\": 2, \"number\": true, \"theme\": \"blue\"}",
                   FOLIO_SETTINGS_MALFORMED, "an unknown theme word is refused");
    expect_refusal("{\"version\": 2, \"number\": true, \"theme\": true}", FOLIO_SETTINGS_MALFORMED,
                   "a boolean where a theme word belongs is refused");
    expect_refusal("{\"version\": 2, \"number\": true, \"theme\": \"dark\", \"language\": \"ja\"}",
                   FOLIO_SETTINGS_MALFORMED, "an unknown key is refused (decision 1)");
    expect_refusal("{\"version\": 2, \"number\": true, \"theme\": \"dark\", \"theme\": \"light\"}",
                   FOLIO_SETTINGS_MALFORMED, "a duplicated key is refused");
    expect_refusal("{\"version\": 2, \"number\": 1, \"theme\": \"dark\"}", FOLIO_SETTINGS_MALFORMED,
                   "a number where a boolean belongs is refused");
    expect_refusal("{\"version\": 2, \"number\": null, \"theme\": \"dark\"}",
                   FOLIO_SETTINGS_MALFORMED,
                   "null is not a boolean, and it does not fall back to the default");
    expect_refusal("{\"version\": 2, \"number\": {\"value\": true}, \"theme\": \"dark\"}",
                   FOLIO_SETTINGS_MALFORMED, "a nested object where a boolean belongs is refused");
    expect_refusal("{\"version\": \"2\", \"number\": true, \"theme\": \"dark\"}",
                   FOLIO_SETTINGS_MALFORMED, "a string version is refused");
    expect_refusal("{\"version\": 2, \"theme\": \"dark\", \"number\": true}",
                   FOLIO_SETTINGS_MALFORMED, "the key order is part of the shape");
    expect_refusal("", FOLIO_SETTINGS_MALFORMED, "an empty file is refused");
    expect_refusal("{\"version\": 2, \"number\": true, \"theme\": \"dark\"",
                   FOLIO_SETTINGS_MALFORMED, "an unterminated document is refused");
    expect_refusal("{\"version\": 2, \"number\": true, \"theme\": \"dark\"} trailing",
                   FOLIO_SETTINGS_MALFORMED, "trailing text is refused");
    expect_refusal("[1]", FOLIO_SETTINGS_MALFORMED, "an array is not the settings document");
}

/* 3 つの選択 × 2 つの OS の値（ADR 0031 の決定 2）。 */
static void verify_resolve(void)
{
    require(folio_theme_resolve(FOLIO_THEME_CHOICE_SYSTEM, FOLIO_THEME_LIGHT) == FOLIO_THEME_LIGHT,
            "system follows a light operating system");
    require(folio_theme_resolve(FOLIO_THEME_CHOICE_SYSTEM, FOLIO_THEME_DARK) == FOLIO_THEME_DARK,
            "system follows a dark operating system");
    require(folio_theme_resolve(FOLIO_THEME_CHOICE_LIGHT, FOLIO_THEME_LIGHT) == FOLIO_THEME_LIGHT &&
                folio_theme_resolve(FOLIO_THEME_CHOICE_LIGHT, FOLIO_THEME_DARK) ==
                    FOLIO_THEME_LIGHT,
            "light ignores the operating system");
    require(folio_theme_resolve(FOLIO_THEME_CHOICE_DARK, FOLIO_THEME_LIGHT) == FOLIO_THEME_DARK &&
                folio_theme_resolve(FOLIO_THEME_CHOICE_DARK, FOLIO_THEME_DARK) == FOLIO_THEME_DARK,
            "dark ignores the operating system");
}

void run_settings_tests(void)
{
    verify_default();
    verify_parse();
    verify_migration();
    verify_round_trip(true, FOLIO_THEME_CHOICE_DARK);
    verify_round_trip(false, FOLIO_THEME_CHOICE_LIGHT);
    verify_round_trip(true, FOLIO_THEME_CHOICE_SYSTEM);
    verify_refusals();
    verify_resolve();
}
