#include "category_ledger.h"
#include "drawer_layout.h"
#include "note_ledger.h"
#include "unit_tests.h"

#include <string.h>

static const struct drawer_metrics metrics = {.top_padding = 8,
                                              .row_height = 28,
                                              .category_height = 34,
                                              .category_gap = 6,
                                              .category_indent = 12,
                                              .note_indent = 32};

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

/* 行の上端は含み、下端は含まない。行の外では index を触らない。 */
static void verify_hits(const struct drawer_layout *_Nonnull layout)
{
    size_t index = 99;
    require(!drawer_layout_hit(layout, 13, &index) && index == 99, "above first row");
    require(drawer_layout_hit(layout, 14, &index) && index == 0, "top edge of first row");
    require(drawer_layout_hit(layout, 47, &index) && index == 0, "bottom edge of first row");
    require(drawer_layout_hit(layout, 48, &index) && index == 1, "first note row");
    require(drawer_layout_hit(layout, 143, &index) && index == 3, "last row");
    require(!drawer_layout_hit(layout, 144, &index), "below last row");
}

static void verify_rows(void)
{
    struct drawer_layout *layout = build_layout();
    require(drawer_layout_row_count(layout) == 4, "collapsed notes make no rows");
    struct drawer_row row = drawer_layout_row(layout, 0);
    require(row.kind == DRAWER_ROW_CATEGORY && row.top == 14 && row.height == 34 &&
                row.indent == 12 && same_text(row.text, "work") && row.color.blue == 0xFF,
            "category row");
    require(row.ordinal == 1 && row.expanded && !row.selected, "category row marks");
    row = drawer_layout_row(layout, 1);
    require(row.kind == DRAWER_ROW_NOTE && row.top == 48 && row.height == 28 && row.indent == 32 &&
                same_text(row.text, "alpha") && row.color.blue == 0xFF,
            "first note row");
    row = drawer_layout_row(layout, 2);
    require(row.kind == DRAWER_ROW_NOTE && row.top == 76 && same_text(row.text, "beta"),
            "second note row");
    row = drawer_layout_row(layout, 3);
    require(row.kind == DRAWER_ROW_CATEGORY && row.top == 110 && same_text(row.text, "closed") &&
                row.color.green == 0xFF && row.ordinal == 2 && !row.expanded,
            "collapsed category row");
    drawer_layout_select(layout, 0, 1);
    require(!drawer_layout_row(layout, 1).selected && drawer_layout_row(layout, 2).selected,
            "selection marks one note row");
    drawer_layout_select(layout, 1, 0);
    require(!drawer_layout_row(layout, 2).selected && !drawer_layout_row(layout, 3).selected,
            "selection of a hidden note marks nothing");
    require(drawer_layout_row(layout, 0).category == 0 &&
                drawer_layout_row(layout, 2).category == 0 && row.category == 1,
            "rows know their category");
    require(drawer_layout_row(layout, 1).note == 0 && drawer_layout_row(layout, 2).note == 1,
            "note rows know their note");
    verify_hits(layout);
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
    size_t index = 0;
    require(!drawer_layout_hit(layout, 0, &index), "nothing to hit");
    drawer_layout_destroy(layout);
    category_ledger_destroy(categories);
}

/* build_layout の配置は 塊 0 = work(14..48) + alpha(48..76) + beta(76..104)、
 * 塊 1 = closed(110..144)。塊の中点は 59 と 127、ノート行の中点は 62 と 90。
 * カテゴリ行を掴むと候補は塊の境界で、掴んだ塊の前後では番号が変わらない。 */
static void verify_category_drops(const struct drawer_layout *_Nonnull layout)
{
    struct drop_target target = drawer_layout_drop(layout, 0, 0);
    require(target.kind == DROP_CATEGORY && target.category == 0 && target.index == 0 &&
                target.line_y == 11,
            "above everything keeps the first category and draws over its gap");
    target = drawer_layout_drop(layout, 0, 59);
    require(target.index == 0 && target.line_y == 11, "the midpoint itself is still above");
    target = drawer_layout_drop(layout, 0, 60);
    require(target.index == 0 && target.line_y == 107,
            "just past its own midpoint the number does not change");
    target = drawer_layout_drop(layout, 0, 127);
    require(target.index == 0 && target.line_y == 107, "the second midpoint is still above");
    target = drawer_layout_drop(layout, 0, 128);
    require(target.index == 1 && target.line_y == 144,
            "past the second midpoint the category moves to the end");
    target = drawer_layout_drop(layout, 3, 10);
    require(target.kind == DROP_CATEGORY && target.category == 1 && target.index == 0 &&
                target.line_y == 11,
            "the collapsed category moves to the front");
    target = drawer_layout_drop(layout, 3, 60);
    require(target.index == 1 && target.line_y == 107, "just after the first chunk it stays");
    target = drawer_layout_drop(layout, 3, 500);
    require(target.index == 1 && target.line_y == 144, "below everything it stays last");
}

/* ノート行を掴む。候補は同じカテゴリのノート行だけで、外は端に寄る。 */
static void verify_note_drops(const struct drawer_layout *_Nonnull layout)
{
    struct drop_target target = drawer_layout_drop(layout, 1, 0);
    require(target.kind == DROP_NOTE && target.category == 0 && target.index == 0 &&
                target.line_y == 48,
            "the first note stays first above everything");
    target = drawer_layout_drop(layout, 1, 62);
    require(target.index == 0 && target.line_y == 48, "the midpoint itself is still above");
    target = drawer_layout_drop(layout, 1, 63);
    require(target.index == 0 && target.line_y == 76,
            "just past its own midpoint the number does not change");
    target = drawer_layout_drop(layout, 1, 91);
    require(target.index == 1 && target.line_y == 104, "past the second midpoint it moves down");
    target = drawer_layout_drop(layout, 1, 500);
    require(target.index == 1 && target.line_y == 104,
            "over another category it goes to its own end");
    target = drawer_layout_drop(layout, 2, 0);
    require(target.category == 0 && target.index == 0 && target.line_y == 48,
            "the second note moves to the front");
    target = drawer_layout_drop(layout, 2, 63);
    require(target.index == 1 && target.line_y == 76, "before its own row it stays");
    target = drawer_layout_drop(layout, 2, 91);
    require(target.index == 1 && target.line_y == 104, "past its own midpoint it stays");
}

static void verify_drops(void)
{
    struct drawer_layout *layout = build_layout();
    verify_category_drops(layout);
    verify_note_drops(layout);
    drawer_layout_destroy(layout);
}

void run_layout_tests(void)
{
    verify_rows();
    verify_empty();
    verify_drops();
}
