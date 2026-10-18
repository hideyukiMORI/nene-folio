/* 操作部品の様式（ADR0060 の決定 1）。3 様式＋選択群の項目だけで、ほかの様式は作らない。
 * 値を足したら ui_part.c の閉じた switch に枝を足す（C-002）。 */
#ifndef NENEFOLIO_UI_PART_STYLE_H
#define NENEFOLIO_UI_PART_STYLE_H

enum ui_part_style : unsigned char
{
    UI_PART_STYLE_COMMAND, /* A 中立コマンド: 16px の絵＋字。単独なら 1px の枠 */
    UI_PART_STYLE_PRIMARY, /* B 主操作: chip_background の塗り。1 つの面に最大 1 つ */
    UI_PART_STYLE_ICON,    /* C 絵だけ: 当たり 28×28、絵 20px */
    UI_PART_STYLE_CHOICE   /* S 選択群の項目: 選択中は塗り＋下辺の棒 */
};

#endif
