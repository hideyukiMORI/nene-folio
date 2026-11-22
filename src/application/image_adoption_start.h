#ifndef NENEFOLIO_IMAGE_ADOPTION_START_H
#define NENEFOLIO_IMAGE_ADOPTION_START_H
/* #1062 / ADR0120: 落とされたファイルの取り込みを始めようとした結果。断る理由は 1 つずつ別の値で、
 * 窓はそれぞれに「次に何をすればよいか」の 1 行を出す。 */
enum image_adoption_start : unsigned char
{
    IMAGE_ADOPTION_STARTED,
    IMAGE_ADOPTION_NOTHING,  /* ファイルが 1 つも無い */
    IMAGE_ADOPTION_VIEWING,  /* 閲覧表示（決定 1: 受けるのは編集表示の本文だけ） */
    IMAGE_ADOPTION_UNTITLED, /* 無題のノート（決定 5） */
    IMAGE_ADOPTION_NO_NOTE,  /* 開いているノートが無い・カテゴリの改名の途中 */
    IMAGE_ADOPTION_BUSY,     /* 前の取り込みがまだ閉じられていない */
    IMAGE_ADOPTION_OUT_OF_MEMORY
};
#endif
