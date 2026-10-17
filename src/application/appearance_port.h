/* 外観ポート（ARC-003 / ARC-007）。OS の「アプリのモード」を読む唯一の窓口。実装は adapters/win32
 * の appearance_adapter だけで、application はその型を不完全型としてしか知らない（C-006）。 */
#ifndef NENEFOLIO_APPEARANCE_PORT_H
#define NENEFOLIO_APPEARANCE_PORT_H

#include "appearance_outcome.h"
#include "appearance_snapshot.h"

struct appearance_adapter;

struct appearance_port
{
    struct appearance_adapter *_Nonnull adapter;
    /* UNAVAILABLEではoutを変更しない。失敗と通常表示を区別する（ADR0057）。 */
    enum appearance_outcome (*_Nonnull read)(struct appearance_adapter *_Nonnull adapter,
                                             struct appearance_snapshot *_Nonnull out);
};

#endif
