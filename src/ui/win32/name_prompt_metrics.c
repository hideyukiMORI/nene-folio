#include "name_prompt_metrics.h"

#include "folio_state.h"
#include "folio_state_outcome.h"
#include "ui_text.h"
#include "utf16_text.h"

#include <stdlib.h>
#include <string.h>

constexpr size_t pending_reason_capacity = 512;
constexpr int tracking_unavailable = (int)0x80000000U;

struct name_prompt_metrics
{
    bool valid;
    int width;
    UINT dpi;
    enum folio_language language;
    int tracking;
    LOGFONTW font;
    int maximum_height;
};

static enum name_prompt_metrics_outcome
wrapped_height(HDC _Nonnull device, const char *_Nonnull text, int width, int *_Nonnull height)
{
    if (width <= 0)
        return NAME_PROMPT_METRICS_FAILED;
    if (text[0] == '\0')
    {
        *height = 0;
        return NAME_PROMPT_METRICS_READY;
    }
    char16_t units[pending_reason_capacity];
    size_t written = 0;
    if (utf16_text_fill(text, units, pending_reason_capacity, &written) != UTF16_TEXT_FILL_READY)
        return NAME_PROMPT_METRICS_FAILED;
    RECT bounds = {0, 0, width, 0};
    int drawn = DrawTextW(device, units, -1, &bounds,
                          DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX | DT_EXPANDTABS);
    if (drawn <= 0 || bounds.bottom <= 0)
        return NAME_PROMPT_METRICS_FAILED;
    *height = bounds.bottom;
    return NAME_PROMPT_METRICS_READY;
}

static bool font_signature(HFONT _Nonnull font, LOGFONTW *_Nonnull signature)
{
    memset(signature, 0, sizeof *signature);
    return GetObjectW(font, (int)sizeof *signature, signature) == (int)sizeof *signature;
}

static bool matches(const struct name_prompt_metrics *_Nonnull metrics,
                    const struct name_prompt_metrics_request *_Nonnull request, int tracking,
                    const LOGFONTW *_Nonnull font)
{
    return metrics->valid && metrics->width == request->width && metrics->dpi == request->dpi &&
           metrics->language == request->language && metrics->tracking == tracking &&
           memcmp(&metrics->font, font, sizeof *font) == 0;
}

static enum name_prompt_metrics_outcome
measured_maximum(HDC _Nonnull device, const struct name_prompt_metrics_request *_Nonnull request,
                 int *_Nonnull maximum_height)
{
    int maximum = 0;
    for (size_t at = 0; at <= FOLIO_STATE_CATEGORY_NOT_SELECTED; ++at)
    {
        const char *line =
            folio_state_failure_line((enum folio_state_outcome)at, request->language);
        int height = 0;
        if (wrapped_height(device, line, request->width, &height) != NAME_PROMPT_METRICS_READY)
            return NAME_PROMPT_METRICS_FAILED;
        maximum = height > maximum ? height : maximum;
    }
    if (maximum <= 0)
        return NAME_PROMPT_METRICS_FAILED;
    *maximum_height = maximum;
    return NAME_PROMPT_METRICS_READY;
}

enum name_prompt_metrics_create_outcome
name_prompt_metrics_create(struct name_prompt_metrics *_Nullable *_Nonnull out)
{
    *out = calloc(1, sizeof(struct name_prompt_metrics));
    return *out != nullptr ? NAME_PROMPT_METRICS_CREATED : NAME_PROMPT_METRICS_NO_MEMORY;
}

void name_prompt_metrics_destroy(struct name_prompt_metrics *_Nullable metrics)
{
    free(metrics);
}

void name_prompt_metrics_invalidate(struct name_prompt_metrics *_Nonnull metrics)
{
    metrics->valid = false;
}

enum name_prompt_metrics_outcome
name_prompt_metrics_failure_height(struct name_prompt_metrics *_Nonnull metrics,
                                   const struct name_prompt_metrics_request *_Nonnull request,
                                   int *_Nonnull height)
{
    LOGFONTW signature;
    if (request->width <= 0 || request->dpi == 0 || !font_signature(request->font, &signature))
    {
        name_prompt_metrics_invalidate(metrics);
        return NAME_PROMPT_METRICS_FAILED;
    }
    HDC device = GetDC(request->dialog);
    if (device == nullptr)
    {
        name_prompt_metrics_invalidate(metrics);
        return NAME_PROMPT_METRICS_FAILED;
    }
    int tracking = GetTextCharacterExtra(device);
    if (tracking == tracking_unavailable)
    {
        ReleaseDC(request->dialog, device);
        name_prompt_metrics_invalidate(metrics);
        return NAME_PROMPT_METRICS_FAILED;
    }
    if (matches(metrics, request, tracking, &signature))
    {
        ReleaseDC(request->dialog, device);
        *height = metrics->maximum_height;
        return NAME_PROMPT_METRICS_READY;
    }
    HGDIOBJ previous = SelectObject(device, request->font);
    if (previous == nullptr || previous == HGDI_ERROR)
    {
        ReleaseDC(request->dialog, device);
        name_prompt_metrics_invalidate(metrics);
        return NAME_PROMPT_METRICS_FAILED;
    }
    int maximum = 0;
    enum name_prompt_metrics_outcome measured = measured_maximum(device, request, &maximum);
    SelectObject(device, previous);
    ReleaseDC(request->dialog, device);
    if (measured != NAME_PROMPT_METRICS_READY)
    {
        name_prompt_metrics_invalidate(metrics);
        return NAME_PROMPT_METRICS_FAILED;
    }
    *metrics = (struct name_prompt_metrics){.valid = true,
                                            .width = request->width,
                                            .dpi = request->dpi,
                                            .language = request->language,
                                            .tracking = tracking,
                                            .font = signature,
                                            .maximum_height = maximum};
    *height = maximum;
    return NAME_PROMPT_METRICS_READY;
}
