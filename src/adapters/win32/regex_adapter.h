/* 正規表現ポートの Win32 実装（FR-023 / ADR 0028 の決定 1）。Windows 同梱の ICU
 * （`icu.dll` / `icu.lib` / `icu.h`。Windows 10 1903 以降）の `uregex_*` を呼ぶ唯一の区画。
 * ICU は core / application からは呼べない（eng/symbols.py が未定義シンボルで落とす）。 */
#ifndef NENEFOLIO_REGEX_ADAPTER_H
#define NENEFOLIO_REGEX_ADAPTER_H

#include "regex_adapter_outcome.h"
#include "regex_port.h"

[[nodiscard]] enum regex_adapter_outcome
regex_adapter_create(struct regex_adapter *_Nullable *_Nonnull out);
/* adapter が生きている間だけ有効なポート。 */
[[nodiscard]] struct regex_port regex_adapter_port(struct regex_adapter *_Nonnull adapter);
void regex_adapter_destroy(struct regex_adapter *_Nullable adapter);

#endif
