#ifndef NENEFOLIO_STORAGE_PROMPT_H
#define NENEFOLIO_STORAGE_PROMPT_H
#include "storage_prompt_outcome.h"
#include "storage_prompt_request.h"
#include <windows.h>
struct folio_palette;
[[nodiscard]] enum storage_prompt_outcome
storage_prompt_show(HWND _Nullable owner, const struct storage_prompt_request *_Nonnull request,
                    const struct folio_palette *_Nonnull palette);
#endif
