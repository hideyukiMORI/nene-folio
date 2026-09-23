/* core の folio_settings（ADR 0025・ADR 0031 の決定 1・2・ADR 0032 の決定 1）。
 * 版 1・版 2 からの移行・版 3 の往復・既定値・拒む形・未知の版と、folio_theme_resolve の
 * 6 通り・folio_language_count の一致・ui_font_face と ui_font_fallback_face の 3 値・
 * ui_font_pick の 3 段を確かめる。 */
#include "folio_language.h"
#include "folio_settings.h"
#include "folio_theme.h"
#include "json_writer.h"
#include "ui_font.h"
#include "unit_tests.h"

#include <string.h>

static const char *const version_one = "{\n  \"version\": 1,\n  \"number\": true\n}";
static const char *const version_two =
    "{\n  \"version\": 2,\n  \"number\": true,\n  \"theme\": \"dark\"\n}";
static const char *const version_three = "{\n  \"version\": 3,\n  \"number\": true,\n  "
                                         "\"theme\": \"dark\",\n  \"language\": \"en\"\n}";

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
    require(folio_settings_language(settings) == FOLIO_LANGUAGE_JA,
            "the default language is Japanese");
    folio_settings_destroy(settings);
}

/* 言語の数の正本は列挙で、folio_language_count はそれと一致する（ADR 0032 の決定 1）。 */
static void verify_language_count(void)
{
    require(folio_language_count == 3, "three languages");
    require(folio_language_count == (size_t)FOLIO_LANGUAGE_ZH_HANS + 1,
            "the count follows the last value of the enumeration");
}

/* face は印字できる ASCII で、RTF の fonttbl にも LOGFONT にもそのまま入る
 * （ADR 0032 の決定 4）。 */
static void expect_face_text(const char *_Nonnull face)
{
    require(face[0] != 0, "every language has a face");
    for (size_t at = 0; face[at] != 0; ++at)
    {
        unsigned char byte = (unsigned char)face[at];
        require(byte >= 0x20 && byte < 0x7F, "the face name is printable ASCII");
        require(byte != '{' && byte != '}' && byte != '\\' && byte != ';',
                "the face name carries no RTF control characters");
    }
    require(strlen(face) < 32, "the face name fits LF_FACESIZE");
}

/* 同梱の face と OS の退避の face の 2 つの表（ADR 0036 の決定 4）。 */
static void verify_font_faces(void)
{
    for (size_t index = 0; index < folio_language_count; ++index)
    {
        expect_face_text(ui_font_face((enum folio_language)index));
        expect_face_text(ui_font_fallback_face((enum folio_language)index));
    }
    require(same_text(ui_font_face(FOLIO_LANGUAGE_JA), "Noto Sans JP"), "Japanese is bundled");
    require(same_text(ui_font_face(FOLIO_LANGUAGE_EN), "Arimo"), "English is bundled");
    require(same_text(ui_font_face(FOLIO_LANGUAGE_ZH_HANS), "Noto Sans SC"),
            "simplified Chinese is bundled");
    require(same_text(ui_font_fallback_face(FOLIO_LANGUAGE_JA),
                      ui_font_fallback_face(FOLIO_LANGUAGE_EN)),
            "Japanese and English share a fallback (Segoe UI carries no CJK)");
    require(same_text(ui_font_fallback_face(FOLIO_LANGUAGE_JA), "Yu Gothic UI"),
            "the Japanese fallback is Yu Gothic UI");
    require(same_text(ui_font_fallback_face(FOLIO_LANGUAGE_ZH_HANS), "Microsoft YaHei UI"),
            "simplified Chinese has its own fallback");
}

/* 同梱 → その言語の退避 → 日本語の退避の 3 段（ADR 0036 の決定 4）。 */
static void verify_font_pick(void)
{
    for (size_t index = 0; index < folio_language_count; ++index)
    {
        enum folio_language language = (enum folio_language)index;
        require(ui_font_pick(language, true, true) == ui_font_face(language),
                "the bundled face comes first");
        require(ui_font_pick(language, true, false) == ui_font_face(language),
                "the bundled face does not need the fallback");
        require(ui_font_pick(language, false, true) == ui_font_fallback_face(language),
                "without the bundle the fallback of the language is next");
        require(ui_font_pick(language, false, false) == ui_font_fallback_face(FOLIO_LANGUAGE_JA),
                "the Japanese fallback is last");
    }
}

/* 書いた文書をもう一度読んで同じ値になるか（キーの順序と整形も版 3 の形のまま）。 */
static void verify_round_trip(bool number, enum folio_theme_choice theme,
                              enum folio_language language)
{
    struct folio_settings *settings = nullptr;
    require(folio_settings_default(&settings) == FOLIO_SETTINGS_READY, "default");
    struct folio_settings *numbered = nullptr;
    require(folio_settings_with_number(settings, number, &numbered) == FOLIO_SETTINGS_READY,
            "a derived copy carries the new value");
    folio_settings_destroy(settings);
    struct folio_settings *themed = nullptr;
    require(folio_settings_with_theme(numbered, theme, &themed) == FOLIO_SETTINGS_READY,
            "a derived copy carries the new theme");
    require(folio_settings_number(numbered) == number && folio_settings_number(themed) == number,
            "with_theme keeps the other value");
    folio_settings_destroy(numbered);
    struct folio_settings *changed = nullptr;
    require(folio_settings_with_language(themed, language, &changed) == FOLIO_SETTINGS_READY,
            "a derived copy carries the new language");
    require(folio_settings_theme(themed) == theme && folio_settings_theme(changed) == theme &&
                folio_settings_language(themed) == FOLIO_LANGUAGE_JA,
            "with_language keeps the other values and leaves the original alone");
    folio_settings_destroy(themed);
    require(folio_settings_number(changed) == number && folio_settings_theme(changed) == theme &&
                folio_settings_language(changed) == language,
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
    require(folio_settings_number(reread) == number && folio_settings_theme(reread) == theme &&
                folio_settings_language(reread) == language,
            "the values survive the round trip");
    folio_settings_destroy(reread);
    json_writer_destroy(writer);
}

/* 旧版を読んで書き戻すと版 3 になり、足りないキーには既定値が入る（移行・決定 1）。 */
static void expect_migration(const char *_Nonnull text, const char *_Nonnull description)
{
    struct folio_settings *settings = nullptr;
    require(folio_settings_parse(text, strlen(text), &settings) == FOLIO_SETTINGS_READY,
            description);
    require(folio_settings_number(settings) &&
                folio_settings_language(settings) == FOLIO_LANGUAGE_JA,
            "an older version keeps number and gains the default language");
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "writer");
    folio_settings_write(settings, writer);
    folio_settings_destroy(settings);
    require(json_writer_finish(writer) == JSON_WRITER_ACCEPTED, "the document is complete");
    require(strstr(json_writer_text(writer), "\"version\": 3") != nullptr &&
                strstr(json_writer_text(writer), "\"language\": \"ja\"") != nullptr,
            "writing always produces version 3");
    json_writer_destroy(writer);
}

static void verify_migration(void)
{
    struct folio_settings *settings = nullptr;
    require(folio_settings_parse(version_one, strlen(version_one), &settings) ==
                FOLIO_SETTINGS_READY,
            "version 1 is still accepted");
    require(folio_settings_theme(settings) == FOLIO_THEME_CHOICE_SYSTEM,
            "version 1 gains the default theme");
    folio_settings_destroy(settings);
    expect_migration(version_one, "version 1 still parses");
    expect_migration(version_two, "version 2 still parses");
}

static void expect_language(const char *_Nonnull text, enum folio_language language,
                            const char *_Nonnull description)
{
    struct folio_settings *settings = nullptr;
    require(folio_settings_parse(text, strlen(text), &settings) == FOLIO_SETTINGS_READY &&
                folio_settings_language(settings) == language,
            description);
    folio_settings_destroy(settings);
}

static void verify_parse(void)
{
    struct folio_settings *settings = nullptr;
    require(folio_settings_parse(version_three, strlen(version_three), &settings) ==
                FOLIO_SETTINGS_READY,
            "version 3 is accepted");
    require(folio_settings_number(settings) &&
                folio_settings_theme(settings) == FOLIO_THEME_CHOICE_DARK &&
                folio_settings_language(settings) == FOLIO_LANGUAGE_EN,
            "the stored values are read back");
    folio_settings_destroy(settings);
    require(folio_settings_parse(version_two, strlen(version_two), &settings) ==
                FOLIO_SETTINGS_READY,
            "version 2 is accepted");
    require(folio_settings_theme(settings) == FOLIO_THEME_CHOICE_DARK,
            "the version 2 theme is read back");
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
    expect_language("{\"version\": 3, \"number\": true, \"theme\": \"dark\", "
                    "\"language\": \"ja\"}",
                    FOLIO_LANGUAGE_JA, "ja is one of the three words");
    expect_language("{\"version\": 3, \"number\": true, \"theme\": \"dark\", "
                    "\"language\": \"en\"}",
                    FOLIO_LANGUAGE_EN, "en is one of the three words");
    expect_language("{\"version\": 3, \"number\": false, \"theme\": \"light\", "
                    "\"language\": \"zh-Hans\"}",
                    FOLIO_LANGUAGE_ZH_HANS, "zh-Hans is one of the three words");
}

/* 黙って既定値へ落とさない形（決定 1）。無い・壊れているは呼び出し側が区別する。 */
static void verify_refusals(void)
{
    expect_refusal("{\"version\": 4, \"number\": true, \"theme\": \"dark\", "
                   "\"language\": \"ja\"}",
                   FOLIO_SETTINGS_UNSUPPORTED_VERSION,
                   "a newer version is refused, not downgraded");
    expect_refusal("{\"version\": 3, \"number\": true, \"theme\": \"dark\"}",
                   FOLIO_SETTINGS_MALFORMED, "version 3 requires the language key");
    expect_refusal("{\"version\": 3, \"number\": true, \"theme\": \"dark\", "
                   "\"language\": \"fr\"}",
                   FOLIO_SETTINGS_MALFORMED, "an unknown language word is refused");
    expect_refusal("{\"version\": 3, \"number\": true, \"theme\": \"dark\", "
                   "\"language\": \"zh-hans\"}",
                   FOLIO_SETTINGS_MALFORMED, "the spelling of zh-Hans is part of the shape");
    expect_refusal("{\"version\": 3, \"number\": true, \"theme\": \"dark\", "
                   "\"language\": true}",
                   FOLIO_SETTINGS_MALFORMED, "a boolean where a language word belongs is refused");
    expect_refusal("{\"version\": 3, \"number\": true, \"language\": \"ja\", "
                   "\"theme\": \"dark\"}",
                   FOLIO_SETTINGS_MALFORMED, "the key order is part of the version 3 shape");
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
    verify_language_count();
    verify_font_faces();
    verify_font_pick();
    verify_parse();
    verify_migration();
    verify_round_trip(true, FOLIO_THEME_CHOICE_DARK, FOLIO_LANGUAGE_ZH_HANS);
    verify_round_trip(false, FOLIO_THEME_CHOICE_LIGHT, FOLIO_LANGUAGE_EN);
    verify_round_trip(true, FOLIO_THEME_CHOICE_SYSTEM, FOLIO_LANGUAGE_JA);
    verify_refusals();
    verify_resolve();
}
