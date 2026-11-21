#ifndef NENEFOLIO_IMAGE_ADOPT_OUTCOME_H
#define NENEFOLIO_IMAGE_ADOPT_OUTCOME_H
/* #1062 / ADR0120: 落とされたファイル 1 つを、ノートのカテゴリの images/ へ取り込んだ結果。 */
enum image_adopt_outcome : unsigned char
{
    IMAGE_ADOPT_COPIED,  /* images/ へ写した */
    IMAGE_ADOPT_PRESENT, /* 写していない: 同じ中身がもうある、または images/ の中のファイルだった */
    /* PNG でも JPEG でもない（中身で確かめる）。空のファイル・フォルダ・リンクそのものも */
    IMAGE_ADOPT_NOT_IMAGE,
    IMAGE_ADOPT_TOO_LARGE,  /* 1 枚の上限（image_input_byte_limit）を超える */
    IMAGE_ADOPT_UNREADABLE, /* 開けない・読み切れない */
    IMAGE_ADOPT_NOT_STORED, /* images/ を作れない・書けない・空いている名前が無い */
    IMAGE_ADOPT_OUT_OF_MEMORY
};
#endif
