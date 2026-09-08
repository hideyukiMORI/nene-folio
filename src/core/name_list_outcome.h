/* name_list の追加結果（C-005）。 */
#ifndef NENEFOLIO_NAME_LIST_OUTCOME_H
#define NENEFOLIO_NAME_LIST_OUTCOME_H

enum name_list_outcome : unsigned char
{
    NAME_LIST_ACCEPTED,
    NAME_LIST_DUPLICATE,
    NAME_LIST_INVALID_NAME,
    NAME_LIST_OUT_OF_MEMORY
};

#endif
