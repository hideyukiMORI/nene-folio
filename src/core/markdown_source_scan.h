#ifndef NENEFOLIO_MARKDOWN_SOURCE_SCAN_H
#define NENEFOLIO_MARKDOWN_SOURCE_SCAN_H
#include <stddef.h>
/* #1008: what counting a finished RTF's paragraph marks has seen so far. */
struct markdown_source_scan
{
    const char *_Nonnull rtf;
    size_t length;
    size_t entry; /* the entry whose RTF the scan is in, or none before the first */
    size_t next;  /* the next entry to enter */
    size_t marks;
    size_t orphans;  /* marks before the first entry */
    bool last_par;   /* the last mark was a \par */
    bool text_after; /* visible text came after the last mark */
};
#endif
