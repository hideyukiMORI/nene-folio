#include "allocation_probe.h"
#include "note_metadata.h"
#include "note_tag.h"
#include "note_tags.h"
#include "note_text.h"
#include "persisted_size_limit.h"
#include "tag_update.h"
#include "tag_update_request.h"
#include "unit_tests.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct note_tag *_Nonnull tag(const char *_Nonnull name)
{
    struct note_tag *_Nullable value = nullptr;
    require(note_tag_create(name, strlen(name), &value) == NOTE_TAG_READY, "valid management tag");
    return value;
}

static struct note_text *_Nonnull text(const char *_Nonnull source)
{
    struct note_text *_Nullable value = nullptr;
    require(note_text_create(source, strlen(source), &value) == NOTE_TEXT_ACCEPTED,
            "valid management source");
    return value;
}

static struct tag_update *_Nonnull prepared(const char *_Nonnull source,
                                            const char *_Nullable replacement)
{
    struct note_text *body = text(source);
    struct note_tag *from = tag("old");
    struct note_tag *_Nullable to = replacement == nullptr ? nullptr : tag(replacement);
    const struct tag_update_request request = {"仕事", "図 😀", body, from, to};
    struct tag_update *_Nullable update = nullptr;
    require(tag_update_create(&request, &update) == TAG_UPDATE_READY,
            "prepare management snapshot");
    note_text_destroy(body);
    note_tag_destroy(from);
    note_tag_destroy(to);
    return update;
}

static void golden(const char *_Nonnull source, const char *_Nullable replacement,
                   const char *_Nonnull expected)
{
    struct tag_update *update = prepared(source, replacement);
    const struct note_text *_Nullable before = tag_update_source(update);
    const struct note_text *_Nullable after = tag_update_body(update);
    require(tag_update_kind(update) == TAG_UPDATE_CHANGE && before != nullptr && after != nullptr,
            "changed preview owns both complete texts");
    require(same_text(tag_update_category(update), "仕事") &&
                same_text(tag_update_note(update), "図 😀"),
            "preview owns copied Unicode identity after input destruction");
    require(same_text(note_text_bytes(before), source) &&
                same_text(note_text_bytes(after), expected),
            "independent golden bytes preserve original and exact intended patch");
    tag_update_destroy(update);
}

static void transformations(void)
{
    golden("---\r\ntitle: untouched\n# comment\r\ntags: [old, \"keep\", old] # "
           "inline\r\n---\nBody\rline\n",
           "new",
           "---\r\ntitle: untouched\n# comment\r\ntags: [\"keep\", \"new\"] # "
           "inline\r\n---\nBody\rline\n");
    golden("---\ntags:\n  - old # gone\n  # standalone\n  - 'keep' # retained\n---\nBody", "new",
           "---\ntags:\n  # standalone\n  - 'keep' # retained\n  - \"new\"\n---\nBody");
    golden("---\ntags: [old, new, old]\n---\nbody", "new", "---\ntags: [new]\n---\nbody");
    golden("---\ntags: [old, old]\n---\nbody", nullptr, "---\ntags: []\n---\nbody");
    golden("---\ntags: [old]\n---\n\xef\xbb\xbf"
           "body",
           "仕事 😀",
           "---\ntags: [\"仕事 😀\"]\n---\n\xef\xbb\xbf"
           "body");
    golden("---\ntags: [old]\n---\nbody", "quote\"\\name",
           "---\ntags: [\"quote\\\"\\\\name\"]\n---\nbody");
}

static void non_changes(void)
{
    const char *sources[] = {"plain", "---\ntags: [keep]\n---\nbody", "---\ntags: [old]\n---\nbody",
                             "---\ntags: *alias\n---\nbody"};
    const enum tag_update_kind kinds[] = {TAG_UPDATE_NO_MATCH, TAG_UPDATE_NO_MATCH,
                                          TAG_UPDATE_NO_MATCH, TAG_UPDATE_UNAVAILABLE};
    for (size_t at = 0; at < sizeof sources / sizeof sources[0]; ++at)
    {
        struct tag_update *update = prepared(sources[at], "old");
        require(tag_update_kind(update) == kinds[at] && tag_update_source(update) == nullptr &&
                    tag_update_body(update) == nullptr,
                "no-match and unavailable previews have no saved-body change");
        tag_update_destroy(update);
    }
    struct note_tag *from = tag("old");
    const struct tag_update_request missing = {"folder", "missing", nullptr, from, nullptr};
    struct tag_update *update = nullptr;
    require(tag_update_create(&missing, &update) == TAG_UPDATE_READY &&
                tag_update_kind(update) == TAG_UPDATE_UNREADABLE &&
                tag_update_body(update) == nullptr,
            "unreadable input is not known untagged");
    tag_update_destroy(update);
    note_tag_destroy(from);
}

static void allocation_sweep(void)
{
    struct note_text *body = text("---\ntitle: original\ntags: [old, keep]\n---\nbody");
    struct note_tag *from = tag("old");
    struct note_tag *to = tag("new");
    const struct tag_update_request request = {"folder", "one", body, from, to};
    struct tag_update *sentinel = nullptr;
    require(tag_update_create(&request, &sentinel) == TAG_UPDATE_READY, "existing preview output");
    size_t failures = 0;
    for (size_t nth = 1; nth < 512; ++nth)
    {
        struct tag_update *result = sentinel;
        allocation_probe_fail_at(nth);
        enum tag_update_outcome made = tag_update_create(&request, &result);
        allocation_probe_fail_at(0);
        if (made == TAG_UPDATE_READY)
        {
            tag_update_destroy(result);
            break;
        }
        require(made == TAG_UPDATE_OUT_OF_MEMORY && result == sentinel,
                "every preview allocation failure preserves out and source");
        ++failures;
    }
    require(failures < 511, "preview allocation sweep reaches success");
    require(same_text(note_text_bytes(body), "---\ntitle: original\ntags: [old, keep]\n---\nbody"),
            "allocation failures do not change the source");
    printf("tag update: %zu injected allocation failures\n", failures);
    tag_update_destroy(sentinel);
    note_text_destroy(body);
    note_tag_destroy(from);
    note_tag_destroy(to);
}

static void rewrite_limits(void)
{
    char *source = malloc(persisted_size_limit + 1);
    require(source != nullptr, "full-size fixed source fixture");
    if (source == nullptr)
        return;
    const char prefix[] = "---\ntags: [old]\n---\n";
    memcpy(source, prefix, sizeof prefix - 1);
    memset(source + sizeof prefix - 1, 'x', persisted_size_limit - (sizeof prefix - 1));
    source[persisted_size_limit] = '\0';
    struct tag_update *update = prepared(source, "longer-name");
    require(tag_update_kind(update) == TAG_UPDATE_LIMIT && tag_update_body(update) == nullptr,
            "batch rewrite refuses a persisted source exceeding sixteen MiB");
    tag_update_destroy(update);
    const char header[] = "---\ntags: [old]\n#";
    memcpy(source, header, sizeof header - 1);
    size_t closing = note_metadata_max_bytes - 8;
    memset(source + sizeof header - 1, 'x', closing - (sizeof header - 1));
    memcpy(source + closing, "\n---\nbody", 10);
    struct note_text *body = text(source);
    require(note_metadata_status(note_text_metadata(body)) == NOTE_METADATA_SUPPORTED,
            "original header fits immediately below the metadata byte limit");
    note_text_destroy(body);
    update = prepared(source, "longer-name-than-the-remaining-header-capacity");
    require(tag_update_kind(update) == TAG_UPDATE_LIMIT && tag_update_source(update) == nullptr,
            "expanded header is revalidated and cannot become unreadable after publication");
    tag_update_destroy(update);
    free(source);
}

static void full_tag_set(void)
{
    char source[1024] = "---\ntags:\n  - old\n";
    size_t at = strlen(source);
    for (size_t index = 1; index < note_tags_max_count; ++index)
    {
        int written = snprintf(source + at, sizeof source - at, "  - tag%zu\n", index);
        require(written > 0 && (size_t)written < sizeof source - at, "full tag source fits");
        at += (size_t)written;
    }
    memcpy(source + at, "---\nbody", 9);
    struct tag_update *update = prepared(source, "new");
    const struct note_text *body = tag_update_body(update);
    require(tag_update_kind(update) == TAG_UPDATE_CHANGE && body != nullptr &&
                note_tags_count(note_metadata_tags(note_text_metadata(body))) ==
                    note_tags_max_count,
            "rename removes old before adding new when all thirty-two slots are occupied");
    struct note_tag *old = tag("old");
    struct note_tag *next = tag("new");
    require(!note_tags_contains(note_metadata_tags(note_text_metadata(body)), old) &&
                note_tags_contains(note_metadata_tags(note_text_metadata(body)), next),
            "full-set replacement changes exactly the requested membership");
    note_tag_destroy(old);
    note_tag_destroy(next);
    tag_update_destroy(update);
}

void run_tag_management_tests(void)
{
    transformations();
    non_changes();
    rewrite_limits();
    full_tag_set();
    allocation_sweep();
}
