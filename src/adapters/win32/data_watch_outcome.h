#ifndef NENEFOLIO_DATA_WATCH_OUTCOME_H
#define NENEFOLIO_DATA_WATCH_OUTCOME_H
enum data_watch_outcome : unsigned char
{
    DATA_WATCH_OPENED,
    /* The place sends no change notices (or its path does not fit): the app runs without them. */
    DATA_WATCH_UNAVAILABLE,
    DATA_WATCH_OUT_OF_MEMORY
};
#endif
