#ifndef NENEFOLIO_DATA_WATCH_WAKE_H
#define NENEFOLIO_DATA_WATCH_WAKE_H
/* Why the message loop's wait ended. */
enum data_watch_wake : unsigned char
{
    DATA_WATCH_MESSAGE, /* input or a message is waiting */
    DATA_WATCH_CHANGED, /* something under the data folder changed */
    DATA_WATCH_STOPPED  /* the wait itself failed: the loop ends, as with WaitMessage */
};
#endif
