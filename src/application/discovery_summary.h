#ifndef NENEFOLIO_DISCOVERY_SUMMARY_H
#define NENEFOLIO_DISCOVERY_SUMMARY_H
#include "discovery_mode.h"
#include "discovery_phase.h"
#include "discovery_preview.h"
#include "folio_state_outcome.h"
#include <stddef.h>
#include <stdint.h>
struct discovery_summary
{
    enum discovery_phase phase;
    enum discovery_mode mode;
    enum discovery_preview preview;
    enum folio_state_outcome notice;
    size_t total;
    size_t shown;
    size_t selected;
    size_t unreadable;
    size_t unsupported;
    size_t source_total;
    size_t source_checked;
    uint64_t revision;
    uint64_t results_revision; /* Candidate identities, independent of selected preview. */
};
#endif
