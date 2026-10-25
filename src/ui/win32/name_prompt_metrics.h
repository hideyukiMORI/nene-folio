#ifndef NENEFOLIO_NAME_PROMPT_METRICS_H
#define NENEFOLIO_NAME_PROMPT_METRICS_H

#include "name_prompt_metrics_create_outcome.h"
#include "name_prompt_metrics_outcome.h"
#include "name_prompt_metrics_request.h"
#include <windows.h>

struct name_prompt_metrics;

[[nodiscard]] enum name_prompt_metrics_create_outcome
name_prompt_metrics_create(struct name_prompt_metrics *_Nullable *_Nonnull out);
void name_prompt_metrics_destroy(struct name_prompt_metrics *_Nullable metrics);
void name_prompt_metrics_invalidate(struct name_prompt_metrics *_Nonnull metrics);
[[nodiscard]] enum name_prompt_metrics_outcome
name_prompt_metrics_failure_height(struct name_prompt_metrics *_Nonnull metrics,
                                   const struct name_prompt_metrics_request *_Nonnull request,
                                   int *_Nonnull height);

#endif
