/* drawer_window の生成結果（C-005）。 */
#ifndef NENEFOLIO_DRAWER_WINDOW_OUTCOME_H
#define NENEFOLIO_DRAWER_WINDOW_OUTCOME_H

enum drawer_window_outcome : unsigned char
{
    DRAWER_WINDOW_CREATED,
    DRAWER_WINDOW_NOT_CREATED, /* ウィンドウクラスの登録や CreateWindowExW が失敗した */
    DRAWER_WINDOW_OUT_OF_MEMORY
};

#endif
