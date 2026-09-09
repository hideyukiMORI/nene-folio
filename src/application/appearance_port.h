/* 外観ポート（ARC-003 / ARC-007）。OS の「アプリのモード」を読む唯一の窓口。実装は adapters/win32
 * の appearance_adapter だけで、application はその型を不完全型としてしか知らない（C-006）。 */
#ifndef NENEFOLIO_APPEARANCE_PORT_H
#define NENEFOLIO_APPEARANCE_PORT_H

#include "folio_theme.h"

struct appearance_adapter;

struct appearance_port
{
    struct appearance_adapter *_Nonnull adapter;
    /* 読めなければアダプタが LIGHT を返す（OS の既定と同じ）。 */
    enum folio_theme (*_Nonnull read_theme)(struct appearance_adapter *_Nonnull adapter);
};

#endif
