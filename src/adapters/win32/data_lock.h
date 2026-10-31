/* 同じrootの錠はpersistenceと保存先検証がこの一経路で取得する（ADR0077）。 */
#ifndef NENEFOLIO_DATA_LOCK_H
#define NENEFOLIO_DATA_LOCK_H
#include "data_lock_outcome.h"
#include <stddef.h>
struct data_lock;
[[nodiscard]] enum data_lock_outcome data_lock_open(const wchar_t *_Nonnull root,
                                                    struct data_lock *_Nullable *_Nonnull out);
void data_lock_destroy(struct data_lock *_Nullable lock);
#endif
