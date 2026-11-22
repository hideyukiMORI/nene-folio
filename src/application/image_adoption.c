#include "image_adoption.h"
#include "image_adopt_outcome.h"
#include "image_link.h"
#include "image_port.h"
#include "note_identity.h"
#include "storage_path.h"
#include "text_buffer.h"
#include <stdlib.h>
struct image_adoption
{
    struct note_identity *_Nonnull destination;
    struct text_buffer *_Nonnull text;
    struct storage_path *_Nullable sources[image_adoption_limit];
    size_t count; /* sources のうち、写し終えた数（＝扱う数） */
    size_t next;
    struct image_adoption_summary counts;
    bool displaced; /* 宛先のノートから離れた・本文が入れ替わった。ためた文はもう入れられない */
};

/* destination は消さない: 作りかけを片付けるときは、まだ呼んだ側のもの。 */
static void discard(struct image_adoption *_Nonnull adoption)
{
    for (size_t at = 0; at < adoption->count; ++at)
        storage_path_destroy(adoption->sources[at]);
    text_buffer_destroy(adoption->text);
    free(adoption);
}

static bool copy_sources(struct image_adoption *_Nonnull adoption,
                         struct storage_path *_Nonnull const *_Nonnull paths, size_t count)
{
    for (size_t at = 0; at < count; ++at)
    {
        if (storage_path_copy(paths[at], &adoption->sources[at]) != STORAGE_PATH_READY)
            return false;
        adoption->count = at + 1;
    }
    return true;
}

enum image_adoption_start image_adoption_create(struct note_identity *_Nonnull destination,
                                                struct storage_path *_Nonnull const *_Nonnull paths,
                                                size_t count,
                                                struct image_adoption *_Nullable *_Nonnull out)
{
    if (count == 0)
        return IMAGE_ADOPTION_NOTHING;
    struct text_buffer *_Nullable text = nullptr;
    if (text_buffer_create(&text) != TEXT_BUFFER_ACCEPTED || text == nullptr)
        return IMAGE_ADOPTION_OUT_OF_MEMORY;
    struct image_adoption *_Nullable adoption = calloc(1, sizeof *adoption);
    if (adoption == nullptr)
    {
        text_buffer_destroy(text);
        return IMAGE_ADOPTION_OUT_OF_MEMORY;
    }
    adoption->destination = destination;
    adoption->text = text;
    size_t handled = count < image_adoption_limit ? count : image_adoption_limit;
    if (!copy_sources(adoption, paths, handled))
    {
        discard(adoption);
        return IMAGE_ADOPTION_OUT_OF_MEMORY;
    }
    adoption->counts = (struct image_adoption_summary){
        .dropped = count, .beyond = count - handled, .pending = handled};
    *out = adoption;
    return IMAGE_ADOPTION_STARTED;
}

/* leaf（images/ からの相対パス）を指す Markdown を、1 回の追加でためた文へ足す。途中で記憶が
 * 足りなくなっても、ためた文に書きかけの記法を残さない。 */
static enum image_adopt_outcome link(struct image_adoption *_Nonnull adoption,
                                     const struct text_buffer *_Nonnull leaf,
                                     enum image_adopt_outcome taken)
{
    if (text_buffer_finish(leaf) != TEXT_BUFFER_ACCEPTED)
        return IMAGE_ADOPT_OUT_OF_MEMORY;
    if (text_buffer_length(leaf) == 0)
        return IMAGE_ADOPT_NOT_STORED;
    struct text_buffer *_Nullable piece = nullptr;
    if (text_buffer_create(&piece) != TEXT_BUFFER_ACCEPTED || piece == nullptr)
        return IMAGE_ADOPT_OUT_OF_MEMORY;
    if (adoption->counts.taken != 0)
        text_buffer_append_text(piece, "\n");
    image_link_append(piece, text_buffer_bytes(leaf), text_buffer_length(leaf));
    bool whole = text_buffer_finish(piece) == TEXT_BUFFER_ACCEPTED;
    if (whole)
        text_buffer_append(adoption->text, text_buffer_bytes(piece), text_buffer_length(piece));
    text_buffer_destroy(piece);
    return whole && text_buffer_finish(adoption->text) == TEXT_BUFFER_ACCEPTED
               ? taken
               : IMAGE_ADOPT_OUT_OF_MEMORY;
}

static enum image_adopt_outcome take(struct image_adoption *_Nonnull adoption,
                                     const struct image_port *_Nonnull port,
                                     const struct storage_path *_Nonnull source)
{
    struct text_buffer *_Nullable leaf = nullptr;
    if (text_buffer_create(&leaf) != TEXT_BUFFER_ACCEPTED || leaf == nullptr)
        return IMAGE_ADOPT_OUT_OF_MEMORY;
    enum image_adopt_outcome outcome =
        port->adopt(port->adapter, note_identity_category(adoption->destination), source, leaf);
    if (outcome == IMAGE_ADOPT_COPIED || outcome == IMAGE_ADOPT_PRESENT)
        outcome = link(adoption, leaf, outcome);
    text_buffer_destroy(leaf);
    return outcome;
}

static void record(struct image_adoption_summary *_Nonnull counts, enum image_adopt_outcome outcome)
{
    switch (outcome)
    {
    case IMAGE_ADOPT_COPIED:
    case IMAGE_ADOPT_PRESENT:
        ++counts->taken;
        return;
    case IMAGE_ADOPT_NOT_IMAGE:
        ++counts->not_image;
        return;
    case IMAGE_ADOPT_TOO_LARGE:
        ++counts->too_large;
        return;
    case IMAGE_ADOPT_UNREADABLE:
        ++counts->unreadable;
        return;
    case IMAGE_ADOPT_NOT_STORED:
        ++counts->not_stored;
        return;
    case IMAGE_ADOPT_OUT_OF_MEMORY:
        ++counts->out_of_memory;
        return;
    }
}

static void finish_early(struct image_adoption *_Nonnull adoption)
{
    if (adoption->counts.finished)
        return;
    adoption->counts.finished = true;
    adoption->counts.interrupted = adoption->counts.pending != 0;
}

enum image_adoption_step image_adoption_advance(struct image_adoption *_Nonnull adoption,
                                                const struct image_port *_Nonnull port)
{
    if (adoption->counts.finished)
        return IMAGE_ADOPTION_IDLE;
    const struct storage_path *_Nullable source = adoption->sources[adoption->next];
    if (source == nullptr)
    {
        finish_early(adoption);
        return IMAGE_ADOPTION_FINISHED;
    }
    ++adoption->next;
    --adoption->counts.pending;
    record(&adoption->counts, take(adoption, port, source));
    /* ためた文をもう伸ばせないなら、残りは写しても Markdown を書けない。写さずに終える。 */
    if (text_buffer_finish(adoption->text) != TEXT_BUFFER_ACCEPTED)
        finish_early(adoption);
    if (!adoption->counts.finished && adoption->next < adoption->count)
        return IMAGE_ADOPTION_PROGRESSED;
    adoption->counts.finished = true;
    return IMAGE_ADOPTION_FINISHED;
}

void image_adoption_stop(struct image_adoption *_Nonnull adoption)
{
    adoption->displaced = true;
    finish_early(adoption);
}

bool image_adoption_landable(const struct image_adoption *_Nonnull adoption)
{
    return !adoption->displaced;
}

struct image_adoption_summary
image_adoption_summary_of(const struct image_adoption *_Nonnull adoption)
{
    return adoption->counts;
}

const struct note_identity *_Nonnull image_adoption_destination(
    const struct image_adoption *_Nonnull adoption)
{
    return adoption->destination;
}

const char *_Nonnull image_adoption_text(const struct image_adoption *_Nonnull adoption)
{
    return text_buffer_bytes(adoption->text);
}

void image_adoption_destroy(struct image_adoption *_Nullable adoption)
{
    if (adoption == nullptr)
        return;
    note_identity_destroy(adoption->destination);
    discard(adoption);
}
