#include "replace_preview.h"

#include "regex_matches.h"
#include "replace_template.h"

#include <stdlib.h>
#include <string.h>

struct replace_preview
{
    char16_t *_Nonnull text;
    size_t length;
    struct regex_matches list; /* items は引き取った配列。count は総数のまま残す */
    struct replace_template *_Nonnull replacement;
    char *_Nonnull category; /* 終端付き UTF-8 */
    char *_Nonnull note;     /* 終端付き UTF-8。無題は空文字列 */
};

static char *_Nullable copy_bytes(const char *_Nonnull text)
{
    size_t length = strlen(text) + 1;
    char *_Nullable copy = malloc(length);
    if (copy != nullptr)
    {
        memcpy(copy, text, length);
    }
    return copy;
}

static void source_destroy(char16_t *_Nullable text, char *_Nullable category, char *_Nullable note)
{
    free(text);
    free(category);
    free(note);
}

static bool adopt_source(struct replace_preview *_Nonnull preview,
                         const struct replace_source *_Nonnull source)
{
    char16_t *_Nullable text = calloc(source->length + 1, sizeof *text);
    char *_Nullable category = copy_bytes(source->category);
    char *_Nullable note = copy_bytes(source->note);
    if (text == nullptr || category == nullptr || note == nullptr)
    {
        source_destroy(text, category, note);
        return false;
    }
    preview->text = text;
    preview->category = category;
    preview->note = note;
    memcpy(preview->text, source->text, source->length * sizeof *preview->text);
    preview->length = source->length;
    return true;
}

/* 一致の配列は写さずに引き取る（補正 25）。失敗しうる確保をすべて済ませてから呼ぶので、
 * ここは失敗しない。count は総数のまま残す（capacity を超えていれば組み立てが断る）。 */
static void adopt_matches(struct replace_preview *_Nonnull preview,
                          struct regex_matches *_Nonnull matches)
{
    preview->list = *matches;
    matches->capacity = 0;
    matches->count = 0;
}

static enum replace_preview_outcome from_template(enum replace_template_outcome parsed)
{
    switch (parsed)
    {
    case REPLACE_TEMPLATE_READY:
        return REPLACE_PREVIEW_READY;
    case REPLACE_TEMPLATE_MALFORMED:
        return REPLACE_PREVIEW_BAD_TEMPLATE;
    case REPLACE_TEMPLATE_OUT_OF_MEMORY:
        return REPLACE_PREVIEW_OUT_OF_MEMORY;
    }
    return REPLACE_PREVIEW_BAD_TEMPLATE;
}

static void construction_destroy(struct replace_preview *_Nonnull preview,
                                 struct replace_template *_Nullable replacement)
{
    replace_template_destroy(replacement);
    free(preview);
}

enum replace_preview_outcome replace_preview_create(const struct replace_source *_Nonnull source,
                                                    struct regex_matches *_Nonnull matches,
                                                    struct replace_preview *_Nullable *_Nonnull out)
{
    struct replace_preview *_Nullable preview = calloc(1, sizeof *preview);
    if (preview == nullptr)
    {
        return REPLACE_PREVIEW_OUT_OF_MEMORY;
    }
    struct replace_template *_Nullable replacement = nullptr;
    enum replace_preview_outcome parsed = from_template(
        replace_template_create(source->replacement, source->replacement_length, &replacement));
    if (parsed != REPLACE_PREVIEW_READY)
    {
        construction_destroy(preview, replacement);
        return parsed;
    }
    if (!adopt_source(preview, source))
    {
        construction_destroy(preview, replacement);
        return REPLACE_PREVIEW_OUT_OF_MEMORY;
    }
    preview->replacement = replacement;
    adopt_matches(preview, matches);
    *out = preview;
    return REPLACE_PREVIEW_READY;
}

size_t replace_preview_count(const struct replace_preview *_Nonnull preview)
{
    return preview->list.count;
}

bool replace_preview_holds(const struct replace_preview *_Nonnull preview,
                           const struct replace_source *_Nonnull source)
{
    return preview->length == source->length &&
           memcmp(preview->text, source->text, preview->length * sizeof *preview->text) == 0 &&
           strcmp(preview->category, source->category) == 0 &&
           strcmp(preview->note, source->note) == 0;
}

struct note_replace_plan replace_preview_plan(const struct replace_preview *_Nonnull preview,
                                              enum replace_scope scope,
                                              struct note_search_span anchor)
{
    struct note_replace_plan plan = {.text = preview->text,
                                     .length = preview->length,
                                     .matches = &preview->list,
                                     .replacement = preview->replacement,
                                     .scope = scope,
                                     .anchor = anchor};
    return plan;
}

void replace_preview_destroy(struct replace_preview *_Nullable preview)
{
    if (preview == nullptr)
    {
        return;
    }
    replace_template_destroy(preview->replacement);
    free(preview->text);
    free(preview->list.items);
    free(preview->category);
    free(preview->note);
    free(preview);
}
