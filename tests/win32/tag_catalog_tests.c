/* #819: saved-catalog scale and the owned UI projection, outside the editor desktop. */
#include "note_tag.h"
#include "note_tags.h"
#include "tag_candidates.h"
#include "tag_candidates_source.h"
#include "tag_catalog.h"
#include "utf8_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool measured;

static void require(bool condition, const char *_Nonnull message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static struct note_tags *_Nonnull source_tags(size_t index)
{
    char name[32];
    require(snprintf(name, sizeof name, "tag-%06zu", index) > 0, "scale name fits");
    struct note_tag *shared = nullptr, *unique = nullptr;
    require(note_tag_create("shared", 6, &shared) == NOTE_TAG_READY &&
                note_tag_create(name, strlen(name), &unique) == NOTE_TAG_READY,
            "validated scale names");
    const struct note_tag *names[] = {shared, unique};
    struct note_tags *tags = nullptr;
    require(note_tags_create(names, 2, &tags) == NOTE_TAGS_READY, "scale source set");
    note_tag_destroy(shared);
    note_tag_destroy(unique);
    return tags;
}

static void phase(const char *_Nonnull name, size_t count, size_t run)
{
    printf("{\"phase\":\"%s\",\"notes\":%zu,\"sample\":%zu}\n", name, count, run);
    require(fflush(stdout) == 0, "publish measurement phase");
    if (measured)
        require(getchar() == '\n', "measurement driver acknowledges the phase");
}

static void project(struct tag_catalog *_Nonnull catalog, size_t count)
{
    struct utf8_text *query = nullptr;
    struct tag_candidates *candidates = nullptr;
    require(utf8_text_create(u"", 0, &query) == UTF8_TEXT_CONVERTED &&
                tag_candidates_create(&candidates) == CANDIDATE_CREATED,
            "projection owners");
    struct tag_candidates_source source = {.catalog = catalog, .query = query, .revision = 1};
    require(tag_candidates_prepare(candidates, &source) == CANDIDATE_CREATED &&
                tag_candidates_count(candidates) == count + 1 &&
                tag_candidates_usage(candidates, 0) == count,
            "all unique names and saved note counts survive projection");
    tag_catalog_destroy(catalog);
    require(strcmp(note_tag_bytes(tag_candidates_tag(candidates, 0)), "shared") == 0,
            "projection retains owned names after catalog release");
    tag_candidates_invalidate(candidates);
    require(!tag_candidates_current(candidates, 1) && tag_candidates_count(candidates) == 0,
            "invalidated projection releases all visible names");
    tag_candidates_destroy(candidates);
    utf8_text_destroy(query);
}

static void sample(const struct note_tags *_Nullable const *_Nonnull notes, size_t count,
                   size_t run)
{
    struct tag_catalog *catalog = nullptr;
    phase("ready", count, run);
    require(tag_catalog_create(notes, count + 1, &catalog) == TAG_CATALOG_READY,
            "completed catalog construction");
    phase("built", count, run);
    require(tag_catalog_count(catalog) == count + 1 && tag_catalog_unavailable(catalog) == 1 &&
                tag_catalog_usage(catalog, 0) == count,
            "scale names, counts and unavailable are exact");
    project(catalog, count);
    phase("released", count, run);
}

static void measure(size_t count)
{
    struct note_tags **owned = calloc(count, sizeof *owned);
    const struct note_tags **notes = calloc(count + 1, sizeof *notes);
    require(owned != nullptr && notes != nullptr, "scale reference arrays");
    for (size_t at = 0; at < count; ++at)
    {
        owned[at] = source_tags(at);
        notes[at] = owned[at];
    }
    for (size_t run = 0; run < 7; ++run)
        sample(notes, count, run);
    for (size_t at = 0; at < count; ++at)
        note_tags_destroy(owned[at]);
    free(owned);
    free(notes);
}

int main(int argc, char *_Nonnull argv[])
{
    measured = argc == 2 && strcmp(argv[1], "--measure") == 0;
    require(argc == 1 || measured, "optional --measure protocol");
    const size_t counts[] = {100, 1000, 10000};
    for (size_t at = 0; at < sizeof counts / sizeof counts[0]; ++at)
        measure(counts[at]);
    return EXIT_SUCCESS;
}
