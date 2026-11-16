/* 閲覧のコードブロックのコピー操作へ、描画時に重ねる自前のボタン（ADR0098 第3版）。
 * 文書内の札（リンクの区間）が鍵・読み上げ・検索・クリックの本体で、ここは見た目だけを足す。
 * 子窓を作らず、位置も保存しない: 描くたびに note_pane_links の派生した文字位置から RichEdit に
 * 矩形を尋ねる。持つ状態は、ポインタの乗っている札・押している札・コピー済みを示している札と
 * その戻しのタイマーだけ。飾りが出なくても、下の札はそのまま使える。 */
#ifndef NENEFOLIO_NOTE_PANE_CODE_BUTTON_H
#define NENEFOLIO_NOTE_PANE_CODE_BUTTON_H

#include "note_pane_outcome.h"
#include <uchar.h>
#include <windows.h>

#include <richedit.h>
#include <tom.h>

struct note_pane_code_button;
struct note_pane_links;
struct theme_palette;

/* 済みの表示を戻すタイマーの番号（本文の窓のタイマー。RichEdit へは渡さない）。 */
constexpr UINT_PTR note_pane_code_button_timer_id = 0x4E46;

[[nodiscard]] enum note_pane_outcome
note_pane_code_button_create(HWND _Nonnull window,
                             struct note_pane_code_button *_Nullable *_Nonnull out);
void note_pane_code_button_destroy(struct note_pane_code_button *_Nullable button);
/* 配色は既存の役割だけを使う。高コントラストでは飾らない。 */
void note_pane_code_button_palette(struct note_pane_code_button *_Nonnull button,
                                   const struct theme_palette *_Nonnull theme);
/* 本文の表示倍率（百分率）。寸法と字の大きさへ掛ける。 */
void note_pane_code_button_zoom(struct note_pane_code_button *_Nonnull button, int zoom_percent);
/* 文書の投入後に、札の位置の派生値と書体・文言を借りる。links が無ければ（編集）飾らない。
 * links は次の attach まで生きていること。 */
void note_pane_code_button_attach(struct note_pane_code_button *_Nonnull button,
                                  const struct note_pane_links *_Nullable links,
                                  ITextDocument *_Nullable document);
/* WM_PAINT の前に呼ぶ。見えている札の矩形を更新領域から外し、下の札を描かせない。 */
void note_pane_code_button_exclude(const struct note_pane_code_button *_Nonnull button);
/* 見えている札へボタンを描く。device が nullptr なら窓の DC へ直接描く。 */
void note_pane_code_button_paint(struct note_pane_code_button *_Nonnull button,
                                 HDC _Nullable device);
/* 左ボタンの押下を RichEdit へ渡す前に呼ぶ。札の上なら、そのときの選択とスクロール位置を
 * 預かる。同じ札の上で離したとき（クリックでのコピー）と取消で元へ戻し、ボタンの中に
 * キャレットを残さない。札から外へ引いた選択と、Ctrl+Enter のキャレットには触らない。 */
void note_pane_code_button_press(struct note_pane_code_button *_Nonnull button, LPARAM position);
/* RichEdit が処理した後の入力。乗る・押す・キャレットの状態を採り直して描き直す。 */
void note_pane_code_button_input(struct note_pane_code_button *_Nonnull button, UINT message,
                                 WPARAM wparam, LPARAM lparam);
/* コピーに成功した札（slot）を、短い間だけ済みの表示にする。text は終端付き。 */
void note_pane_code_button_done(struct note_pane_code_button *_Nonnull button, size_t slot,
                                const char16_t *_Nonnull text);
/* 済みの表示を戻すタイマーなら消費して true。 */
[[nodiscard]] bool note_pane_code_button_timer(struct note_pane_code_button *_Nonnull button,
                                               WPARAM identifier);

#endif
