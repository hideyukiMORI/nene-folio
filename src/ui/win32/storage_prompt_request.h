#ifndef NENEFOLIO_STORAGE_PROMPT_REQUEST_H
#define NENEFOLIO_STORAGE_PROMPT_REQUEST_H
#include "folio_language.h"
struct storage_state;
struct folio_state;
struct storage_prompt_request
{
    struct storage_state *_Nonnull storage;
    /* 初回起動にはまだノート側stateが無い。 */
    const struct folio_state *_Nullable folio;
    enum folio_language language;
};
#endif
