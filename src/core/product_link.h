/* 製品の公式リンク。閉じた集合と、その絶対https URLの唯一の表（ADR0097）。
 * 値を足すときは product_link.c のswitchへ枝を足す（C-002）。開く経路は application の
 * folio_state_open_product_link だけで、UIはURLを組み立てない。 */
#ifndef NENEFOLIO_PRODUCT_LINK_H
#define NENEFOLIO_PRODUCT_LINK_H

#include <stddef.h>

enum product_link : unsigned char
{
    PRODUCT_LINK_REPOSITORY,
    PRODUCT_LINK_RELEASES,
    PRODUCT_LINK_STORE,
    PRODUCT_LINK_ISSUES
};

constexpr size_t product_link_count = PRODUCT_LINK_ISSUES + 1;

/* 終端付きUTF-8。集合の外の値には空文字列を返し、web_uriの検証が拒否する。 */
[[nodiscard]] const char *_Nonnull product_link_uri(enum product_link link);
[[nodiscard]] size_t product_link_uri_length(enum product_link link);

#endif
