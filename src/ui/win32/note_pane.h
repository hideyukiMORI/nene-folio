/* 右ペイン（FR-005 の閲覧側と FR-006 の編集側）。Msftedit.dll の RICHEDIT50W を所有し、
 * application が作った RTF を EM_STREAMIN で写し、編集中の本文を EM_STREAMOUT で渡すだけで、
 * 判断を持たない（ARC-011 / ADR 0002 / ADR 0006）。 */
#ifndef NENEFOLIO_NOTE_PANE_H
#define NENEFOLIO_NOTE_PANE_H

#include "gutter_row.h"
#include "note_pane_outcome.h"
#include "note_pane_text_outcome.h"
#include "note_search_span.h"

#include <stddef.h>
#include <uchar.h>
#include <windows.h>

struct note_pane;

/* 本文の RichEdit の control id。主窓は WM_COMMAND を送り手の HWND と id で振り分ける
 * （#40 の絞り込みの欄の EN_CHANGE と同じ窓に届く・ADR 0026 の決定 4）。 */
constexpr int note_pane_control_id = 3;

/* background は地の色、text は編集モードの本文色（テーマの正本 folio_palette から）。 */
[[nodiscard]] enum note_pane_outcome note_pane_create(HWND _Nonnull parent, COLORREF background,
                                                      COLORREF text,
                                                      struct note_pane *_Nullable *_Nonnull out);
/* 親が配置に使う。ウィンドウが既に破棄されていれば nullptr。 */
[[nodiscard]] HWND _Nullable note_pane_handle(const struct note_pane *_Nonnull pane);
/* 未確定入力中は親がキーを操作へ変換せず、RichEdit/IMEへ渡す（ADR 0018）。 */
[[nodiscard]] bool note_pane_composing(const struct note_pane *_Nonnull pane);
/* 編集本文だけを対象に主窓が呼ぶ。処理済みなら元のMSGを再翻訳しない（ADR 0019）。 */
[[nodiscard]] bool note_pane_translate(const struct note_pane *_Nonnull pane,
                                       const MSG *_Nonnull message);
/* RTF を流し込んで表示を置き換え、読み取り専用に戻す（閲覧）。 */
void note_pane_render(struct note_pane *_Nonnull pane, const char *_Nonnull rtf, size_t length);
/* 本文を平文で流し込み、入力を受け付ける（編集）。フォーカスは動かさない
 * （区画を移すのは主窓の仕事・ADR 0013 の決定 1）。 */
void note_pane_edit(struct note_pane *_Nonnull pane, const char16_t *_Nonnull units, size_t count);
/* 編集中の本文を UTF-16 で取り出す。pane が所有し、次の note_pane の呼び出しまで有効。 */
[[nodiscard]] enum note_pane_text_outcome note_pane_text(struct note_pane *_Nonnull pane,
                                                         const char16_t *_Nonnull *_Nonnull units,
                                                         size_t *_Nonnull count);
/* いま表示している平文を UTF-16 で取り出す（閲覧なら描画後の文字・編集なら未保存本文）。
 * 段落区切りは CR 1 つで、位置は note_pane_select / note_pane_selection と 1 対 1
 * （保存用の note_pane_text は CRLF なので別物・ADR 0023 の決定 1）。
 * pane が所有し、次の note_pane の呼び出しまで有効。 */
[[nodiscard]] enum note_pane_text_outcome
note_pane_display_text(struct note_pane *_Nonnull pane, const char16_t *_Nonnull *_Nonnull units,
                       size_t *_Nonnull count);
/* 一致 1 つを選択して見える位置へ寄せる。フォーカスも本文も Undo も触らない。 */
void note_pane_select(struct note_pane *_Nonnull pane, size_t start, size_t end);
/* span の範囲を units で置き換える（ADR 0028 の決定 7）。EM_EXSETSEL → EM_REPLACESEL(TRUE) の
 * 1 回なので Undo も 1 単位になる。units は終端付きで、長さは EM_REPLACESEL が終端で決める
 * （ADR 0028 は 5 引数で書いていたが C-012 の上限に収めて span へ束ねた）。
 * 流し込みの印も EM_SETMODIFY も触らない。EN_CHANGE は既存の経路で 1 回出る。 */
void note_pane_replace(struct note_pane *_Nonnull pane, struct note_search_span span,
                       const char16_t *_Nonnull units);
/* いまの選択範囲（表示中の平文の位置）。窓が無ければ false で start も end も触らない。 */
[[nodiscard]] bool note_pane_selection(const struct note_pane *_Nonnull pane,
                                       size_t *_Nonnull start, size_t *_Nonnull end);
/* 論理行の表を「古い」と印を付ける（EN_CHANGE と本文の差し替え・ADR 0026 の決定 3）。
 * ここでは本文へ問い合わせず、次に番号を描くときに作り直す。 */
void note_pane_invalidate_lines(struct note_pane *_Nonnull pane);
/* いま見えている表示行のうち、論理行の先頭になっている行だけを rows へ並べる（決定 3）。
 * y は主窓の client 座標へ直して返し、高さは表示行ごとに EM_POSFROMCHAR から得る
 * （行高は一定でないので番号 × 行高では求めない）。capacity を超えたら描けるぶんだけで止める。
 * 本文が古ければ先に表を作り直す。取り出せなければ false（帯を描かない）。 */
[[nodiscard]] bool note_pane_visible_rows(struct note_pane *_Nonnull pane,
                                          struct gutter_row *_Nonnull rows, size_t capacity,
                                          size_t *_Nonnull count);
/* 番号の桁数（帯の幅を決める・決定 6）。古ければ表ごと作り直す（平文を取り出すのは
 * 本文が変わったあとの 1 回だけで、配置のたびではない）。取り出せなければ最小の桁数。 */
[[nodiscard]] size_t note_pane_line_digits(struct note_pane *_Nonnull pane);
/* いま最初に見えている論理行の番号。取れなければ 0（決定 6 の復元の前半）。 */
[[nodiscard]] size_t note_pane_first_visible_line(struct note_pane *_Nonnull pane);
/* その論理行が最初に見えるところまで戻す。本文・Undo・選択は触らない（決定 6 の後半）。
 * 折り返しの途中から見えていた場合は、その論理行の先頭へ寄る。 */
void note_pane_scroll_to_line(struct note_pane *_Nonnull pane, size_t number);
void note_pane_destroy(struct note_pane *_Nullable pane);

#endif
