/* クリップボードへ文字列を置くポート（ADR0098）。OS のクリップボードを書くのは adapters/win32
 * だけ。 bytes は終端に頼らない UTF-8 で、行の区切りは LF。OS の形（UTF-16・CRLF）への変換は
 * adapter が行う。 */
#ifndef NENEFOLIO_CLIPBOARD_PORT_H
#define NENEFOLIO_CLIPBOARD_PORT_H
#include "clipboard_outcome.h"
#include <stddef.h>
struct clipboard_adapter;
struct clipboard_port
{
    struct clipboard_adapter *_Nonnull adapter;
    enum clipboard_outcome (*_Nonnull write)(struct clipboard_adapter *_Nonnull adapter,
                                             const char *_Nonnull bytes, size_t length);
};
#endif
