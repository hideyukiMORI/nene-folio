#include "discovery_text.h"
#include "folio_state.h"
#include "note_excerpt.h"
#include "note_tag.h"
#include "note_tags.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include <string.h>

static enum ui_text rank_label(enum index_match_kind kind)
{
    switch (kind)
    {
    case INDEX_MATCH_ALL:
        return UI_TEXT_DISCOVERY_MATCH_ALL;
    case INDEX_MATCH_NAME_EXACT:
        return UI_TEXT_DISCOVERY_NAME_EXACT;
    case INDEX_MATCH_NAME_PREFIX:
        return UI_TEXT_DISCOVERY_NAME_PREFIX;
    case INDEX_MATCH_NAME_CONTAINS:
        return UI_TEXT_DISCOVERY_NAME_CONTAINS;
    case INDEX_MATCH_NAME_SUBSEQUENCE:
        return UI_TEXT_DISCOVERY_NAME_SUBSEQUENCE;
    case INDEX_MATCH_CATEGORY_EXACT:
        return UI_TEXT_DISCOVERY_CATEGORY_EXACT;
    case INDEX_MATCH_CATEGORY_PREFIX:
        return UI_TEXT_DISCOVERY_CATEGORY_PREFIX;
    case INDEX_MATCH_CATEGORY_CONTAINS:
        return UI_TEXT_DISCOVERY_CATEGORY_CONTAINS;
    case INDEX_MATCH_CATEGORY_SUBSEQUENCE:
        return UI_TEXT_DISCOVERY_CATEGORY_SUBSEQUENCE;
    }
    return UI_TEXT_EMPTY;
}

void discovery_text_row(const struct folio_state *_Nonnull state, size_t index, char *_Nonnull out,
                        size_t capacity)
{
    struct index_ranked_note row = folio_state_discovery_row(state, index);
    enum folio_language language = folio_state_language(state);
    const struct ui_text_request request = {.id = UI_TEXT_DISCOVERY_ROW,
                                            .language = language,
                                            .from =
                                                folio_state_category_name(state, row.ref.category),
                                            .name = folio_state_note_name(state, row.ref),
                                            .to = ui_text_line(rank_label(row.kind), language)};
    if (ui_text_format(&request, out, capacity) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

void discovery_text_match(const struct folio_state *_Nonnull state, size_t index,
                          char *_Nonnull out, size_t capacity)
{
    struct index_ranked_note row = folio_state_discovery_row(state, index);
    (void)strcpy_s(out, capacity, ui_text_line(rank_label(row.kind), folio_state_language(state)));
}

void discovery_text_name(const struct folio_state *_Nonnull state, size_t index, char *_Nonnull out,
                         size_t capacity)
{
    struct note_ref ref = folio_state_discovery_row(state, index).ref;
    const struct ui_text_request request = {.id = UI_TEXT_DISCOVERY_PREVIEW_TITLE,
                                            .language = folio_state_language(state),
                                            .from = folio_state_category_name(state, ref.category),
                                            .name = folio_state_note_name(state, ref)};
    if (ui_text_format(&request, out, capacity) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

static enum ui_text phase_label(enum discovery_phase phase)
{
    switch (phase)
    {
    case DISCOVERY_CLOSED:
        return UI_TEXT_EMPTY;
    case DISCOVERY_PREPARING:
        return UI_TEXT_DISCOVERY_PREPARING;
    case DISCOVERY_MATCHING:
        return UI_TEXT_DISCOVERY_MATCHING;
    case DISCOVERY_READY:
        return UI_TEXT_DISCOVERY_READY;
    case DISCOVERY_FAILED:
        return UI_TEXT_EMPTY;
    }
    return UI_TEXT_EMPTY;
}

void discovery_text_status(const struct folio_state *_Nonnull state, char *_Nonnull out,
                           size_t capacity)
{
    struct discovery_summary summary = folio_state_discovery_summary(state);
    enum folio_language language = folio_state_language(state);
    if (summary.phase == DISCOVERY_FAILED)
    {
        (void)strcpy_s(out, capacity, folio_state_failure_line(summary.notice, language));
        return;
    }
    char missing[256] = "";
    const struct ui_text_request unknown = {.id = UI_TEXT_DISCOVERY_INCOMPLETE,
                                            .language = language,
                                            .k = summary.unreadable,
                                            .n = summary.unsupported};
    if (summary.unreadable != 0 || summary.unsupported != 0)
        (void)ui_text_format(&unknown, missing, sizeof missing);
    const struct ui_text_request request = {
        .id = phase_label(summary.phase),
        .language = language,
        .k = summary.phase == DISCOVERY_PREPARING ? summary.source_checked : summary.shown,
        .n = summary.phase == DISCOVERY_PREPARING ? summary.source_total : summary.total,
        .from = missing};
    if (ui_text_format(&request, out, capacity) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

void discovery_text_conditions(const struct folio_state *_Nonnull state, char *_Nonnull out,
                               size_t capacity)
{
    enum folio_language language = folio_state_language(state);
    const struct note_tags *_Nullable tags = folio_state_discovery_tags(state);
    enum ui_text scope = folio_state_discovery_scope(state) == NOTE_SCOPE_FAVORITES
                             ? UI_TEXT_DISCOVERY_FAVORITES
                             : UI_TEXT_DISCOVERY_ALL;
    enum ui_text order = folio_state_discovery_order(state) == NOTE_ORDER_FAVORITES_FIRST
                             ? UI_TEXT_DISCOVERY_FAVORITES_FIRST
                             : UI_TEXT_DISCOVERY_MANUAL;
    enum ui_text mode =
        folio_state_discovery_tag_mode(state) == TAG_MATCH_ALL ? UI_TEXT_TAG_ALL : UI_TEXT_TAG_ANY;
    const struct ui_text_request request = {.id = UI_TEXT_DISCOVERY_CONDITIONS,
                                            .language = language,
                                            .from = ui_text_line(scope, language),
                                            .to = ui_text_line(order, language),
                                            .name = ui_text_line(mode, language),
                                            .n = tags == nullptr ? 0 : note_tags_count(tags)};
    if (ui_text_format(&request, out, capacity) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

void discovery_text_tag(const struct folio_state *_Nonnull state, size_t index, char *_Nonnull out,
                        size_t capacity)
{
    const struct note_tags *_Nullable tags = folio_state_discovery_tags(state);
    out[0] = '\0';
    if (tags == nullptr || index >= note_tags_count(tags))
        return;
    const struct ui_text_request request = {.id = UI_TEXT_DISCOVERY_TAG_REMOVE,
                                            .language = folio_state_language(state),
                                            .k = index + 1,
                                            .n = note_tags_count(tags),
                                            .name = note_tag_bytes(note_tags_at(tags, index))};
    (void)ui_text_format(&request, out, capacity);
}

static enum ui_text extent_label(enum note_excerpt_extent extent)
{
    switch (extent)
    {
    case NOTE_EXCERPT_FULL:
        return UI_TEXT_DISCOVERY_FULL;
    case NOTE_EXCERPT_LINE_LIMIT:
        return UI_TEXT_DISCOVERY_LINE_LIMIT;
    case NOTE_EXCERPT_BYTE_LIMIT:
        return UI_TEXT_DISCOVERY_BYTE_LIMIT;
    }
    return UI_TEXT_EMPTY;
}

static enum ui_text preview_label(enum discovery_preview preview)
{
    switch (preview)
    {
    case DISCOVERY_PREVIEW_NONE:
        return UI_TEXT_DISCOVERY_PREVIEW_NONE;
    case DISCOVERY_PREVIEW_WAITING:
        return UI_TEXT_DISCOVERY_PREVIEW_WAIT;
    case DISCOVERY_PREVIEW_UNREADABLE:
        return UI_TEXT_DISCOVERY_PREVIEW_UNREADABLE;
    case DISCOVERY_PREVIEW_FAILED:
    case DISCOVERY_PREVIEW_READY:
        return UI_TEXT_EMPTY;
    }
    return UI_TEXT_EMPTY;
}

void discovery_text_preview(const struct folio_state *_Nonnull state, char *_Nonnull out,
                            size_t capacity)
{
    struct discovery_summary summary = folio_state_discovery_summary(state);
    enum folio_language language = folio_state_language(state);
    if (summary.preview == DISCOVERY_PREVIEW_FAILED)
    {
        (void)strcpy_s(out, capacity, folio_state_failure_line(summary.notice, language));
        return;
    }
    const struct note_excerpt *_Nullable excerpt = folio_state_discovery_excerpt(state);
    struct ui_text_request request = {.language = language, .id = preview_label(summary.preview)};
    if (excerpt != nullptr)
    {
        request.id = extent_label(note_excerpt_extent(excerpt));
        request.n = note_excerpt_line_count(excerpt);
    }
    if (ui_text_format(&request, out, capacity) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

void discovery_text_title(const struct folio_state *_Nonnull state, char *_Nonnull out,
                          size_t capacity)
{
    struct discovery_summary summary = folio_state_discovery_summary(state);
    out[0] = '\0';
    if (summary.phase != DISCOVERY_READY || summary.shown == 0)
        return;
    struct note_ref ref = folio_state_discovery_row(state, summary.selected).ref;
    const struct ui_text_request request = {.id = UI_TEXT_DISCOVERY_PREVIEW_TITLE,
                                            .language = folio_state_language(state),
                                            .from = folio_state_category_name(state, ref.category),
                                            .name = folio_state_note_name(state, ref)};
    (void)ui_text_format(&request, out, capacity);
}
