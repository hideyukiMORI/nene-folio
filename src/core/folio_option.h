/* `:set` が受ける設定の語（ADR 0026 の決定 8）。効果を実装した語だけが並ぶ。
 * 未知の語は解けないので、この列挙型に「未知」は無い（ARC-010）。 */
#ifndef NENEFOLIO_FOLIO_OPTION_H
#define NENEFOLIO_FOLIO_OPTION_H

enum folio_option : unsigned char
{
    FOLIO_OPTION_NUMBER_SHOW,  /* number / nu */
    FOLIO_OPTION_NUMBER_HIDE,  /* nonumber / nonu */
    FOLIO_OPTION_NUMBER_TOGGLE /* number! / nu! / invnumber / invnu */
};

#endif
