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

/* state_tests のポート実装（偽のアダプタ）。台帳の文書を返し、書き戻しは受け入れる。
 * folio_state はポートの adapter を借りるので、state より長く生かしてから test_adapter_destroy
 * する。 */
struct persistence_adapter;
struct persistence_port;
[[nodiscard]] struct persistence_adapter *_Nonnull test_adapter_create(
    const char *_Nonnull categories_text, const char *_Nonnull notes_text);
[[nodiscard]] struct persistence_port
test_adapter_port(struct persistence_adapter *_Nonnull adapter);
void test_adapter_destroy(struct persistence_adapter *_Nullable adapter);

#endif
