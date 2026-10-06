/* category_name の生成の結果（C-005 / ARC-010）。値の名前と数は note_name_outcome に揃える。 */
#ifndef NENEFOLIO_CATEGORY_NAME_OUTCOME_H
#define NENEFOLIO_CATEGORY_NAME_OUTCOME_H
enum category_name_outcome : unsigned char
{
    CATEGORY_NAME_ACCEPTED,
    CATEGORY_NAME_INVALID,
    CATEGORY_NAME_OUT_OF_MEMORY
};
#endif
