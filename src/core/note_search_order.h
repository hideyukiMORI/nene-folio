/* Two-Way の最大 suffix を選ぶ辞書順。利用者の検索方向とは別の閉じた選択肢。 */
#ifndef NENEFOLIO_NOTE_SEARCH_ORDER_H
#define NENEFOLIO_NOTE_SEARCH_ORDER_H

enum note_search_order : unsigned char
{
    NOTE_SEARCH_ORDER_NORMAL,
    NOTE_SEARCH_ORDER_REVERSED
};

#endif
