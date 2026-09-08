#include "category_ledger.h"
#include "drawer_layout.h"
#include "note_ledger.h"
#include "unit_tests.h"

#include <string.h>

static const struct drawer_metrics metrics = {
    .top_padding = 8, .row_height = 28, .category_indent = 12, .note_indent = 32};

static struct category_ledger *_Nonnull categories_from(const char *_Nonnull text)
{
    struct category_ledger *ledger = nullptr;
    require(category_ledger_parse(text, strlen(text), &ledger) == CATEGORY_LEDGER_ACCEPTED,
            "layout categories");
    return ledger;
}

static struct note_ledger *_Nonnull notes_from(const char *_Nonnull text)
{
    struct note_ledger *ledger = nullptr;
    require(note_ledger_parse(text, strlen(text), &ledger) == NOTE_LEDGER_ACCEPTED, "layout notes");
    return ledger;
}

/* 台帳を先に破棄してから行を読む。名前が layout に複製されていなければ ASan が止める。 */
static struct drawer_layout *_Nonnull build_layout(void)
{
    struct category_ledger *categories =
        categories_from("{\"version\": 1, \"categories\": ["
                        "{\"name\": \"work\", \"color\": \"#3D7EFF\", \"expanded\": true},"
                        "{\"name\": \"closed\", \"color\": \"#00FF00\", \"expanded\": false}]}");
    struct note_ledger *first = notes_from("{\"version\": 1, \"notes\": [\"alpha\", \"beta\"]}");
    struct note_ledger *second = notes_from("{\"version\": 1, \"notes\": [\"hidden\"]}");
    const struct note_ledger *const notes[] = {first, second};
    struct drawer_layout *layout = nullptr;
    require(drawer_layout_create(categories, notes, metrics, &layout) == DRAWER_LAYOUT_CREATED,
            "layout create");
    note_ledger_destroy(second);
    note_ledger_destroy(first);
    category_ledger_destroy(categories);
    return layout;
}

static void verify_rows(void)
{
    struct drawer_layout *layout = build_layout();
    require(drawer_layout_row_count(layout) == 4, "collapsed notes make no rows");
    struct drawer_row row = drawer_layout_row(layout, 0);
    require(row.kind == DRAWER_ROW_CATEGORY && row.top == 8 && row.height == 28 &&
                row.indent == 12 && same_text(row.text, "work") && row.color.blue == 0xFF,
            "category row");
    row = drawer_layout_row(layout, 1);
    require(row.kind == DRAWER_ROW_NOTE && row.top == 36 && row.indent == 32 &&
                same_text(row.text, "alpha") && row.color.blue == 0xFF,
            "first note row");
    row = drawer_layout_row(layout, 2);
    require(row.kind == DRAWER_ROW_NOTE && row.top == 64 && same_text(row.text, "beta"),
            "second note row");
    row = drawer_layout_row(layout, 3);
    require(row.kind == DRAWER_ROW_CATEGORY && row.top == 92 && same_text(row.text, "closed") &&
                row.color.green == 0xFF,
            "collapsed category row");
    drawer_layout_destroy(layout);
    drawer_layout_destroy(nullptr);
}

static void verify_empty(void)
{
    struct category_ledger *categories = categories_from("{\"version\": 1, \"categories\": []}");
    const struct note_ledger *const notes[] = {nullptr};
    struct drawer_layout *layout = nullptr;
    require(drawer_layout_create(categories, notes, metrics, &layout) == DRAWER_LAYOUT_CREATED,
            "empty layout");
    require(drawer_layout_row_count(layout) == 0, "no rows");
    drawer_layout_destroy(layout);
    category_ledger_destroy(categories);
}

void run_layout_tests(void)
{
    verify_rows();
    verify_empty();
}
