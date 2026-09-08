/* folio_window の生成結果（C-005）。 */
#ifndef NENEFOLIO_FOLIO_WINDOW_OUTCOME_H
#define NENEFOLIO_FOLIO_WINDOW_OUTCOME_H

enum folio_window_outcome : unsigned char
{
    FOLIO_WINDOW_CREATED,
    FOLIO_WINDOW_NOT_CREATED, /* クラス登録・CreateWindowExW・ドロワーの生成が失敗した */
    FOLIO_WINDOW_OUT_OF_MEMORY
};

#endif
