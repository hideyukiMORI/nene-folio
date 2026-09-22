/* 失敗の箱の内部の結果（C-005）。
 * 自前のモーダルで見せたか、作れずに退避したか（ADR 0035 の補正 3）。 */
#ifndef NENEFOLIO_FAILURE_BOX_OUTCOME_H
#define NENEFOLIO_FAILURE_BOX_OUTCOME_H

enum failure_box_outcome : unsigned char
{
    FAILURE_BOX_SHOWN,
    FAILURE_BOX_FALLBACK /* 塗り・書体・DC・子窓・面のどれかを作れず、MessageBoxW で知らせる */
};

#endif
