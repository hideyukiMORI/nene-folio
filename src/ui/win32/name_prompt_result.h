/* The dialog outcome and permission to replace the untitled document are separate. */
#ifndef NENEFOLIO_NAME_PROMPT_RESULT_H
#define NENEFOLIO_NAME_PROMPT_RESULT_H
#include "folio_state_outcome.h"
#include "folio_untitled_policy.h"
struct name_prompt_result
{
    enum folio_state_outcome outcome;
    enum folio_untitled_policy untitled;
};
#endif
