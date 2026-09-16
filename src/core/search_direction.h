/* 検索の向き（ADR 0023 の決定 2）。閉じた選択肢なので専用の型で表す（C-002）。
 * 「直前の方向」を覚えるのは application で、判断は note_search が持つ。 */
#ifndef NENEFOLIO_SEARCH_DIRECTION_H
#define NENEFOLIO_SEARCH_DIRECTION_H

enum search_direction : unsigned char
{
    SEARCH_DIRECTION_FORWARD, /* `/`・「次へ」。anchor の終わり以降を探す */
    SEARCH_DIRECTION_BACKWARD /* `?`・「前へ」。anchor の始まりより前を探す */
};

#endif
