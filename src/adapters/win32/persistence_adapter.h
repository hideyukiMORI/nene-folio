/* 永続化ポートの Win32 実装（FR-002）。合成ルートから受けた保存先
 * を走査し、台帳を読み書きする（ADR0077）。
 * ファイル・環境・プロセス状態に触れてよい唯一の区画（ARC-007）。UTF-16 のパスはここで組み立て、
 * 名前は core の utf8_text / utf16_text で変換する（C-014）。 */
#ifndef NENEFOLIO_PERSISTENCE_ADAPTER_H
#define NENEFOLIO_PERSISTENCE_ADAPTER_H

#include "persistence_adapter_outcome.h"
#include "persistence_port.h"

struct storage_path;
/* 起動時に決めたrootを受け、寿命中は変えない。 */
[[nodiscard]] enum persistence_adapter_outcome
persistence_adapter_create(const struct storage_path *_Nonnull root,
                           struct persistence_adapter *_Nullable *_Nonnull out);
/* adapter が生きている間だけ有効なポート。 */
[[nodiscard]] struct persistence_port
persistence_adapter_port(struct persistence_adapter *_Nonnull adapter);
void persistence_adapter_destroy(struct persistence_adapter *_Nullable adapter);

#endif
