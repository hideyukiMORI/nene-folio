/* Previewだけがmatchesを引き取る境界。heap配列を注入の外で用意する。 */
#include "allocation_probe.h"
#include "regex_matches.h"
#include "replace_preview.h"
#include "unit_tests.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct regex_matches heap_matches(void)
{
    struct regex_match *_Nullable items = calloc(2, sizeof *items);
    if (items == nullptr)
    {
        test_failure("allocate caller-owned preview matches outside injection");
    }
    items[0].whole.end = 1;
    items[1].whole.start = 2;
    items[1].whole.end = 3;
    return (struct regex_matches){.items = items, .capacity = 2, .count = 2};
}

static void verify_transfer(const struct replace_preview *_Nonnull preview,
                            const struct regex_matches *_Nonnull caller,
                            const struct regex_matches *_Nonnull held,
                            const struct replace_source *_Nonnull source)
{
    require(caller->items == held->items && caller->capacity == 0 && caller->count == 0,
            "READY moves the array while retaining the caller's nonnull pointer value");
    struct note_replace_plan plan =
        replace_preview_plan(preview, REPLACE_ALL, (struct note_search_span){0, 0});
    require(plan.matches->items == held->items && plan.matches->capacity == held->capacity &&
                plan.matches->count == held->count && replace_preview_count(preview) == held->count,
            "preview owns the original heap array and original match extent");
    require(replace_preview_holds(preview, source) && plan.text != source->text,
            "preview owns a separate source copy after all allocations succeed");
}

static struct replace_preview *_Nonnull sentinel_preview(
    const struct replace_source *_Nonnull source)
{
    struct regex_matches list = heap_matches();
    struct regex_matches held = list;
    struct replace_preview *sentinel = nullptr;
    require(replace_preview_create(source, &list, &sentinel) == REPLACE_PREVIEW_READY,
            "create a real nonnull output sentinel outside injection");
    verify_transfer(sentinel, &list, &held, source);
    return sentinel;
}

static void verify_caller(const struct regex_matches *_Nonnull actual,
                          const struct regex_matches *_Nonnull held,
                          const struct regex_match *_Nonnull original)
{
    require(actual->items == held->items && actual->capacity == held->capacity &&
                actual->count == held->count &&
                memcmp(actual->items, original, held->count * sizeof *original) == 0,
            "a failed preview preserves caller pointer, capacity, count and all matches");
}

static bool verify_allocation(size_t nth, const struct replace_source *_Nonnull source)
{
    struct regex_matches list = heap_matches();
    struct regex_matches held = list;
    struct regex_match original[2];
    memcpy(original, list.items, sizeof original);
    struct replace_preview *sentinel = sentinel_preview(source);
    struct replace_preview *out = sentinel;
    allocation_probe_fail_at(nth);
    enum replace_preview_outcome result = replace_preview_create(source, &list, &out);
    size_t calls = allocation_probe_calls();
    allocation_probe_fail_at(0);
    require(result == REPLACE_PREVIEW_READY || result == REPLACE_PREVIEW_OUT_OF_MEMORY,
            "a valid preview has only success or typed OOM");
    if (result == REPLACE_PREVIEW_OUT_OF_MEMORY)
    {
        require(out == sentinel && calls >= nth,
                "each injected allocation fails without replacing a nonnull output");
        verify_caller(&list, &held, original);
        require(replace_preview_create(source, &list, &out) == REPLACE_PREVIEW_READY,
                "the caller can retry using exactly the same retained matches after OOM");
    }
    require(out != sentinel && replace_preview_holds(sentinel, source),
            "success or retry does not change the previous owned preview");
    verify_transfer(out, &list, &held, source);
    /* list.itemsは移転済み。配列はpreviewだけが解放し、callerからfreeしない。 */
    replace_preview_destroy(out);
    replace_preview_destroy(sentinel);
    return result == REPLACE_PREVIEW_READY;
}

static void verify_bad_template(const struct replace_source *_Nonnull source)
{
    struct regex_matches list = heap_matches();
    struct regex_matches held = list;
    struct regex_match original[2];
    memcpy(original, list.items, sizeof original);
    struct replace_preview *sentinel = sentinel_preview(source);
    struct replace_preview *out = sentinel;
    struct replace_source malformed = *source;
    malformed.replacement = u"\\q";
    malformed.replacement_length = 2;
    require(replace_preview_create(&malformed, &list, &out) == REPLACE_PREVIEW_BAD_TEMPLATE &&
                out == sentinel,
            "BAD_TEMPLATE keeps the existing nonnull output");
    verify_caller(&list, &held, original);
    free(list.items);
    replace_preview_destroy(sentinel);
}

void run_replace_preview_allocation_tests(void)
{
    const struct replace_source source = {.text = u"a-a",
                                          .length = 3,
                                          .replacement = u"<&>",
                                          .replacement_length = 3,
                                          .category = "category",
                                          .note = ""};
    verify_bad_template(&source);
    for (size_t nth = 1; nth < 64; ++nth)
    {
        if (verify_allocation(nth, &source))
        {
            printf("preview ownership allocation: %zu failed points, then complete\n", nth - 1);
            return;
        }
    }
    require(false, "all preview allocation failures were exhausted");
}
