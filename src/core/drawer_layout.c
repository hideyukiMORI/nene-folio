#include "drawer_layout.h"

#include "category_ledger.h"
#include "note_ledger.h"

#include <stdlib.h>
#include <string.h>

struct drawer_layout
{
    struct drawer_row *_Nullable rows;
    size_t count;
    char *_Nullable texts; /* 全行の名前を終端付きで並べた所有領域 */
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
    return offset + length;
}

static struct drawer_row row_at(struct drawer_metrics metrics, size_t index,
                                enum drawer_row_kind kind, const char *_Nonnull text)
{
    struct drawer_row row = {
        .kind = kind,
        .top = metrics.top_padding + (int)index * metrics.row_height,
        .height = metrics.row_height,
        .indent = kind == DRAWER_ROW_CATEGORY ? metrics.category_indent : metrics.note_indent,
        .text = text,
        .color = {0, 0, 0},
    };
    return row;
}

static void fill(struct drawer_layout *_Nonnull layout,
                 const struct category_ledger *_Nonnull categories,
                 const struct note_ledger *_Nonnull const *_Nonnull notes,
                 struct drawer_metrics metrics)
{
    size_t offset = 0;
    size_t categories_count = category_ledger_count(categories);
    for (size_t category = 0; category < categories_count; ++category)
    {
        struct rgb_color color = category_ledger_color(categories, category);
        struct drawer_row row = row_at(metrics, layout->count, DRAWER_ROW_CATEGORY,
                                       category_ledger_name(categories, category));
        row.color = color;
        offset = place(layout, offset, row);
        if (!category_ledger_expanded(categories, category))
        {
            continue;
        }
        size_t notes_count = note_ledger_count(notes[category]);
        for (size_t note = 0; note < notes_count; ++note)
        {
            row = row_at(metrics, layout->count, DRAWER_ROW_NOTE,
                         note_ledger_name(notes[category], note));
            row.color = color;
            offset = place(layout, offset, row);
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
    if (layout->rows == nullptr || layout->texts == nullptr)
    {
        drawer_layout_destroy(layout);
        return DRAWER_LAYOUT_OUT_OF_MEMORY;
    }
    fill(layout, categories, notes, metrics);
    *out = layout;
    return DRAWER_LAYOUT_CREATED;
}

size_t drawer_layout_row_count(const struct drawer_layout *_Nonnull layout)
{
    return layout->count;
}

struct drawer_row drawer_layout_row(const struct drawer_layout *_Nonnull layout, size_t index)
{
    return layout->rows[index];
}

void drawer_layout_destroy(struct drawer_layout *_Nullable layout)
{
    if (layout == nullptr)
    {
        return;
    }
    free(layout->rows);
    free(layout->texts);
    free(layout);
}
