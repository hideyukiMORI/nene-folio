#ifndef NENEFOLIO_DISCOVERY_SOURCE_H
#define NENEFOLIO_DISCOVERY_SOURCE_H
/* Session observations only. The sole body owner remains note_corpus. */
enum discovery_source : unsigned char
{
    DISCOVERY_SOURCE_UNREAD,
    DISCOVERY_SOURCE_READY,
    DISCOVERY_SOURCE_UNREADABLE,
    DISCOVERY_SOURCE_UNSUPPORTED
};
#endif
