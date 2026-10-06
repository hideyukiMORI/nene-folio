#include "category_ledger.h"
#include "category_name.h"
#include "json_writer.h"
#include "unit_tests.h"
#include <string.h>

static void verify_rejected_names(void)
{
    const char *_Nonnull const rejected[] = {"",
                                             ".git",
                                             ".",
                                             "..",
                                             ".hidden",
                                             "Categories.JSON",
                                             "settings.json",
                                             "SETTINGS.Json",
                                             "categories.json",
                                             "a/b",
                                             "a\\b",
                                             "a:b",
                                             "foo ",
                                             "foo.",
                                             "CON",
                                             "nul.txt",
                                             "COM1",
                                             "lpt\xC2\xB9",
                                             "a\nb",
                                             "\xFF"};
    for (size_t index = 0; index < sizeof rejected / sizeof rejected[0]; ++index)
    {
        struct category_name *name = nullptr;
        require(category_name_create(rejected[index], strlen(rejected[index]), &name) ==
                    CATEGORY_NAME_INVALID,
                rejected[index]);
        require(name == nullptr, "invalid category names do not publish an object");
    }
    char long_name[256];
    memset(long_name, 'a', sizeof long_name);
    struct category_name *name = nullptr;
    require(category_name_create(long_name, 256, &name) == CATEGORY_NAME_INVALID,
            "256 bytes is too long");
    require(category_name_create(long_name, 255, &name) == CATEGORY_NAME_ACCEPTED,
            "255 bytes fits");
    category_name_destroy(name);
    category_name_destroy(nullptr);
}

static void verify_accepted_names(void)
{
    /* 末尾の .md は剥がさない。途中の "." と予約名に似た別の名前は通る。 */
    const char *_Nonnull const accepted[] = {
        "日本語",     "仕事 メモ",           "議事.md",  "議事.MD",         "v1.2.3",
        "categories", "categories.json.bak", "settings", "a.settings.json", " 先頭の空白",
        "COM10"};
    for (size_t index = 0; index < sizeof accepted / sizeof accepted[0]; ++index)
    {
        struct category_name *name = nullptr;
        require(category_name_create(accepted[index], strlen(accepted[index]), &name) ==
                    CATEGORY_NAME_ACCEPTED,
                accepted[index]);
        require(same_text(category_name_text(name), accepted[index]), "the text is kept as is");
        category_name_destroy(name);
    }
}

static struct category_name *_Nonnull accepted_name(const char *_Nonnull text)
{
    struct category_name *name = nullptr;
    require(category_name_create(text, strlen(text), &name) == CATEGORY_NAME_ACCEPTED, text);
    return name;
}

static struct category_ledger *_Nonnull three_categories(void)
{
    const char *text = "{\"version\": 1, \"categories\": ["
                       "{\"name\": \"a\", \"color\": \"#010101\", \"expanded\": true},"
                       "{\"name\": \"b\", \"color\": \"#020202\", \"expanded\": false},"
                       "{\"name\": \"c\", \"color\": \"#030303\", \"expanded\": true}]}";
    struct category_ledger *ledger = nullptr;
    require(category_ledger_parse(text, strlen(text), &ledger) == CATEGORY_LEDGER_ACCEPTED,
            "three categories");
    return ledger;
}

/* 新しいカテゴリは既定の色（照合が足すものと同じ #7F8FA6）で展開して始まる。 */
static bool fresh_entry(const struct category_ledger *_Nonnull ledger, size_t index)
{
    struct rgb_color color = category_ledger_color(ledger, index);
    return color.red == 0x7F && color.green == 0x8F && color.blue == 0xA6 &&
           category_ledger_expanded(ledger, index);
}

/* a b c の index に "n" を挿し、名前の頭文字を順に並べた 4 文字を返す。 */
static void inserted_categories(size_t index, char *_Nonnull out)
{
    struct category_ledger *ledger = three_categories();
    struct category_name *name = accepted_name("n");
    struct category_ledger *grown = nullptr;
    require(category_ledger_inserted(ledger, index, name, &grown) == CATEGORY_LEDGER_ACCEPTED,
            "inserted");
    require(category_ledger_count(grown) == 4 && category_ledger_count(ledger) == 3,
            "one more entry and the source is untouched");
    for (size_t position = 0; position < 4; ++position)
    {
        out[position] = category_ledger_name(grown, position)[0];
        bool kept = out[position] == 'n' ||
                    (category_ledger_color(grown, position).red ==
                         (unsigned char)(out[position] - 'a' + 1) &&
                     category_ledger_expanded(grown, position) == (out[position] != 'b'));
        require(kept, "existing entries keep their color and expansion");
    }
    require(fresh_entry(grown, index), "the new entry starts with the default color, expanded");
    category_ledger_destroy(grown);
    category_name_destroy(name);
    category_ledger_destroy(ledger);
}

static void verify_insert_positions(void)
{
    char order[5] = {0};
    inserted_categories(0, order);
    require(same_text(order, "nabc"), "insert at the head");
    inserted_categories(1, order);
    require(same_text(order, "anbc"), "insert in the middle");
    inserted_categories(3, order);
    require(same_text(order, "abcn"), "insert at the tail");
    struct category_ledger *empty = nullptr;
    require(category_ledger_empty(&empty) == CATEGORY_LEDGER_ACCEPTED, "empty ledger");
    struct category_name *name = accepted_name("議事.md");
    struct category_ledger *grown = nullptr;
    require(category_ledger_inserted(empty, 0, name, &grown) == CATEGORY_LEDGER_ACCEPTED &&
                category_ledger_count(grown) == 1 &&
                same_text(category_ledger_name(grown, 0), "議事.md") && fresh_entry(grown, 0),
            "insert into an empty ledger");
    category_ledger_destroy(grown);
    category_name_destroy(name);
    category_ledger_destroy(empty);
}

static void verify_insert_duplicates(void)
{
    struct category_ledger *ledger = three_categories();
    const char *_Nonnull const taken[] = {"a", "c"};
    for (size_t index = 0; index < 2; ++index)
    {
        struct category_name *name = accepted_name(taken[index]);
        struct category_ledger *grown = nullptr;
        /* 既にある名前は挿す位置の前にあっても後ろにあっても拒む。 */
        require(category_ledger_inserted(ledger, 1, name, &grown) == CATEGORY_LEDGER_MALFORMED,
                taken[index]);
        require(grown == nullptr, "a rejected insertion publishes nothing");
        category_name_destroy(name);
    }
    require(category_ledger_count(ledger) == 3 && same_text(category_ledger_name(ledger, 1), "b"),
            "the source is untouched after a rejection");
    category_ledger_destroy(ledger);
}

static void verify_insert_round_trip(void)
{
    struct category_ledger *ledger = three_categories();
    struct category_name *name = accepted_name("新しい カテゴリ");
    struct category_ledger *grown = nullptr;
    require(category_ledger_inserted(ledger, 2, name, &grown) == CATEGORY_LEDGER_ACCEPTED,
            "inserted for writing");
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "writer");
    category_ledger_write(grown, writer);
    require(json_writer_finish(writer) == JSON_WRITER_ACCEPTED, "inserted write finish");
    const char *text = json_writer_text(writer);
    struct category_ledger *again = nullptr;
    require(category_ledger_parse(text, strlen(text), &again) == CATEGORY_LEDGER_ACCEPTED,
            "the written ledger reads back");
    require(category_ledger_count(again) == 4 &&
                same_text(category_ledger_name(again, 2), "新しい カテゴリ") &&
                fresh_entry(again, 2) && !category_ledger_expanded(again, 1),
            "the inserted entry survives the round trip");
    category_ledger_destroy(again);
    json_writer_destroy(writer);
    category_ledger_destroy(grown);
    category_name_destroy(name);
    category_ledger_destroy(ledger);
}

void run_category_name_tests(void)
{
    verify_rejected_names();
    verify_accepted_names();
    verify_insert_positions();
    verify_insert_duplicates();
    verify_insert_round_trip();
}
