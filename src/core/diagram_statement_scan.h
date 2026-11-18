#ifndef NENEFOLIO_DIAGRAM_STATEMENT_SCAN_H
#define NENEFOLIO_DIAGRAM_STATEMENT_SCAN_H
#include <stddef.h>
struct diagram_statement_scan
{
    size_t start; /* where the statement began */
    size_t cursor;
    size_t brackets;
    char quote;
    bool pipe;
};
#endif
