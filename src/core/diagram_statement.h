#ifndef NENEFOLIO_DIAGRAM_STATEMENT_H
#define NENEFOLIO_DIAGRAM_STATEMENT_H
#include "diagram_input.h"
#include "diagram_outcome.h"
struct diagram_statement
{
    struct diagram_text text;
    size_t next;
};
[[nodiscard]] enum diagram_outcome diagram_statement_read(struct diagram_input input, size_t start,
                                                          struct diagram_statement *_Nonnull out);
#endif
