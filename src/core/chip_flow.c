#include "chip_flow.h"

/* 札を 1 つ置く。行に入らなければ次の行の左端へ送る。行より広い札は行の幅に切るので、
 * 空の行（left が 0）では必ず入る。
 * x は、いまの行で最後に置いた札の右端（行が空なら -gap）。row はいまの行。 */
static struct chip_flow_place put(const struct chip_flow_input *_Nonnull input, int *_Nonnull x,
                                  int *_Nonnull row, int width)
{
    int room = input->available > 0 ? input->available : 0;
    int fitted = width < room ? width : room;
    int left = *x + input->gap;
    if (left + fitted > room)
    {
        *row += 1;
        left = 0;
    }
    *x = left + fitted;
    return (struct chip_flow_place){
        .left = left, .top = *row * (input->height + input->row_gap), .width = fitted};
}

/* 先頭の shown 個を置き、畳んだ分があれば「+N」、最後に「開く札」を置いたときの結果。 */
static struct chip_flow_result attempt(const struct chip_flow_input *_Nonnull input, size_t shown,
                                       struct chip_flow_place *_Nonnull places)
{
    int x = -input->gap;
    int row = 0;
    for (size_t index = 0; index < shown; ++index)
    {
        places[index] = put(input, &x, &row, input->widths[index]);
    }
    struct chip_flow_result result = {.shown = shown, .hidden = input->count - shown};
    if (result.hidden > 0)
    {
        result.more = put(input, &x, &row, input->more_width);
    }
    result.opener = put(input, &x, &row, input->opener_width);
    result.rows = row + 1;
    return result;
}

struct chip_flow_result chip_flow_arrange(const struct chip_flow_input *_Nonnull input,
                                          struct chip_flow_place *_Nonnull places)
{
    for (size_t shown = input->count; shown > 1; --shown)
    {
        struct chip_flow_result result = attempt(input, shown, places);
        if (result.rows <= input->rows)
        {
            return result;
        }
    }
    return attempt(input, input->count == 0 ? 0 : 1, places);
}
