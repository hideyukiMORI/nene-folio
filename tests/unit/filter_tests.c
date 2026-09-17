/* 全ノートの絞り込み（FR-032 / #40 / ADR 0024）。core の判定・写しの置き場・配置だけを測る。 */
#include "category_ledger.h"
#include "drawer_layout.h"
#include "index_filter.h"
#include "note_corpus.h"
#include "note_ledger.h"
#include "note_text.h"
#include "unit_tests.h"

#include <string.h>

static const struct drawer_metrics metrics = {.top_padding = 8,
                                              .row_height = 28,
                                              .category_height = 34,
                                              .category_gap = 6,
                                              .category_indent = 12,
                                              .note_indent = 32};

static struct note_ref at(size_t category, size_t note)
{
    struct note_ref ref = {.category = category, .note = note};
    return ref;
}

static struct index_filter_entry entry_of(struct note_ref ref, const char *_Nonnull name,
                                          const char *_Nullable body)
{
    struct index_filter_entry entry = {
        .ref = ref, .name = name, .body = body, .length = body == nullptr ? 0 : strlen(body)};
    return entry;
}

/* 語と列から集合を作る。作れなければ nullptr を返す（理由は呼び出し側が別に測る）。 */
static struct index_filter *_Nullable filter_of(const char *_Nonnull term,
                                                const struct index_filter_entry *_Nonnull entries,
                                                size_t count)
{
    struct index_filter_query query = {
        .term = term, .term_length = strlen(term), .entries = entries, .count = count};
    struct index_filter *filter = nullptr;
    if (index_filter_create(&query, &filter) != INDEX_FILTER_ACCEPTED)
    {
        return nullptr;
    }
    return filter;
}

/* ASCII の英字だけ大小を無視し、多バイト文字は区別する（決定 2）。 */
static void verify_folding(void)
{
    const struct index_filter_entry entries[] = {
        entry_of(at(0, 0), "Alpha", "MIXED Case Body"),
        entry_of(at(0, 1), "beta", "ひらがな カタカナ"),
        entry_of(at(0, 2), "gamma", "ｱｲｳ"),
    };
    struct index_filter *_Nullable filter = filter_of("mixed", entries, 3);
    require(filter != nullptr, "the filter is built");
    require(index_filter_count(filter) == 1, "one note matches regardless of ASCII case");
    require(index_filter_note(filter, at(0, 0)), "and it is the one whose body holds the term");
    index_filter_destroy(filter);

    filter = filter_of("ALPHA", entries, 3);
    require(filter != nullptr && index_filter_note(filter, at(0, 0)),
            "the name matches with the case folded too");
    index_filter_destroy(filter);

    filter = filter_of("カタカナ", entries, 3);
    require(filter != nullptr && index_filter_count(filter) == 1 &&
                index_filter_note(filter, at(0, 1)),
            "a multi-byte term matches only the same bytes");
    index_filter_destroy(filter);

    filter = filter_of("アイウ", entries, 3);
    require(filter != nullptr && index_filter_count(filter) == 0,
            "half width katakana is not the same as full width");
    index_filter_destroy(filter);
}

/* 名前と本文のどちらでも一致し、写しの無いノートは名前が合っても見えない（決定 1 / 2）。 */
static void verify_sources(void)
{
    const struct index_filter_entry entries[] = {
        entry_of(at(0, 0), "report", "nothing here"),
        entry_of(at(1, 0), "notes", "the report is late"),
        entry_of(at(1, 1), "report copy", nullptr),
    };
    struct index_filter *_Nullable filter = filter_of("report", entries, 3);
    require(filter != nullptr, "the filter is built");
    require(index_filter_note(filter, at(0, 0)), "a name match is visible");
    require(index_filter_note(filter, at(1, 0)), "a body match is visible");
    require(!index_filter_note(filter, at(1, 1)), "a note without a copy never matches");
    require(index_filter_count(filter) == 2, "and nothing else is visible");
    require(index_filter_category(filter, 0) && index_filter_category(filter, 1),
            "both categories hold a visible note");
    require(!index_filter_category(filter, 2), "an unknown category holds none");
    index_filter_destroy(filter);
}

/* 空の語は絞り込みなし、壊れた UTF-8 の語は MALFORMED（どちらも集合を作らない・決定 2）。 */
static void verify_rejections(void)
{
    const struct index_filter_entry entries[] = {entry_of(at(0, 0), "alpha", "body")};
    struct index_filter_query empty = {
        .term = "", .term_length = 0, .entries = entries, .count = 1};
    struct index_filter *filter = nullptr;
    require(index_filter_create(&empty, &filter) == INDEX_FILTER_NO_TERM,
            "an empty term does not filter");
    require(filter == nullptr, "and leaves the output alone");
    /* 0xC3 は 2 バイト列の先頭で、続きが無い。 */
    struct index_filter_query broken = {
        .term = "\xC3", .term_length = 1, .entries = entries, .count = 1};
    require(index_filter_create(&broken, &filter) == INDEX_FILTER_MALFORMED,
            "a broken term is refused");
    require(filter == nullptr, "and leaves the output alone");
    struct index_filter_query longer = {
        .term = "bodybody", .term_length = 8, .entries = entries, .count = 1};
    require(index_filter_create(&longer, &filter) == INDEX_FILTER_ACCEPTED,
            "a term longer than every text still builds a set");
    require(index_filter_count(filter) == 0, "with nothing in it");
    index_filter_destroy(filter);
}

/* 語と名前と本文の長さの境目。写しが空でも名前では一致し、名前より長い語は本文で一致する。 */
static void verify_lengths(void)
{
    const struct index_filter_entry entries[] = {
        entry_of(at(0, 0), "report", ""),
        entry_of(at(0, 1), "ab", "0123456789"),
    };
    struct index_filter *_Nullable filter = filter_of("report", entries, 2);
    require(filter != nullptr && index_filter_count(filter) == 1 &&
                index_filter_note(filter, at(0, 0)),
            "a note whose copy is empty still matches by name");
    index_filter_destroy(filter);

    filter = filter_of("01234", entries, 2);
    require(filter != nullptr && index_filter_count(filter) == 1 &&
                index_filter_note(filter, at(0, 1)),
            "a term longer than the name and shorter than the body matches the body");
    index_filter_destroy(filter);

    filter = filter_of("abc", entries, 2);
    require(filter != nullptr && index_filter_count(filter) == 0,
            "a term longer than the name it starts is not a name match");
    index_filter_destroy(filter);
}

/* 一致集合は台帳の順（カテゴリ番号・ノート番号の昇順）で作られ、所属はその順に依る二分探索で
 * 答える（ADR 0024 の補正 6）。生成の順が崩れれば、この問い合わせのどれかが外れる。 */
static void verify_order(void)
{
    /* 4 カテゴリ・11 ノート。一致するのは 0/1・1/0・1/3・3/0・3/2 の 5 本。 */
    const struct index_filter_entry entries[] = {
        entry_of(at(0, 0), "a", "plain"),    entry_of(at(0, 1), "b", "needle"),
        entry_of(at(1, 0), "needle", "one"), entry_of(at(1, 1), "d", "plain"),
        entry_of(at(1, 2), "e", "plain"),    entry_of(at(1, 3), "f", "a needle here"),
        entry_of(at(2, 0), "g", "plain"),    entry_of(at(2, 1), "h", "plain"),
        entry_of(at(3, 0), "i", "needle"),   entry_of(at(3, 1), "j", "plain"),
        entry_of(at(3, 2), "k", "needle"),
    };
    struct index_filter *_Nullable filter = filter_of("needle", entries, 11);
    require(filter != nullptr && index_filter_count(filter) == 5, "five notes match");
    const bool expected[] = {false, true,  true, false, false, true,
                             false, false, true, false, true};
    for (size_t index = 0; index < 11; ++index)
    {
        require(index_filter_note(filter, entries[index].ref) == expected[index],
                "every note in the ledger order answers for itself");
    }
    require(!index_filter_note(filter, at(1, 9)) && !index_filter_note(filter, at(4, 0)) &&
                !index_filter_note(filter, at(3, 1)),
            "a note beyond the end of a category, an unknown category and a gap are all absent");
    require(index_filter_category(filter, 0) && index_filter_category(filter, 1) &&
                !index_filter_category(filter, 2) && index_filter_category(filter, 3) &&
                !index_filter_category(filter, 4),
            "the categories in the middle and at the ends are told apart");
    index_filter_destroy(filter);
}

static struct category_ledger *_Nonnull categories_from(const char *_Nonnull text)
{
    struct category_ledger *ledger = nullptr;
    require(category_ledger_parse(text, strlen(text), &ledger) == CATEGORY_LEDGER_ACCEPTED,
            "filter categories");
    return ledger;
}

static struct note_ledger *_Nonnull notes_from(const char *_Nonnull text)
{
    struct note_ledger *ledger = nullptr;
    require(note_ledger_parse(text, strlen(text), &ledger) == NOTE_LEDGER_ACCEPTED, "filter notes");
    return ledger;
}

/* 絞り込むと、一致を持つカテゴリだけが折り畳みに関わらず展開して並び、番号と色は台帳のまま
 * （決定 3）。 */
static void verify_layout(void)
{
    struct category_ledger *categories =
        categories_from("{\"version\": 1, \"categories\": ["
                        "{\"name\": \"work\", \"color\": \"#3D7EFF\", \"expanded\": true},"
                        "{\"name\": \"closed\", \"color\": \"#00FF00\", \"expanded\": false}]}");
    struct note_ledger *first = notes_from("{\"version\": 1, \"notes\": [\"alpha\", \"beta\"]}");
    struct note_ledger *second = notes_from("{\"version\": 1, \"notes\": [\"hidden\"]}");
    const struct note_ledger *const notes[] = {first, second};
    const struct index_filter_entry entries[] = {
        entry_of(at(0, 0), "alpha", "no match here"),
        entry_of(at(0, 1), "beta", "the needle is here"),
        entry_of(at(1, 0), "hidden", "the needle again"),
    };
    struct index_filter *_Nullable filter = filter_of("needle", entries, 3);
    require(filter != nullptr, "the filter is built");
    struct drawer_source source = {.categories = categories, .notes = notes, .filter = filter};
    struct drawer_layout *layout = nullptr;
    require(drawer_layout_create(&source, metrics, &layout) == DRAWER_LAYOUT_CREATED,
            "filtered layout create");
    require(drawer_layout_row_count(layout) == 4, "two categories and one note each");
    struct drawer_row row = drawer_layout_row(layout, 1);
    require(row.kind == DRAWER_ROW_NOTE && row.note == 1 && same_text(row.text, "beta"),
            "the matching note keeps its ledger number");
    require(drawer_layout_row(layout, 2).expanded,
            "a collapsed category is opened while filtering");
    require(drawer_layout_row(layout, 2).ordinal == 2 &&
                drawer_layout_row(layout, 2).color.green == 0xFF,
            "and keeps its ordinal and colour");
    require(drawer_layout_row(layout, 3).category == 1 && drawer_layout_row(layout, 3).note == 0,
            "its matching note is numbered by the ledger too");
    drawer_layout_destroy(layout);
    index_filter_destroy(filter);

    /* 一致を持たないカテゴリは行にならない。 */
    filter = filter_of("again", entries, 3);
    require(filter != nullptr, "the second filter is built");
    source.filter = filter;
    require(drawer_layout_create(&source, metrics, &layout) == DRAWER_LAYOUT_CREATED,
            "narrow layout create");
    require(drawer_layout_row_count(layout) == 2, "only the category that matches is placed");
    require(drawer_layout_row(layout, 0).category == 1, "and it is the second one");
    require(drawer_layout_row(layout, 0).top == metrics.top_padding + metrics.category_gap,
            "the first shown row sits under the band");
    drawer_layout_destroy(layout);
    index_filter_destroy(filter);

    /* 一致が 0 件なら索引は空になる。 */
    filter = filter_of("absent", entries, 3);
    require(filter != nullptr, "the empty filter is built");
    source.filter = filter;
    require(drawer_layout_create(&source, metrics, &layout) == DRAWER_LAYOUT_CREATED,
            "empty filtered layout");
    require(drawer_layout_row_count(layout) == 0, "no row is placed");
    drawer_layout_destroy(layout);
    index_filter_destroy(filter);

    note_ledger_destroy(second);
    note_ledger_destroy(first);
    category_ledger_destroy(categories);
}

static struct note_text *_Nonnull text_of(const char *_Nonnull bytes)
{
    struct note_text *text = nullptr;
    require(note_text_create(bytes, strlen(bytes), &text) == NOTE_TEXT_ACCEPTED, "corpus body");
    return text;
}

/* 写しは宛先で引き、改名と移動で付け替わる。並び替えでは何も起きない（決定 1）。 */
static void verify_corpus(void)
{
    struct note_corpus *corpus = nullptr;
    require(note_corpus_create(&corpus) == NOTE_CORPUS_ACCEPTED, "corpus create");
    require(note_corpus_body(corpus, "work", "alpha") == nullptr, "an unread note has no copy");
    struct note_text *first = text_of("first body");
    struct note_text *second = text_of("second body");
    require(note_corpus_put(corpus, "work", "alpha", first) == NOTE_CORPUS_ACCEPTED, "put");
    require(note_corpus_put(corpus, "work", "beta", second) == NOTE_CORPUS_ACCEPTED, "put again");
    const struct note_text *_Nullable held = note_corpus_body(corpus, "work", "alpha");
    require(held != nullptr && same_text(note_text_bytes(held), "first body"),
            "the copy is the body that was stored");
    note_text_destroy(first);
    note_text_destroy(second);
    require(same_text(note_text_bytes(note_corpus_body(corpus, "work", "alpha")), "first body"),
            "and outlives the text it was copied from");

    struct note_text *replaced = text_of("third body");
    require(note_corpus_put(corpus, "work", "alpha", replaced) == NOTE_CORPUS_ACCEPTED, "replace");
    note_text_destroy(replaced);
    require(same_text(note_text_bytes(note_corpus_body(corpus, "work", "alpha")), "third body"),
            "putting the same destination replaces the copy");

    require(note_corpus_rename(corpus, "work", "alpha", "renamed") == NOTE_CORPUS_ACCEPTED,
            "rename");
    require(note_corpus_body(corpus, "work", "alpha") == nullptr, "the old name has no copy");
    require(same_text(note_text_bytes(note_corpus_body(corpus, "work", "renamed")), "third body"),
            "and the new name holds it");
    require(note_corpus_rename(corpus, "work", "absent", "other") == NOTE_CORPUS_ACCEPTED,
            "renaming a note without a copy does nothing");

    require(note_corpus_relocate(corpus, "work", "renamed", "spare") == NOTE_CORPUS_ACCEPTED,
            "relocate");
    require(note_corpus_body(corpus, "work", "renamed") == nullptr, "the old category is empty");
    require(same_text(note_text_bytes(note_corpus_body(corpus, "spare", "renamed")), "third body"),
            "and the new category holds the copy");
    require(note_corpus_relocate(corpus, "work", "absent", "spare") == NOTE_CORPUS_ACCEPTED,
            "relocating a note without a copy does nothing");

    /* 移した先に古い写しが残っていたら捨てる（上の層は同名を断るので普段は起きない）。 */
    struct note_text *collision = text_of("stale body");
    require(note_corpus_put(corpus, "work", "beta", collision) == NOTE_CORPUS_ACCEPTED, "collide");
    note_text_destroy(collision);
    require(note_corpus_rename(corpus, "spare", "renamed", "kept") == NOTE_CORPUS_ACCEPTED,
            "rename to a free name");
    require(note_corpus_relocate(corpus, "spare", "kept", "work") == NOTE_CORPUS_ACCEPTED,
            "move into the category that already holds beta");
    require(note_corpus_rename(corpus, "work", "kept", "beta") == NOTE_CORPUS_ACCEPTED,
            "rename onto an occupied destination");
    require(same_text(note_text_bytes(note_corpus_body(corpus, "work", "beta")), "third body"),
            "the destination keeps the moved copy, not the stale one");
    require(note_corpus_body(corpus, "work", "kept") == nullptr, "and the old name is gone");
    note_corpus_destroy(corpus);
}

void run_filter_tests(void)
{
    verify_folding();
    verify_sources();
    verify_rejections();
    verify_lengths();
    verify_order();
    verify_layout();
    verify_corpus();
}
