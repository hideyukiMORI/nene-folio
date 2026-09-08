/* OS 非依存の中核（core / application）の単体テスト。ASan / UBSan / nullability
 * 付きで走る（C-016）。 */
#ifndef NENEFOLIO_UNIT_TESTS_H
#define NENEFOLIO_UNIT_TESTS_H

#include <stddef.h>

/* 条件が偽なら説明を stderr に出して終了コード 1 で止める。 */
void require(bool condition, const char *_Nonnull description);
/* 2 つの終端付き文字列が同じか。 */
[[nodiscard]] bool same_text(const char *_Nonnull actual, const char *_Nonnull expected);

void run_text_tests(void);
void run_json_tests(void);
void run_ledger_tests(void);
void run_layout_tests(void);
void run_state_tests(void);
void run_allocation_tests(void);

/* state_tests のポート実装で、台帳の文書から folio_state を作る。記憶不足はそのまま返す。 */
struct folio_state;
enum folio_state_outcome : unsigned char;
[[nodiscard]] enum folio_state_outcome
state_from_texts(const char *_Nonnull categories_text, const char *_Nonnull notes_text,
                 struct folio_state *_Nullable *_Nonnull out);

#endif
