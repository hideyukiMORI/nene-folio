#include "note_file_times.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct note_file_times
{
    char created[17];
    char updated[17];
};

static bool timestamp_shape(const char *_Nonnull text)
{
    if (strlen(text) != 16)
        return false;
    static const char shape[] = "0000-00-00 00:00";
    for (size_t at = 0; at < 16; ++at)
    {
        bool matches =
            shape[at] == '0' ? text[at] >= '0' && text[at] <= '9' : text[at] == shape[at];
        if (!matches)
            return false;
    }
    return true;
}

static unsigned int pair(const char *_Nonnull text)
{
    return (unsigned int)(text[0] - '0') * 10 + (unsigned int)(text[1] - '0');
}

static bool valid_timestamp(const char *_Nonnull text)
{
    if (!timestamp_shape(text))
        return false;
    unsigned int year = pair(text) * 100 + pair(text + 2);
    unsigned int month = pair(text + 5);
    unsigned int day = pair(text + 8);
    if (year < 1601 || month < 1 || month > 12 || pair(text + 11) > 23 || pair(text + 14) > 59)
        return false;
    static const unsigned int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    unsigned int limit = days[month - 1] + (month == 2 && leap ? 1U : 0U);
    return day >= 1 && day <= limit;
}

enum note_file_times_outcome note_file_times_create(const char *_Nonnull created,
                                                    const char *_Nonnull updated,
                                                    struct note_file_times *_Nullable *_Nonnull out)
{
    if (!valid_timestamp(created) || !valid_timestamp(updated))
        return NOTE_FILE_TIMES_INVALID;
    struct note_file_times *_Nullable times = malloc(sizeof *times);
    if (times == nullptr)
        return NOTE_FILE_TIMES_OUT_OF_MEMORY;
    memcpy(times->created, created, sizeof times->created);
    memcpy(times->updated, updated, sizeof times->updated);
    *out = times;
    return NOTE_FILE_TIMES_ACCEPTED;
}

const char *_Nonnull note_file_times_created(const struct note_file_times *_Nonnull times)
{
    return times->created;
}

const char *_Nonnull note_file_times_updated(const struct note_file_times *_Nonnull times)
{
    return times->updated;
}

void note_file_times_destroy(struct note_file_times *_Nullable times)
{
    free(times);
}
