#ifndef NENEFOLIO_TAG_MANAGEMENT_SUMMARY_H
#define NENEFOLIO_TAG_MANAGEMENT_SUMMARY_H
#include "folio_state_outcome.h"
#include "tag_management_phase.h"
#include <stddef.h>
struct tag_management_summary
{
    enum tag_management_phase phase;
    enum folio_state_outcome notice;
    size_t total;
    size_t prepared;
    size_t pending;
    size_t changed;
    size_t skipped;
    size_t failed;
    bool merging;
};
#endif
