#include "discovery_target.h"
#include "note_identity.h"
#include "note_text.h"
#include <stdlib.h>

struct discovery_target
{
    struct note_identity *_Nonnull identity;
    struct note_text *_Nonnull body;
    struct note_search_span selection;
};

enum discovery_target_outcome
discovery_target_create(const struct note_identity *_Nonnull identity,
                        const struct note_text *_Nonnull body,
                        const struct note_source_match *_Nonnull match,
                        struct discovery_target *_Nullable *_Nonnull out)
{
    if (match->bytes.start >= match->bytes.end || match->bytes.end > note_text_length(body) ||
        match->native.start >= match->native.end || match->native.end > match->bytes.end)
        return DISCOVERY_TARGET_INVALID_RANGE;
    if (note_text_editability(body) == NOTE_TEXT_EDITOR_ALTERS)
        return DISCOVERY_TARGET_UNEDITABLE;
    struct discovery_target *_Nullable target = malloc(sizeof *target);
    struct note_identity *_Nullable copied_identity = nullptr;
    struct note_text *_Nullable copied_body = nullptr;
    if (target == nullptr ||
        note_identity_create(note_identity_category(identity), note_identity_note(identity),
                             &copied_identity) != NOTE_IDENTITY_READY ||
        note_text_copy(body, &copied_body) != NOTE_TEXT_ACCEPTED)
    {
        note_identity_destroy(copied_identity);
        note_text_destroy(copied_body);
        free(target);
        return DISCOVERY_TARGET_OUT_OF_MEMORY;
    }
    target->identity = copied_identity;
    target->body = copied_body;
    target->selection = match->native;
    *out = target;
    return DISCOVERY_TARGET_READY;
}

const struct note_identity *_Nonnull discovery_target_identity(
    const struct discovery_target *_Nonnull target)
{
    return target->identity;
}

const struct note_text *_Nonnull discovery_target_body(
    const struct discovery_target *_Nonnull target)
{
    return target->body;
}

struct note_search_span discovery_target_selection(const struct discovery_target *_Nonnull target)
{
    return target->selection;
}

void discovery_target_destroy(struct discovery_target *_Nullable target)
{
    if (target == nullptr)
        return;
    note_identity_destroy(target->identity);
    note_text_destroy(target->body);
    free(target);
}
