#include "tag_filter_text.h"
#include "bounded_text.h"
#include "folio_state.h"
#include "note_tags.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include <string.h>

void tag_filter_text_format(const struct folio_state *_Nonnull state, char *_Nonnull out,
                            size_t capacity)
{
    const struct note_tags *_Nullable tags = folio_state_tag_filter(state);
    size_t count = tags == nullptr ? 0 : note_tags_count(tags);
    enum folio_language language = folio_state_language(state);
    if (count == 0)
    {
        (void)bounded_text_copy(out, capacity, ui_text_line(UI_TEXT_TAG_FILTER_EMPTY, language));
        return;
    }
    enum ui_text mode =
        folio_state_tag_match_mode(state) == TAG_MATCH_ALL ? UI_TEXT_TAG_ALL : UI_TEXT_TAG_ANY;
    struct ui_text_request request = {.id = UI_TEXT_TAG_FILTER_COUNT,
                                      .language = language,
                                      .n = count,
                                      .name = ui_text_line(mode, language)};
    if (ui_text_format(&request, out, capacity) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}
