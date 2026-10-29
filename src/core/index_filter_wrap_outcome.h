/* Private completed-owner allocation result; no readiness or term validation occurs here. */
#ifndef NENEFOLIO_INDEX_FILTER_WRAP_OUTCOME_H
#define NENEFOLIO_INDEX_FILTER_WRAP_OUTCOME_H
enum index_filter_wrap_outcome : unsigned char
{
    INDEX_FILTER_WRAP_ACCEPTED,
    INDEX_FILTER_WRAP_OUT_OF_MEMORY
};
#endif
