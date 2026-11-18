#ifndef NENEFOLIO_MERMAID_PIE_SLICE_H
#define NENEFOLIO_MERMAID_PIE_SLICE_H
#include "diagram_text.h"
#include <stdint.h>
/* #955: one slice as it was read, before the model is built: the name and the number borrow the
 * source; the value is the number in thousandths. */
struct mermaid_pie_slice
{
    struct diagram_text name;
    struct diagram_text number;
    uint64_t value;
};
#endif
