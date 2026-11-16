/* クリップボードのポートの Win32 実装（ADR0098）。OS のクリップボードを書く唯一の区画。
 * 文字列は CF_UNICODETEXT で置き、行の区切りは CRLF にする。所有者の窓は adapter が持つ
 * 表示しない窓で、最初の書き込みのときに、呼び出したスレッド（UI スレッド）で作る。 */
#ifndef NENEFOLIO_CLIPBOARD_ADAPTER_H
#define NENEFOLIO_CLIPBOARD_ADAPTER_H

#include "clipboard_adapter_outcome.h"
#include "clipboard_port.h"

[[nodiscard]] enum clipboard_adapter_outcome
clipboard_adapter_create(struct clipboard_adapter *_Nullable *_Nonnull out);
/* adapter が生きている間だけ有効なポート。作ったスレッドから呼ぶ。 */
[[nodiscard]] struct clipboard_port
clipboard_adapter_port(struct clipboard_adapter *_Nonnull adapter);
void clipboard_adapter_destroy(struct clipboard_adapter *_Nullable adapter);

#endif
