/* 固定帯で押しているボタン。行のdragとは別に一つだけ覚える（ADR0051）。 */
#ifndef NENEFOLIO_DRAWER_HEADER_BUTTON_H
#define NENEFOLIO_DRAWER_HEADER_BUTTON_H

enum drawer_header_button : unsigned char
{
    DRAWER_HEADER_NONE,
    DRAWER_HEADER_NEW_NOTE, /* 検索の欄の右の印（#1291） */
    DRAWER_HEADER_FAVORITES,
    DRAWER_HEADER_TAG,  /* タグごとの札。どの札かは番号で持つ（#1247） */
    DRAWER_HEADER_MORE, /* 畳んだタグの「+N」 */
    DRAWER_HEADER_TAG_FILTER,
    DRAWER_HEADER_MATCH_ALL, /* タグの条件の 2 択（#1249） */
    DRAWER_HEADER_MATCH_ANY,
    DRAWER_HEADER_SCOPE_ALL, /* 検索の範囲の 2 択（#1292） */
    DRAWER_HEADER_SCOPE_FILTERED,
    DRAWER_HEADER_CLEAR,
    DRAWER_HEADER_NEW_CATEGORY
};

#endif
