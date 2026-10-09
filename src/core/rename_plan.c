#include "rename_plan.h"
#include "category_ledger.h"
#include "category_name.h"
#include "json_reader.h"
#include "json_writer.h"
#include "name_list.h"
#include "note_ledger.h"
#include "note_name.h"
#include <stdlib.h>
#include <string.h>

struct rename_plan
{
    enum rename_kind kind;
    struct name_list *_Nullable category;
    struct name_list *_Nullable names;
    struct note_ledger *_Nullable notes;
    struct category_ledger *_Nullable categories;
};

static enum rename_plan_outcome from_name(enum name_list_outcome outcome)
{
    switch (outcome)
    {
    case NAME_LIST_ACCEPTED:
        return RENAME_PLAN_ACCEPTED;
    case NAME_LIST_DUPLICATE:
    case NAME_LIST_INVALID_NAME:
        return RENAME_PLAN_INVALID;
    case NAME_LIST_OUT_OF_MEMORY:
        return RENAME_PLAN_OUT_OF_MEMORY;
    }
    return RENAME_PLAN_INVALID;
}

static enum rename_plan_outcome from_notes(enum note_ledger_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_LEDGER_ACCEPTED:
        return RENAME_PLAN_ACCEPTED;
    case NOTE_LEDGER_MALFORMED:
    case NOTE_LEDGER_UNSUPPORTED_VERSION:
        return RENAME_PLAN_INVALID;
    case NOTE_LEDGER_OUT_OF_MEMORY:
        return RENAME_PLAN_OUT_OF_MEMORY;
    }
    return RENAME_PLAN_INVALID;
}

static enum rename_plan_outcome from_categories(enum category_ledger_outcome outcome)
{
    switch (outcome)
    {
    case CATEGORY_LEDGER_ACCEPTED:
        return RENAME_PLAN_ACCEPTED;
    case CATEGORY_LEDGER_MALFORMED:
    case CATEGORY_LEDGER_UNSUPPORTED_VERSION:
        return RENAME_PLAN_INVALID;
    case CATEGORY_LEDGER_OUT_OF_MEMORY:
        return RENAME_PLAN_OUT_OF_MEMORY;
    }
    return RENAME_PLAN_INVALID;
}

static enum rename_plan_outcome empty(enum rename_kind kind,
                                      struct rename_plan *_Nullable *_Nonnull out)
{
    struct rename_plan *_Nullable plan = calloc(1, sizeof *plan);
    if (plan == nullptr)
    {
        return RENAME_PLAN_OUT_OF_MEMORY;
    }
    plan->kind = kind;
    enum name_list_outcome outcome = name_list_create(&plan->names);
    switch (kind)
    {
    case RENAME_KIND_NOTE:
        if (outcome == NAME_LIST_ACCEPTED)
        {
            outcome = name_list_create(&plan->category);
        }
        break;
    case RENAME_KIND_CATEGORY:
        break;
    }
    if (outcome != NAME_LIST_ACCEPTED)
    {
        rename_plan_destroy(plan);
        return from_name(outcome);
    }
    *out = plan;
    return RENAME_PLAN_ACCEPTED;
}

static enum rename_plan_outcome append_name(struct name_list *_Nonnull list,
                                            const char *_Nonnull text, size_t limit)
{
    size_t length = strlen(text);
    return length > limit ? RENAME_PLAN_INVALID : from_name(name_list_append(list, text, length));
}

enum rename_plan_outcome rename_plan_create_note(const char *_Nonnull category,
                                                 const struct note_ledger *_Nonnull ledger,
                                                 const struct note_rename_target *_Nonnull target,
                                                 struct rename_plan *_Nullable *_Nonnull out)
{
    if (target->index >= note_ledger_count(ledger))
    {
        return RENAME_PLAN_INVALID;
    }
    struct rename_plan *_Nullable plan = nullptr;
    enum rename_plan_outcome outcome = empty(RENAME_KIND_NOTE, &plan);
    if (outcome == RENAME_PLAN_ACCEPTED)
    {
        outcome = append_name(plan->category, category, name_list_max_length);
    }
    if (outcome == RENAME_PLAN_ACCEPTED)
    {
        outcome = append_name(plan->names, note_ledger_name(ledger, target->index),
                              name_list_max_length - 3);
    }
    if (outcome == RENAME_PLAN_ACCEPTED)
    {
        outcome = append_name(plan->names, note_name_stem(target->name), name_list_max_length - 3);
    }
    if (outcome == RENAME_PLAN_ACCEPTED)
    {
        outcome = from_notes(
            note_ledger_renamed(ledger, target->index, note_name_stem(target->name), &plan->notes));
    }
    if (outcome != RENAME_PLAN_ACCEPTED)
    {
        rename_plan_destroy(plan);
        return outcome;
    }
    *out = plan;
    return RENAME_PLAN_ACCEPTED;
}

enum rename_plan_outcome rename_plan_create_category(const struct category_ledger *_Nonnull ledger,
                                                     size_t index,
                                                     const struct category_name *_Nonnull name,
                                                     struct rename_plan *_Nullable *_Nonnull out)
{
    if (index >= category_ledger_count(ledger))
    {
        return RENAME_PLAN_INVALID;
    }
    struct rename_plan *_Nullable plan = nullptr;
    enum rename_plan_outcome outcome = empty(RENAME_KIND_CATEGORY, &plan);
    if (outcome == RENAME_PLAN_ACCEPTED)
    {
        outcome =
            append_name(plan->names, category_ledger_name(ledger, index), name_list_max_length);
    }
    if (outcome == RENAME_PLAN_ACCEPTED)
    {
        outcome = append_name(plan->names, category_name_text(name), name_list_max_length);
    }
    if (outcome == RENAME_PLAN_ACCEPTED)
    {
        outcome = from_categories(category_ledger_renamed(ledger, index, name, &plan->categories));
    }
    if (outcome != RENAME_PLAN_ACCEPTED)
    {
        rename_plan_destroy(plan);
        return outcome;
    }
    *out = plan;
    return RENAME_PLAN_ACCEPTED;
}

static bool key(struct json_reader *_Nonnull reader, const char *_Nonnull expected)
{
    return json_reader_next(reader) == JSON_TOKEN_KEY &&
           json_reader_text_length(reader) == strlen(expected) &&
           strcmp(json_reader_text(reader), expected) == 0;
}

/* reader の OUT_OF_MEMORY は sticky。構文不正へ丸めずに保持する。 */
static enum rename_plan_outcome unexpected(struct json_reader *_Nonnull reader)
{
    return json_reader_next(reader) == JSON_TOKEN_OUT_OF_MEMORY ? RENAME_PLAN_OUT_OF_MEMORY
                                                                : RENAME_PLAN_INVALID;
}

static size_t name_limit(enum rename_kind kind)
{
    switch (kind)
    {
    case RENAME_KIND_NOTE:
        return name_list_max_length - 3;
    case RENAME_KIND_CATEGORY:
        return name_list_max_length;
    }
    return 0;
}

static enum rename_plan_outcome read_name(struct json_reader *_Nonnull reader,
                                          struct name_list *_Nonnull list,
                                          const char *_Nonnull field, size_t limit)
{
    if (!key(reader, field) || json_reader_next(reader) != JSON_TOKEN_STRING)
    {
        return unexpected(reader);
    }
    size_t length = json_reader_text_length(reader);
    return length > limit ? RENAME_PLAN_INVALID
                          : from_name(name_list_append(list, json_reader_text(reader), length));
}

static enum rename_plan_outcome read_category_to(struct json_reader *_Nonnull reader,
                                                 struct name_list *_Nonnull names)
{
    if (!key(reader, "to") || json_reader_next(reader) != JSON_TOKEN_STRING)
    {
        return unexpected(reader);
    }
    struct category_name *_Nullable name = nullptr;
    enum category_name_outcome outcome =
        category_name_create(json_reader_text(reader), json_reader_text_length(reader), &name);
    enum rename_plan_outcome result = RENAME_PLAN_INVALID;
    switch (outcome)
    {
    case CATEGORY_NAME_ACCEPTED:
        result = append_name(names, category_name_text(name), name_list_max_length);
        break;
    case CATEGORY_NAME_INVALID:
        break;
    case CATEGORY_NAME_OUT_OF_MEMORY:
        result = RENAME_PLAN_OUT_OF_MEMORY;
        break;
    }
    category_name_destroy(name);
    return result;
}

static enum rename_plan_outcome read_to(struct json_reader *_Nonnull reader,
                                        struct rename_plan *_Nonnull plan)
{
    switch (plan->kind)
    {
    case RENAME_KIND_NOTE:
        return read_name(reader, plan->names, "to", name_limit(plan->kind));
    case RENAME_KIND_CATEGORY:
        return read_category_to(reader, plan->names);
    }
    return RENAME_PLAN_INVALID;
}

static enum rename_plan_outcome read_ledger(struct json_reader *_Nonnull reader,
                                            struct rename_plan *_Nonnull plan)
{
    switch (plan->kind)
    {
    case RENAME_KIND_NOTE:
        return from_notes(note_ledger_read(reader, &plan->notes));
    case RENAME_KIND_CATEGORY:
        return from_categories(category_ledger_read(reader, &plan->categories));
    }
    return RENAME_PLAN_INVALID;
}

static size_t ledger_count(const struct rename_plan *_Nonnull plan)
{
    switch (plan->kind)
    {
    case RENAME_KIND_NOTE:
        return note_ledger_count(plan->notes);
    case RENAME_KIND_CATEGORY:
        return category_ledger_count(plan->categories);
    }
    return 0;
}

static const char *_Nonnull ledger_name(const struct rename_plan *_Nonnull plan, size_t index)
{
    switch (plan->kind)
    {
    case RENAME_KIND_NOTE:
        return note_ledger_name(plan->notes, index);
    case RENAME_KIND_CATEGORY:
        return category_ledger_name(plan->categories, index);
    }
    return "";
}

static bool agrees_with_ledger(const struct rename_plan *_Nonnull plan)
{
    bool found = false;
    size_t count = ledger_count(plan);
    for (size_t index = 0; index < count; ++index)
    {
        const char *_Nonnull name = ledger_name(plan, index);
        if (strcmp(name, rename_plan_from(plan)) == 0)
        {
            return false;
        }
        found = found || strcmp(name, rename_plan_to(plan)) == 0;
    }
    return found;
}

static enum rename_plan_outcome read_completed_ledger(struct json_reader *_Nonnull reader,
                                                      struct rename_plan *_Nonnull plan)
{
    if (!key(reader, "ledger"))
    {
        return unexpected(reader);
    }
    enum rename_plan_outcome outcome = read_ledger(reader, plan);
    if (outcome != RENAME_PLAN_ACCEPTED)
    {
        return outcome == RENAME_PLAN_INVALID ? unexpected(reader) : outcome;
    }
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_END)
    {
        return unexpected(reader);
    }
    return agrees_with_ledger(plan) ? RENAME_PLAN_ACCEPTED : RENAME_PLAN_INVALID;
}

static enum rename_plan_outcome read_document(struct json_reader *_Nonnull reader,
                                              struct rename_plan *_Nonnull plan)
{
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_BEGIN)
    {
        return unexpected(reader);
    }
    enum rename_plan_outcome outcome = RENAME_PLAN_ACCEPTED;
    switch (plan->kind)
    {
    case RENAME_KIND_NOTE:
        outcome = read_name(reader, plan->category, "category", name_list_max_length);
        break;
    case RENAME_KIND_CATEGORY:
        break;
    }
    if (outcome == RENAME_PLAN_ACCEPTED)
    {
        outcome = read_name(reader, plan->names, "from", name_limit(plan->kind));
    }
    if (outcome == RENAME_PLAN_ACCEPTED)
    {
        outcome = read_to(reader, plan);
    }
    if (outcome != RENAME_PLAN_ACCEPTED)
    {
        return outcome;
    }
    return read_completed_ledger(reader, plan);
}

enum rename_plan_outcome rename_plan_read(struct json_reader *_Nonnull reader,
                                          enum rename_kind kind,
                                          struct rename_plan *_Nullable *_Nonnull out)
{
    struct rename_plan *_Nullable plan = nullptr;
    enum rename_plan_outcome outcome = empty(kind, &plan);
    if (outcome == RENAME_PLAN_ACCEPTED)
    {
        outcome = read_document(reader, plan);
    }
    if (outcome != RENAME_PLAN_ACCEPTED)
    {
        rename_plan_destroy(plan);
        return outcome;
    }
    *out = plan;
    return RENAME_PLAN_ACCEPTED;
}

void rename_plan_write(const struct rename_plan *_Nonnull plan, struct json_writer *_Nonnull writer)
{
    json_writer_object_begin(writer);
    switch (plan->kind)
    {
    case RENAME_KIND_NOTE:
        json_writer_key(writer, "category");
        json_writer_string(writer, rename_plan_category(plan));
        break;
    case RENAME_KIND_CATEGORY:
        break;
    }
    json_writer_key(writer, "from");
    json_writer_string(writer, rename_plan_from(plan));
    json_writer_key(writer, "to");
    json_writer_string(writer, rename_plan_to(plan));
    json_writer_key(writer, "ledger");
    switch (plan->kind)
    {
    case RENAME_KIND_NOTE:
        note_ledger_write(plan->notes, writer);
        break;
    case RENAME_KIND_CATEGORY:
        category_ledger_write(plan->categories, writer);
        break;
    }
    json_writer_object_end(writer);
}

enum rename_kind rename_plan_kind(const struct rename_plan *_Nonnull plan)
{
    return plan->kind;
}

const char *_Nonnull rename_plan_category(const struct rename_plan *_Nonnull plan)
{
    return name_list_at(plan->category, 0);
}

const char *_Nonnull rename_plan_from(const struct rename_plan *_Nonnull plan)
{
    return name_list_at(plan->names, 0);
}

const char *_Nonnull rename_plan_to(const struct rename_plan *_Nonnull plan)
{
    return name_list_at(plan->names, 1);
}

const struct note_ledger *_Nonnull rename_plan_note_ledger(const struct rename_plan *_Nonnull plan)
{
    return plan->notes;
}

const struct category_ledger *_Nonnull rename_plan_category_ledger(
    const struct rename_plan *_Nonnull plan)
{
    return plan->categories;
}

static bool category_row_equals(const struct category_ledger *_Nonnull left,
                                const struct category_ledger *_Nonnull right, size_t index)
{
    struct rgb_color a = category_ledger_color(left, index);
    struct rgb_color b = category_ledger_color(right, index);
    return a.red == b.red && a.green == b.green && a.blue == b.blue &&
           category_ledger_expanded(left, index) == category_ledger_expanded(right, index);
}

static bool ledger_row_equals(const struct rename_plan *_Nonnull left,
                              const struct rename_plan *_Nonnull right, size_t index)
{
    if (strcmp(ledger_name(left, index), ledger_name(right, index)) != 0)
    {
        return false;
    }
    switch (left->kind)
    {
    case RENAME_KIND_NOTE:
        return true;
    case RENAME_KIND_CATEGORY:
        return category_row_equals(left->categories, right->categories, index);
    }
    return false;
}

static bool ledger_equals(const struct rename_plan *_Nonnull left,
                          const struct rename_plan *_Nonnull right)
{
    size_t count = ledger_count(left);
    if (count != ledger_count(right))
    {
        return false;
    }
    for (size_t index = 0; index < count; ++index)
    {
        if (!ledger_row_equals(left, right, index))
        {
            return false;
        }
    }
    return true;
}

bool rename_plan_equals(const struct rename_plan *_Nonnull left,
                        const struct rename_plan *_Nonnull right)
{
    if (left->kind != right->kind || strcmp(rename_plan_from(left), rename_plan_from(right)) != 0 ||
        strcmp(rename_plan_to(left), rename_plan_to(right)) != 0)
    {
        return false;
    }
    switch (left->kind)
    {
    case RENAME_KIND_NOTE:
        if (strcmp(rename_plan_category(left), rename_plan_category(right)) != 0)
        {
            return false;
        }
        break;
    case RENAME_KIND_CATEGORY:
        break;
    }
    return ledger_equals(left, right);
}

struct note_ledger *_Nonnull rename_plan_take_note_ledger(struct rename_plan *_Nonnull plan)
{
    struct note_ledger *_Nonnull ledger = plan->notes;
    plan->notes = nullptr;
    return ledger;
}

struct category_ledger *_Nonnull rename_plan_take_category_ledger(struct rename_plan *_Nonnull plan)
{
    struct category_ledger *_Nonnull ledger = plan->categories;
    plan->categories = nullptr;
    return ledger;
}

void rename_plan_destroy(struct rename_plan *_Nullable plan)
{
    if (plan == nullptr)
    {
        return;
    }
    name_list_destroy(plan->category);
    name_list_destroy(plan->names);
    note_ledger_destroy(plan->notes);
    category_ledger_destroy(plan->categories);
    free(plan);
}
