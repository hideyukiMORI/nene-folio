/* #1071 / ADR0118: 起動の引数にある「開く」依頼のリンクと、動いている主窓への受け渡し。
 * 入口（src/app）が使う。引数の文字列は入口が明示的に渡す（ここはプロセスの状態を読まない）。 */
#ifndef NENEFOLIO_FOLIO_LAUNCH_LINK_H
#define NENEFOLIO_FOLIO_LAUNCH_LINK_H
#include <windows.h>
struct storage_path;
struct utf8_text;
/* 引数が `nenefolio:` で始まるリンク 1 つなら、UTF-8 にして返す（前後の空白と、囲む二重引用符は
 * 落とす）。リンクでなければ nullptr（いままでどおり、引数は使わない）。中身の検証はしない: それは
 * 主窓が open_request で行う。記憶不足でも nullptr。 */
[[nodiscard]] struct utf8_text *_Nullable folio_launch_link_read(const wchar_t *_Nonnull arguments);
/* place を data として動いている主窓へ、リンクを渡す（folio_open_link.h の約束）。受けた窓が
 * あれば true。無ければ false で、呼び出し側はふつうに起動する。応えない窓は待たずに飛ばす。 */
[[nodiscard]] bool folio_launch_link_hand_over(const struct storage_path *_Nonnull place,
                                               const char *_Nonnull link, size_t length);
#endif
