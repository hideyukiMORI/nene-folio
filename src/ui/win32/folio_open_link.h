/* #1071 / ADR0118: 動いている主窓へ「開く」依頼のリンクを渡す約束。2つ目の起動（入口）が送り、
 * 主窓が受ける。送り手は信用しない: 受けた文字列は、起動の引数と同じ検証をもう一度通る。 */
#ifndef NENEFOLIO_FOLIO_OPEN_LINK_H
#define NENEFOLIO_FOLIO_OPEN_LINK_H
#include <windows.h>
/* 主窓の class。同じ利用者の主窓を探すために入口が使う。 */
static const wchar_t folio_open_link_class[] = L"NeNeFolioWindow";
/* WM_COPYDATA の dwData。lpData は UTF-8 の「data の場所」・NUL・リンク（終端は要らない）。
 * 受け手は、場所が自分の data と同じときだけ受けて TRUE を返す。違えば FALSE で何もしない。 */
constexpr ULONG_PTR folio_open_link_data = 0x4E464F4C;
/* 送れる全体の上限（場所 + NUL + リンク）。 */
constexpr size_t folio_open_link_limit = 8192;
#endif
