#ifndef NENEFOLIO_WINDOW_FIT_OUTCOME_H
#define NENEFOLIO_WINDOW_FIT_OUTCOME_H
enum window_fit_outcome : unsigned char
{
    WINDOW_FIT_RESTORE, /* open at the recorded place */
    WINDOW_FIT_DEFAULT  /* open at the initial place and size */
};
#endif
