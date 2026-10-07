/* ドロワーの行の右クリックで出すメニューの項目（FR-038・ADR 0039 の決定 11 / 12）。
 * 値を足すと、ドロワーと主窓の閉じた switch が「その項目で何が起きるか」を決めさせる（C-002）。
 * メニュー ID は値 + 1（0 は「何も選ばれなかった」）。RECOLOR はドロワーが自分で処理し、
 * それ以外は folio_message_drawer_menu で主窓の既存の受け口へ渡す。 */
#ifndef NENEFOLIO_DRAWER_MENU_ITEM_H
#define NENEFOLIO_DRAWER_MENU_ITEM_H

enum drawer_menu_item : unsigned char
{
    DRAWER_MENU_RECOLOR,
    DRAWER_MENU_RENAME_NOTE,
    DRAWER_MENU_NEW_CATEGORY,
    DRAWER_MENU_TRASH_NOTE
};

#endif
