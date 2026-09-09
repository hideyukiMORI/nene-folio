#include "drawer_layout.h"

#include "category_ledger.h"
#include "note_ledger.h"

#include <stdlib.h>
#include <string.h>

struct drawer_layout
{
    struct drawer_row *_Nullable rows;
    size_t count;
    char *_Nullable texts;         /* 全行の名前を終端付きで並べた所有領域 */
    size_t *_Nullable notes;       /* カテゴリごとの台帳のノート数（折り畳んでいても持つ） */
    int bottom;                    /* 最後に置いた行の下端 */
    struct drawer_metrics metrics; /* 挿入線の位置に要る（drawer_layout_drop） */
};

/* 行数と名前の総バイト数（終端込み）を数える。 */
static void measure(const struct category_ledger *_Nonnull categories,
                    const struct note_ledger *_Nonnull const *_Nonnull notes, size_t *_Nonnull rows,
                    size_t *_Nonnull bytes)
{
    size_t categories_count = category_ledger_count(categories);
    for (size_t category = 0; category < categories_count; ++category)
    {
        *rows += 1;
        *bytes += strlen(category_ledger_name(categories, category)) + 1;
        if (!category_ledger_expanded(categories, category))
        {
            continue;
        }
        size_t notes_count = note_ledger_count(notes[category]);
        for (size_t note = 0; note < notes_count; ++note)
        {
            *rows += 1;
            *bytes += strlen(note_ledger_name(notes[category], note)) + 1;
        }
    }
}

/* 行を 1 つ書き、名前を texts へ写す。次の書き込み位置を返す。 */
static size_t place(struct drawer_layout *_Nonnull layout, size_t offset, struct drawer_row row)
{
    size_t length = strlen(row.text) + 1;
    memcpy(layout->texts + offset, row.text, length);
    row.text = layout->texts + offset;
    layout->rows[layout->count] = row;
    layout->count += 1;
    layout->bottom = row.top + row.height;
    return offset + length;
}

/* カテゴリ行を、直前の行の下端から間を空けて置く。 */
static struct drawer_row category_row(const struct drawer_layout *_Nonnull layout,
                                      struct drawer_metrics metrics,
                                      const struct category_ledger *_Nonnull categories,
                                      size_t category)
{
    struct drawer_row row = {
        .kind = DRAWER_ROW_CATEGORY,
        .top = layout->bottom + metrics.category_gap,
        .height = metrics.category_height,
        .indent = metrics.category_indent,
        .text = category_ledger_name(categories, category),
        .color = category_ledger_color(categories, category),
        .category = category,
        .note = 0,
        .ordinal = category + 1,
        .expanded = category_ledger_expanded(categories, category),
        .selected = false,
    };
    return row;
}

/* ノート行を、直前の行の直下に置く。カテゴリ行の色と番号を引き継ぐ。 */
static struct drawer_row note_row(const struct drawer_layout *_Nonnull layout,
                                  struct drawer_metrics metrics, struct drawer_row category,
                                  size_t note)
{
    struct drawer_row row = category;
    row.kind = DRAWER_ROW_NOTE;
    row.top = layout->bottom;
    row.height = metrics.row_height;
    row.indent = metrics.note_indent;
    row.note = note;
    row.expanded = true;
    return row;
}

static void fill(struct drawer_layout *_Nonnull layout,
                 const struct category_ledger *_Nonnull categories,
                 const struct note_ledger *_Nonnull const *_Nonnull notes,
                 struct drawer_metrics metrics)
{
    size_t offset = 0;
    layout->bottom = metrics.top_padding;
    size_t categories_count = category_ledger_count(categories);
    for (size_t category = 0; category < categories_count; ++category)
    {
        struct drawer_row row = category_row(layout, metrics, categories, category);
        layout->notes[category] = note_ledger_count(notes[category]);
        offset = place(layout, offset, row);
        if (!row.expanded)
        {
            continue;
        }
        size_t notes_count = note_ledger_count(notes[category]);
        for (size_t note = 0; note < notes_count; ++note)
        {
            struct drawer_row line = note_row(layout, metrics, row, note);
            line.text = note_ledger_name(notes[category], note);
            offset = place(layout, offset, line);
        }
    }
}

enum drawer_layout_outcome
drawer_layout_create(const struct category_ledger *_Nonnull categories,
                     const struct note_ledger *_Nonnull const *_Nonnull notes,
                     struct drawer_metrics metrics, struct drawer_layout *_Nullable *_Nonnull out)
{
    size_t rows = 0;
    size_t bytes = 0;
    measure(categories, notes, &rows, &bytes);
    struct drawer_layout *_Nullable layout = calloc(1, sizeof *layout);
    if (layout == nullptr)
    {
        return DRAWER_LAYOUT_OUT_OF_MEMORY;
    }
    /* 行が 0 でも 1 要素ぶん確保し、確保の失敗と空の区別を残す。 */
    layout->rows = malloc((rows + 1) * sizeof *layout->rows);
    layout->texts = malloc(bytes + 1);
    layout->notes = malloc((category_ledger_count(categories) + 1) * sizeof *layout->notes);
    if (layout->rows == nullptr || layout->texts == nullptr || layout->notes == nullptr)
    {
        drawer_layout_destroy(layout);
        return DRAWER_LAYOUT_OUT_OF_MEMORY;
    }
    layout->metrics = metrics;
    fill(layout, categories, notes, metrics);
    *out = layout;
    return DRAWER_LAYOUT_CREATED;
}

void drawer_layout_select(struct drawer_layout *_Nonnull layout, size_t category, size_t note)
{
    for (size_t index = 0; index < layout->count; ++index)
    {
        struct drawer_row *_Nonnull row = &layout->rows[index];
        row->selected =
            row->kind == DRAWER_ROW_NOTE && row->category == category && row->note == note;
    }
}

size_t drawer_layout_row_count(const struct drawer_layout *_Nonnull layout)
{
    return layout->count;
}

struct drawer_row drawer_layout_row(const struct drawer_layout *_Nonnull layout, size_t index)
{
    return layout->rows[index];
}

bool drawer_layout_hit(const struct drawer_layout *_Nonnull layout, int y, size_t *_Nonnull index)
{
    for (size_t row = 0; row < layout->count; ++row)
    {
        const struct drawer_row *_Nonnull candidate = &layout->rows[row];
        if (y >= candidate->top && y < candidate->top + candidate->height)
        {
            *index = row;
            return true;
        }
    }
    return false;
}

/* start のカテゴリ行に続くノート行を飛ばし、次の塊の先頭の番号を返す。 */
static size_t chunk_end(const struct drawer_layout *_Nonnull layout, size_t start)
{
    size_t end = start + 1;
    while (end < layout->count && layout->rows[end].kind == DRAWER_ROW_NOTE)
    {
        end += 1;
    }
    return end;
}

/* 塊（カテゴリ行と展開中のノート行）の上端と下端の中点。 */
static int chunk_middle(const struct drawer_layout *_Nonnull layout, size_t start, size_t end)
{
    const struct drawer_row *_Nonnull last = &layout->rows[end - 1];
    return (layout->rows[start].top + last->top + last->height) / 2;
}

/* 掴んだ行と挿入位置から移動後の番号を出す。掴んだ行より後ろへ入れるときは 1 つ詰まる。 */
static size_t moved_to(size_t position, size_t source)
{
    return position > source ? position - 1 : position;
}

/* カテゴリの塊の中点で数え、線は次の塊のカテゴリ行の間の中ほどに引く。 */
static struct drop_target category_drop(const struct drawer_layout *_Nonnull layout,
                                        struct drawer_row source, int y)
{
    struct drop_target target = {
        .kind = DROP_CATEGORY, .category = source.category, .index = 0, .line_y = layout->bottom};
    size_t position = 0;
    bool settled = false;
    size_t start = 0;
    while (start < layout->count)
    {
        size_t end = chunk_end(layout, start);
        if (chunk_middle(layout, start, end) < y)
        {
            position += 1;
        }
        else if (!settled)
        {
            settled = true;
            target.line_y = layout->rows[start].top - layout->metrics.category_gap / 2;
        }
        start = end;
    }
    target.index = moved_to(position, source.category);
    return target;
}

/* y のある塊の先頭の行番号。境界は隣り合う塊の間の中ほどで、上端より上は最初の塊、
 * 下端より下は最後の塊になる（ADR 0008 の決定 1）。 */
static size_t chunk_at(const struct drawer_layout *_Nonnull layout, int y)
{
    size_t found = 0;
    size_t start = 0;
    while (start < layout->count)
    {
        if (layout->rows[start].top - layout->metrics.category_gap / 2 <= y)
        {
            found = start;
        }
        start = chunk_end(layout, start);
    }
    return found;
}

/* 移動先の塊のノート行の中点で数える。自分のカテゴリなら掴んだ行のぶんだけ詰まる（ADR 0007）。 */
static struct drop_target note_drop(const struct drawer_layout *_Nonnull layout,
                                    struct drawer_row source, int y)
{
    size_t start = chunk_at(layout, y);
    size_t end = chunk_end(layout, start);
    struct drawer_row head = layout->rows[start];
    struct drop_target target = {
        .kind = DROP_NOTE, .category = head.category, .index = 0, .line_y = head.top + head.height};
    if (end == start + 1)
    {
        /* 折り畳んだカテゴリは台帳のノート数（末尾）。展開していてノートが無ければ同じ値の 0。 */
        target.index = layout->notes[head.category];
        return target;
    }
    size_t position = 0;
    bool settled = false;
    for (size_t index = start + 1; index < end; ++index)
    {
        const struct drawer_row *_Nonnull row = &layout->rows[index];
        if (row->top + row->height / 2 < y)
        {
            position += 1;
        }
        else if (!settled)
        {
            settled = true;
            target.line_y = row->top;
        }
    }
    if (!settled)
    {
        target.line_y = layout->rows[end - 1].top + layout->rows[end - 1].height;
    }
    target.index = head.category == source.category ? moved_to(position, source.note) : position;
    return target;
}

struct drop_target drawer_layout_drop(const struct drawer_layout *_Nonnull layout,
                                      size_t source_row, int y)
{
    struct drawer_row source = layout->rows[source_row];
    switch (source.kind)
    {
    case DRAWER_ROW_CATEGORY:
        return category_drop(layout, source, y);
    case DRAWER_ROW_NOTE:
        return note_drop(layout, source, y);
    }
    return category_drop(layout, source, y);
}

void drawer_layout_destroy(struct drawer_layout *_Nullable layout)
{
    if (layout == nullptr)
    {
        return;
    }
    free(layout->rows);
    free(layout->texts);
    free(layout->notes);
    free(layout);
}
