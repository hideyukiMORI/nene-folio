#include "image_adoption_notice.h"
#include "image_adoption.h"

struct image_adoption_notice image_adoption_notice_refused(enum image_adoption_start start)
{
    switch (start)
    {
    case IMAGE_ADOPTION_VIEWING:
        return (struct image_adoption_notice){.id = UI_TEXT_IMAGE_DROP_VIEWING};
    case IMAGE_ADOPTION_UNTITLED:
        return (struct image_adoption_notice){.id = UI_TEXT_IMAGE_DROP_UNTITLED};
    case IMAGE_ADOPTION_NO_NOTE:
        return (struct image_adoption_notice){.id = UI_TEXT_IMAGE_DROP_NO_NOTE};
    case IMAGE_ADOPTION_BUSY:
        return (struct image_adoption_notice){.id = UI_TEXT_IMAGE_DROP_BUSY};
    case IMAGE_ADOPTION_STARTED:
    case IMAGE_ADOPTION_NOTHING:
    case IMAGE_ADOPTION_OUT_OF_MEMORY:
        break;
    }
    return (struct image_adoption_notice){.id = UI_TEXT_EMPTY};
}

struct image_adoption_notice
image_adoption_notice_progress(const struct image_adoption_summary *_Nonnull summary)
{
    size_t handled = summary->dropped - summary->beyond;
    size_t done = handled - summary->pending;
    return (struct image_adoption_notice){
        .id = UI_TEXT_IMAGE_DROP_PROGRESS, .n = handled, .k = done < handled ? done + 1 : handled};
}

/* 取り込まなかった理由の種類。文の表（下の some / none）の添字。 */
constexpr size_t reason_not_image = 0;
constexpr size_t reason_too_large = 1;
constexpr size_t reason_unreadable = 2;
constexpr size_t reason_not_stored = 3;
constexpr size_t reason_beyond = 4;
constexpr size_t reason_mixed = 5;
constexpr size_t reason_count = 6;

/* 理由が 1 種類だけなら、その理由。2 種類以上なら、まとめた文の添字。 */
static size_t reason_of(const struct image_adoption_summary *_Nonnull summary)
{
    /* 書けない・記憶が足りない・扱わずに終えた、は利用者から見て同じ「置けなかった」。 */
    size_t stored = summary->not_stored + summary->out_of_memory + summary->pending;
    size_t kinds = (size_t)(summary->not_image != 0) + (size_t)(summary->too_large != 0) +
                   (size_t)(summary->unreadable != 0) + (size_t)(stored != 0) +
                   (size_t)(summary->beyond != 0);
    if (kinds != 1)
        return reason_mixed;
    if (summary->not_image != 0)
        return reason_not_image;
    if (summary->too_large != 0)
        return reason_too_large;
    if (summary->unreadable != 0)
        return reason_unreadable;
    return stored != 0 ? reason_not_stored : reason_beyond;
}

/* 写したのに本文へ入らなかったときの文。入ったときは UI_TEXT_EMPTY。 */
static enum ui_text unwritten_text(enum image_adoption_landing landing)
{
    switch (landing)
    {
    case IMAGE_ADOPTION_DISPLACED:
        return UI_TEXT_IMAGE_DROP_NOT_WRITTEN;
    case IMAGE_ADOPTION_UNWRITTEN:
        return UI_TEXT_IMAGE_DROP_NOT_INSERTED;
    case IMAGE_ADOPTION_LANDED:
        break;
    }
    return UI_TEXT_EMPTY;
}

struct image_adoption_notice
image_adoption_notice_finished(const struct image_adoption_summary *_Nonnull summary,
                               enum image_adoption_landing landing)
{
    static const enum ui_text some[reason_count] = {
        UI_TEXT_IMAGE_DROP_SOME_NOT_IMAGE,  UI_TEXT_IMAGE_DROP_SOME_TOO_LARGE,
        UI_TEXT_IMAGE_DROP_SOME_UNREADABLE, UI_TEXT_IMAGE_DROP_SOME_NOT_STORED,
        UI_TEXT_IMAGE_DROP_SOME_BEYOND,     UI_TEXT_IMAGE_DROP_SOME_MIXED};
    /* 1 件も取り込まずに上限だけを超えることは無い（上限までは扱う）。念のため、まとめた文。 */
    static const enum ui_text none[reason_count] = {
        UI_TEXT_IMAGE_DROP_NONE_NOT_IMAGE,  UI_TEXT_IMAGE_DROP_NONE_TOO_LARGE,
        UI_TEXT_IMAGE_DROP_NONE_UNREADABLE, UI_TEXT_IMAGE_DROP_NONE_NOT_STORED,
        UI_TEXT_IMAGE_DROP_NONE_MIXED,      UI_TEXT_IMAGE_DROP_NONE_MIXED};
    size_t left = summary->dropped - summary->taken;
    struct image_adoption_notice notice = {.id = UI_TEXT_IMAGE_DROP_TAKEN,
                                           .n = summary->taken,
                                           .k = left,
                                           .offset = image_adoption_limit};
    if (summary->taken != 0 && landing != IMAGE_ADOPTION_LANDED)
        notice.id = unwritten_text(landing);
    else if (summary->taken == 0 && summary->interrupted && landing == IMAGE_ADOPTION_DISPLACED)
        notice.id = UI_TEXT_IMAGE_DROP_STOPPED;
    else if (left != 0)
        notice.id = (summary->taken != 0 ? some : none)[reason_of(summary)];
    return notice;
}
