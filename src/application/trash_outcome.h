/* ごみ箱ポートの結果（ARC-010 / C-005 / ADR 0040）。完全削除への代替は無い。 */
#ifndef NENEFOLIO_TRASH_OUTCOME_H
#define NENEFOLIO_TRASH_OUTCOME_H

enum trash_outcome : unsigned char
{
    TRASH_TRASHED,     /* md と、存在した履歴を OS のごみ箱へ送った */
    TRASH_ABSENT,      /* md は不在。存在した履歴は OS のごみ箱へ送った */
    TRASH_UNAVAILABLE, /* 種類・場所・パス・ごみ箱の非対応。完全削除しない */
    TRASH_BUSY,        /* 事前確認の共有違反。md は動かしていない */
    TRASH_FAILED       /* その他の失敗。履歴だけが先に移った場合もある */
};

#endif
