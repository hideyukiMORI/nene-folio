#ifndef NENEFOLIO_DISCOVERY_TRACE_PHASE_H
#define NENEFOLIO_DISCOVERY_TRACE_PHASE_H
/* Private observation of one native idle step; no product state. */
enum discovery_trace_phase : unsigned char
{
    DISCOVERY_TRACE_OFF,
    DISCOVERY_TRACE_IDLE,
    DISCOVERY_TRACE_READ,
    DISCOVERY_TRACE_READ_DONE
};
#endif
