/* ASCII の英字だけ大小を無視する畳み込み（ADR 0032 の決定 8）。
 * 多バイト列のバイトは 0x80 以上なので触らず、UTF-8 のバイト比較にも UTF-16 の単位比較にも
 * そのまま使える。索引の絞り込み（index_filter）・ノート内検索（note_search）・
 * パレットの部分一致（folio_command）が引く唯一の規則である（ARC-001）。
 * Ex の完全一致（folio_command_parse）は大小を区別したままで、ここは通らない。 */
#ifndef NENEFOLIO_ASCII_FOLD_H
#define NENEFOLIO_ASCII_FOLD_H

#include <uchar.h>

[[nodiscard]] char ascii_fold_byte(char value);
[[nodiscard]] char16_t ascii_fold_unit(char16_t unit);

#endif
