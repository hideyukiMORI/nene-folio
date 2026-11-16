#ifndef NENEFOLIO_CLIPBOARD_OUTCOME_H
#define NENEFOLIO_CLIPBOARD_OUTCOME_H
enum clipboard_outcome : unsigned char
{
    CLIPBOARD_WRITTEN,
    CLIPBOARD_BUSY,        /* ほかのプログラムが開いたままで、クリップボードは変えていない */
    CLIPBOARD_UNAVAILABLE, /* 開けたが書けなかった・渡した文字列が UTF-8 でない */
    CLIPBOARD_OUT_OF_MEMORY
};
#endif
