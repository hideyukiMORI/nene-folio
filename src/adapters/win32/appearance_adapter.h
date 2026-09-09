/* 外観ポートの Win32 実装（FR-016）。OS の「アプリのモード」をレジストリから読む。
 * 環境を読んでよい唯一の区画（ARC-007）。 */
#ifndef NENEFOLIO_APPEARANCE_ADAPTER_H
#define NENEFOLIO_APPEARANCE_ADAPTER_H

#include "appearance_adapter_outcome.h"
#include "appearance_port.h"

[[nodiscard]] enum appearance_adapter_outcome
appearance_adapter_create(struct appearance_adapter *_Nullable *_Nonnull out);
/* adapter が生きている間だけ有効なポート。 */
[[nodiscard]] struct appearance_port
appearance_adapter_port(struct appearance_adapter *_Nonnull adapter);
void appearance_adapter_destroy(struct appearance_adapter *_Nullable adapter);

#endif
