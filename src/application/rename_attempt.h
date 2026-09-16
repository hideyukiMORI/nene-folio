/* 改名ポートへの頼み方（ADR 0022 の決定 5）。
 * START は新しい意図で、記録が無ければこれから公開する。
 * RESUME は application が保持している意図の続きで、記録が無ければ同一性を確かめられないので
 * 名前だけで移動済みと推測せずに止める。 */
#ifndef NENEFOLIO_RENAME_ATTEMPT_H
#define NENEFOLIO_RENAME_ATTEMPT_H

enum rename_attempt : unsigned char
{
    RENAME_START,
    RENAME_RESUME
};

#endif
