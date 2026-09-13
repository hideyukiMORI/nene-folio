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
void run_markdown_tests(void);
void run_command_tests(void);
void run_note_name_tests(void);

/* state_tests のポート実装（偽のアダプタ）。台帳の文書を返し、書き戻しは受け入れる。
 * folio_state はポートの adapter を借りるので、state より長く生かしてから test_adapter_destroy
 * する。 */
struct persistence_adapter;
struct persistence_port;
[[nodiscard]] struct persistence_adapter *_Nonnull test_adapter_create(
    const char *_Nonnull categories_text, const char *_Nonnull notes_text);
[[nodiscard]] struct persistence_port
test_adapter_port(struct persistence_adapter *_Nonnull adapter);
/* 名前の付いたカテゴリだけ別の索引と md を持たせる（別カテゴリへの移動を通すため）。
 * scanned は nullptr で終わる名前の並びで、adapter より長く生きること。 */
void test_adapter_second_notes(struct persistence_adapter *_Nonnull adapter,
                               const char *_Nonnull category, const char *_Nonnull notes_text,
                               const char *_Nonnull const *_Nonnull scanned);
void test_adapter_destroy(struct persistence_adapter *_Nullable adapter);
/* 常にダークを答える外観ポート。 */
struct appearance_port;
[[nodiscard]] struct appearance_port test_appearance_port(void);

#endif
