/* 文書の一覧とカテゴリ設定の唯一の所有者（ARC-004）。起動時にポートから台帳と走査結果を受け、
 * core の照合で 1 つの索引に固定する。外から見える状態は不変で、UI は派生値を受け取り、
 * 操作は意図（folio_state_toggle_category 等）として渡すだけ（ARC-011）。 */
#ifndef NENEFOLIO_FOLIO_STATE_H
#define NENEFOLIO_FOLIO_STATE_H

#include "drawer_metrics.h"
#include "folio_cursor_kind.h"
#include "folio_document_kind.h"
#include "folio_language.h"
#include "folio_note_change.h"
#include "folio_ports.h"
#include "folio_state_outcome.h"
#include "folio_step.h"
#include "folio_theme.h"
#include "history_row_view.h"
#include "note_destination.h"
#include "note_ref.h"
#include "pane_mode.h"
#include "pane_title_view.h"
#include "rename_view.h"
#include "replace_apply.h"
#include "replace_request.h"
#include "rgb_color.h"
#include "search_direction.h"

#include <stddef.h>
#include <uchar.h>

struct drawer_layout;
struct folio_state;
struct note_name;
struct note_text;
struct replace_edit;

/* ポートは複製して持つ。3 つのポートの adapter は state より長く生きていなければならない。
 * 束（struct folio_ports）は呼び出しの間だけ借り、返ったあとは参照しない（ADR 0029 の決定 3）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_create(const struct folio_ports *_Nonnull ports,
                   struct folio_state *_Nullable *_Nonnull out);
/* 描画に使うテーマ（ADR 0031 の決定 3）。選択が SYSTEM なら直近に読んだ OS の値を通す。 */
[[nodiscard]] enum folio_theme folio_state_theme(const struct folio_state *_Nonnull state);
/* 利用者が選んでいるテーマ。設定画面の印はこの値に付く（解決値ではない）。 */
[[nodiscard]] enum folio_theme_choice
folio_state_theme_choice(const struct folio_state *_Nonnull state);
/* テーマの選択を変える意図（ADR 0031 の決定 3）。**新しい設定と新しい閲覧文書を先に作り、
 * data/settings.json へ書けてから採用する**ので、表示と保存が食い違わない。
 * いまと同じ選択なら書かずに READY。作れなければ OUT_OF_MEMORY、書けなければ
 * SETTINGS_STORE_FAILED で、どちらもファイルも状態も変えない。設定が読めていなければ
 * SETTINGS_UNREADABLE（folio_state_set_number と同じ値で断り、上書きもしない）。
 * 設定は md・台帳・.rename.json と独立なので、未完了の改名の再開は通さない。 */
[[nodiscard]] enum folio_state_outcome folio_state_set_theme(struct folio_state *_Nonnull state,
                                                             enum folio_theme_choice choice);
/* OS の「アプリのモード」を読み直す意図（ADR 0031 の決定 3 / 4）。主窓が WM_SETTINGCHANGE
 * （ImmersiveColorSet）で呼ぶ。解決値が変わったときだけ閲覧文書を作り直す。設定は書かない。
 * 作れなければ OUT_OF_MEMORY で状態は変わらない。変わったかは UI が folio_state_theme を
 * 前後で比べて決める。 */
[[nodiscard]] enum folio_state_outcome
folio_state_refresh_theme(struct folio_state *_Nonnull state);
/* 利用者が選んでいる表示言語（ADR 0032 の決定 5）。UI は ui_text_line / ui_text_format と
 * ui_font_face へこの値を渡す。値の所有者はここ 1 つで、UI は第 2 の言語を持たない（ARC-004）。 */
[[nodiscard]] enum folio_language folio_state_language(const struct folio_state *_Nonnull state);
/* 表示言語を変える意図（ADR 0032 の決定 5）。folio_state_set_theme と同じ順で、
 * **新しい設定と新しい閲覧文書（新しい face の fonttbl）を先に作り、書けてから採用する**。
 * いまと同じ言語なら書かずに READY。作れなければ OUT_OF_MEMORY、書けなければ
 * SETTINGS_STORE_FAILED で、どちらもファイルも状態も変えない。設定が読めていなければ
 * SETTINGS_UNREADABLE。未完了の改名の再開は通さない。 */
[[nodiscard]] enum folio_state_outcome folio_state_set_language(struct folio_state *_Nonnull state,
                                                                enum folio_language language);
/* 起動時に読んだ設定の知らせ（ADR 0025 の決定 6）。読めていれば READY、
 * 壊れている・未知の版・読めないなら SETTINGS_UNREADABLE。合成ルートが窓を作る前に 1 回だけ
 * 尋ね、READY でなければ既存の 1 行で見せて起動を続ける（「見せたか」はどこにも持たない）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_settings_notice(const struct folio_state *_Nonnull state);
/* 編集中の本文の左に原文の行番号を出すか（FR-021 / ADR 0026 の決定 5）。
 * 値の所有者はここ 1 つで、UI は第 2 の真偽を持たない（ARC-004）。 */
[[nodiscard]] bool folio_state_number(const struct folio_state *_Nonnull state);
/* 行番号の表示を変える意図（ADR 0025 の決定 5）。**先に data/settings.json へ書いてから採用する**
 * ので、表示と保存が食い違わない。いまと同じ値なら書かずに READY。
 * 書けなければ値を変えず SETTINGS_STORE_FAILED、設定が読めていなければ SETTINGS_UNREADABLE
 * （上書きしない）。設定は md・台帳・.rename.json と独立なので、未完了の改名の再開は通さない。 */
[[nodiscard]] enum folio_state_outcome folio_state_set_number(struct folio_state *_Nonnull state,
                                                              bool number);
/* いまの索引と寸法からドロワーの配置を作り、選択中のノート・カーソル・いまのスクロール量に印を付ける。
 * 呼び出し側が破棄する。問い合わせなので状態は変えない（要求量の丸めは配置の中だけ・ADR 0009）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_drawer_layout(const struct folio_state *_Nonnull state, struct drawer_metrics metrics,
                          struct drawer_layout *_Nullable *_Nonnull out);
/* ドロワーを delta 画素ぶんスクロールする意図（FR-012 / ADR 0009 の決定 3）。
 * いまの寸法での有効量（要求量を上限で丸めた量）から数え直し、0〜上限に丸めて要求量にする。
 * 折り畳みで上限が縮んでいても 1 回で必ず動く。配置が作れなければ OUT_OF_MEMORY で状態は変えない。
 * 選択・トグル・並び替え・移動・編集はスクロール量を触らない。 */
[[nodiscard]] enum folio_state_outcome folio_state_scroll_drawer(struct folio_state *_Nonnull state,
                                                                 struct drawer_metrics metrics,
                                                                 int delta);
/* 索引にあるノートの総数。 */
[[nodiscard]] size_t folio_state_note_count(const struct folio_state *_Nonnull state);
/* ドロワーのスクロール量を、カーソルの行が見える位置に来る最小の量にする
 * （FR-018 / ADR 0013 の決定 6・ADR 0015 の決定 6）。カテゴリ行のカーソルにも効く。
 * カーソルが無ければ READY で何も変えない。
 * 配置が作れなければ OUT_OF_MEMORY で状態は変えない。鍵でカーソルを動かしたときだけ使う。 */
[[nodiscard]] enum folio_state_outcome folio_state_reveal_cursor(struct folio_state *_Nonnull state,
                                                                 struct drawer_metrics metrics);
/* カテゴリの展開状態を反転し、台帳を書き戻す（FR-004 / FR-007）。書き戻せなければ状態は変えない。
 */
[[nodiscard]] enum folio_state_outcome
folio_state_toggle_category(struct folio_state *_Nonnull state, size_t index);
/* カテゴリの展開状態を expanded にする（FR-018 の h / l・ADR 0015 の決定 3）。
 * index が範囲外なら NO_SUCH_CATEGORY。既にそうなら書かずに READY（カーソルも動かさない）。
 * 変わるときは folio_state_toggle_category と同じ経路で書き戻す。
 * カーソルがそのカテゴリにあるときだけ、書き戻したあとにカーソルが移る:
 * 畳めばそのカテゴリ行へ、展開すれば選択中のノートがその中にあればそのノートへ、
 * 無ければ最初のノートへ（右ペインが変わる。読めなければその結果を返しカーソルは行に残る）。
 * ノートが 0 本ならカテゴリ行に留まる。 */
[[nodiscard]] enum folio_state_outcome
folio_state_set_category_expanded(struct folio_state *_Nonnull state, size_t index, bool expanded);
/* カテゴリの色を変え、categories.json を書き戻す（FR-010 / ADR 0010 の決定 2）。
 * index が範囲外なら NO_SUCH_CATEGORY。いまと同じ色なら書かずに READY。
 * 書き戻せなければ状態は変えない。選択・編集モード・スクロール量は触らない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_recolor_category(struct folio_state *_Nonnull state, size_t index,
                             struct rgb_color color);
/* カテゴリの表示順を変え、categories.json を書き戻す（FR-009）。from == to は書かずに READY。
 * 書き戻せなければ状態は変えない。選択中のノートは移動後も同じノートを指す。 */
[[nodiscard]] enum folio_state_outcome folio_state_move_category(struct folio_state *_Nonnull state,
                                                                 size_t from, size_t to);
/* ノートを from から to へ動かす（FR-008 / FR-009）。同じカテゴリなら表示順を変えて index.json を
 * 書き戻し（from == to は書かずに READY・書き戻せなければ状態は変えない）、別のカテゴリなら md を
 * 移してから移動先・移動元の index.json を書き戻す（ADR 0008 の決定 3）。to.note は移動先の
 * ノート数と等しければ末尾。移動先に同じ名前があれば NAME_TAKEN で何もしない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_move_note(struct folio_state *_Nonnull state, struct note_ref from, struct note_ref to);
/* ノートを選び、本文を読んで右ペインの表示値を作る（FR-005）。読めなければ表示は変えない。
 * 表示モードは変えない（ADR 0013 の決定 4）。 */
[[nodiscard]] enum folio_state_outcome folio_state_select_note(struct folio_state *_Nonnull state,
                                                               size_t category, size_t note);
/* カーソルを歩みのぶんだけ動かす（FR-018 / ADR 0015 の決定 2）。
 * 止まる行は「カテゴリごとに、見えるノート行があればそのノート、無ければそのカテゴリ行 1 つ」で、
 * 台帳の順に辿る。カーソルが無ければ NEXT / FIRST は最初の止まる行、PREVIOUS / LAST は最後。
 * 端では動かず READY。止まる行が 1 つも無ければ（カテゴリが無い）NO_SUCH_NOTE。
 * ノートに止まるときは folio_state_select_note と同じ読み込み経路で、表示モードは変わらない。
 * カテゴリ行に止まるときはカーソルだけが動き、選択も右ペインも変わらない（保存も要らない）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_select_adjacent(struct folio_state *_Nonnull state, enum folio_step step);
/* 歩みの行き先の種類（ADR 0015 の決定 2）。行き先が無ければ（端・止まる行が無い）false で
 * kindとoutは触らない。UIは保存前に行き先を写し、保存でカーソルが動いても同じ対象へ進む。 */
[[nodiscard]] bool folio_state_step_kind(const struct folio_state *_Nonnull state,
                                         enum folio_step step,
                                         enum folio_cursor_kind *_Nonnull kind,
                                         struct note_ref *_Nonnull out);
/* 索引のカーソル。無ければ false で kind も out も触らない。
 * FOLIO_CURSOR_CATEGORY のとき out->note は使わない（カテゴリ行に止まっている）。 */
[[nodiscard]] bool folio_state_cursor(const struct folio_state *_Nonnull state,
                                      enum folio_cursor_kind *_Nonnull kind,
                                      struct note_ref *_Nonnull out);
/* 選択中のノートの居場所。何も選んでいなければ false で out は触らない。 */
[[nodiscard]] bool folio_state_selection(const struct folio_state *_Nonnull state,
                                         struct note_ref *_Nonnull out);
/* 編集モードへ入る（FR-006）。ノートを選んでいなければ NOTHING_SELECTED。 */
[[nodiscard]] enum folio_state_outcome folio_state_begin_edit(struct folio_state *_Nonnull state);
/* 現在文書の種類。無題は索引に存在しない（ADR0020）。 */
[[nodiscard]] enum folio_document_kind
folio_state_document_kind(const struct folio_state *_Nonnull state);
[[nodiscard]] size_t folio_state_category_count(const struct folio_state *_Nonnull state);
[[nodiscard]] const char *_Nonnull folio_state_category_name(
    const struct folio_state *_Nonnull state, size_t category);
/* カーソル→現在文書→最初のカテゴリ。カテゴリ0件なら0で、new_noteが拒否する。 */
[[nodiscard]] size_t folio_state_current_category(const struct folio_state *_Nonnull state);
/* 現在文書の保存先。NONEのときは呼ばない。 */
[[nodiscard]] size_t folio_state_document_category(const struct folio_state *_Nonnull state);
/* 現在本文の保存後にUIが呼ぶ。既に無題ならNAME_REQUIREDで本文を保護する。 */
[[nodiscard]] enum folio_state_outcome folio_state_new_note(struct folio_state *_Nonnull state,
                                                            size_t category);
/* 初回/別名保存の共通作成。VIEWではunits/countを使わず所有する原文を複製する。
 * EDITでは入力を正規化。元ノートのwrite/archiveは呼ばず、成功時は新ノートを選択する。 */
[[nodiscard]] enum folio_state_outcome
folio_state_store_new(struct folio_state *_Nonnull state,
                      const struct note_destination *_Nonnull destination,
                      const char16_t *_Nonnull units, size_t count);
/* 現在の文書の実名（台帳にある名前・終端付き UTF-8）。無題と未選択では空文字列。
 * 右ペインの頭の表示値とは別で、名前入力面の初期値はこちらを使う（ADR 0022 の決定 1）。
 * 未完了の改名があるあいだも、いま data/ にあるはずの名前（旧名）を返す。次の意図まで有効。 */
[[nodiscard]] const char *_Nonnull folio_state_document_name(
    const struct folio_state *_Nonnull state);
/* 未完了の改名の旧名と新名。保持していなければ false で out は触らない（ADR 0022 の決定 2）。
 * 文字列は次の意図まで有効で、改名が完了すると無効になる。 */
[[nodiscard]] bool folio_state_rename_pending(const struct folio_state *_Nonnull state,
                                              struct rename_view *_Nonnull out);
/* 現在のノートの md と履歴を同じカテゴリの新しい名前へ移す（FR-031 / ADR 0022）。
 * units / count は編集中の本文で、未保存なら同じ保存経路で先に確定してから改名する
 * （VIEW では使わない）。未選択は NOTHING_SELECTED、無題は NAME_REQUIRED（初回保存が先）、
 * 同じ名前は何もせず READY、大小文字だけ違う名前を含む同名は NAME_TAKEN。
 * 記録を公開する前の拒否では意図が残らない。公開後に止まったら RENAME_PENDING（再試行で進み得る）
 * または RENAME_HALTED（data/ を直すまで進まない）を返し、どちらでも意図を 1 つだけ保持して、
 * 次の意図より先に同じ改名を再試行する（決定 7）。
 * 成功しても本文・モード・選択位置は変えない。name は呼び出しの間だけ借りる。 */
[[nodiscard]] enum folio_state_outcome
folio_state_rename_note(struct folio_state *_Nonnull state, const struct note_name *_Nonnull name,
                        const char16_t *_Nonnull units, size_t count);
/* 編集中の本文（UTF-16 の単位列）を保存し、編集モードのまま残る（Ctrl+S）。
 * VIEWでは台帳だけ同期し本文は書かない。読んだ本文と同じなら書かない。書き戻せなければ状態は変えない（ADR
 * 0006）。 */
[[nodiscard]] enum folio_state_outcome folio_state_store_note(struct folio_state *_Nonnull state,
                                                              const char16_t *_Nonnull units,
                                                              size_t count);
/* 編集中の本文を保存時と同じ形へ正規化し、保存済み本文との差を答える。
 * VIEWでは台帳が同期済みならSAME。未同期なら保存系と同じ修復を先に試み、
 * 失敗ならLEDGER_UNSYNCED。この修復のほかは mode・本文・履歴・md を
 * 変えない問い合わせである（ADR 0016・ADR 0021 の決定 3）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_note_changed(struct folio_state *_Nonnull state, const char16_t *_Nonnull units,
                         size_t count, enum folio_note_change *_Nonnull out);
/* 編集中の本文を保存して閲覧へ戻る。保存できなければ編集モードのまま。
 * 使うのは「閲覧」の札。終了操作は編集モードを変えず store_note で保存する（ADR 0016）。
 * ノートの切り替えではモードを保つ（ADR 0013 の決定 4）。 */
[[nodiscard]] enum folio_state_outcome folio_state_end_edit(struct folio_state *_Nonnull state,
                                                            const char16_t *_Nonnull units,
                                                            size_t count);
/* 編集を破棄して保存済みの本文に戻す（`:e!`・ADR 0016 の補正 4）。前提の判定だけで状態は変えない:
 * 未選択は NOTHING_SELECTED、閲覧中は NOT_EDITING、編集中は READY。READY なら UI が
 * folio_state_pane_text（最後に読んだか保存に成功した本文・無題なら空）を本文へ流し込み直す。
 * ディスクは読み直さない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_discard_edits(const struct folio_state *_Nonnull state);
/* 索引の絞り込みの語を覚え、一致集合を作り直す（FR-032 / ADR 0024 の決定 7）。
 * UTF-16 の単位列を受ける C-014 の例外の 7 本目。ノート内検索の語（set_search_term）とは別で、
 * 同期しない。永続化もしない。
 * count が 0 なら絞り込みを解き、索引は台帳のとおりに戻る。語が壊れていれば SEARCH_MALFORMED で
 * 前の語と絞り込みを保つ。空でない語が**初めて**来たときだけ、ポートの read_note で全ノートの
 * 本文を 1 回読んで写しにする（起動時には読まない・決定 1）。読めないノートは写しを持たず、
 * 一致しない（理由は出さない）。
 * 変えたあとはスクロール量を 0 に戻し、カーソルの行が消えていれば最初に見える行へ移す。
 * 選択・右ペイン・モードは変えない（決定 5）。寄せる量は UI が folio_state_reveal_cursor で決める。
 */
[[nodiscard]] enum folio_state_outcome
folio_state_set_index_filter(struct folio_state *_Nonnull state, const char16_t *_Nonnull units,
                             size_t count);
/* いま絞り込んでいるか。UI はこれを見てドラッグを始めない（ADR 0024 の決定 4）。 */
[[nodiscard]] bool folio_state_filtering(const struct folio_state *_Nonnull state);
/* 覚えている絞り込みの語（UTF-8・終端付き）。無ければ空文字列。次の set まで有効。 */
[[nodiscard]] const char *_Nonnull folio_state_index_filter_term(
    const struct folio_state *_Nonnull state);
/* いま索引に見えているノートの数。絞り込んでいなければ索引の総数。 */
[[nodiscard]] size_t folio_state_index_filter_count(const struct folio_state *_Nonnull state);
/* 置換の下見を取り直す（FR-023 / ADR 0028 の決定 6）。UTF-16 の入口の**8 本目**で、
 * 位置が EM_EXSETSEL と 1 対 1 でなければならないので本文は UTF-8 へ写さない（C-014 の例外）。
 * 編集中でなければ NOT_EDITING、パターンが空なら REPLACE_NO_PATTERN。
 * 走査と置換文字列の失敗は REPLACE_BAD_PATTERN / REPLACE_BAD_TEMPLATE / REPLACE_TIMED_OUT /
 * REPLACE_TOO_COMPLEX / REPLACE_TOO_MANY へ写し、そのときは前の下見をそのまま保つ。
 * 成功すると古い下見を捨てて新しい下見を持つ。件数はゼロ幅の一致も 1 件。
 * 本文も選択も RichEdit が持ち、ここは触らない（ADR 0023 の決定 3 と同じ境界）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_preview_replace(struct folio_state *_Nonnull state,
                            const struct replace_request *_Nonnull request);
/* 直前の下見の一致の件数。下見が無ければ 0。 */
[[nodiscard]] size_t folio_state_replace_count(const struct folio_state *_Nonnull state);
/* 直前の下見が REPLACE_BAD_PATTERN だったときの位置（1 起算のコード単位）。
 * それ以外は 0 で、UI は位置を添えない（決定 9）。 */
[[nodiscard]] size_t folio_state_replace_error_offset(const struct folio_state *_Nonnull state);
/* 下見を本文へ当てる（決定 6）。UTF-16 の入口の**9 本目**。
 * 渡された本文が下見の写しと違う、宛先が今の文書と違う、下見が無いなら REPLACE_STALE で
 * 何も変えない。反転した anchor は REPLACE_BAD_SPAN。
 * 成功しても md は書かず、RichEdit へ当てるのは UI である。適用する一致が無ければ READY で
 * out は nullptr になる。out は失敗のときも nullptr で、成功時だけ呼び出し側が
 * replace_edit_destroy する。 */
[[nodiscard]] enum folio_state_outcome
folio_state_apply_replace(struct folio_state *_Nonnull state,
                          const struct replace_apply *_Nonnull apply,
                          struct replace_edit *_Nullable *_Nonnull out);
/* ノート内検索の語を覚える（FR-011 / ADR 0023 の決定 3）。UTF-16 の単位列を受ける
 * C-014 の例外で、store_new / rename_note / store_note / note_changed / end_edit に次ぐ 6 本目。
 * count が 0 なら語を捨てる。語が UTF-16 として壊れていれば SEARCH_MALFORMED で前の語を保つ。
 * #40 の絞り込み語とは別の語で、同期しない。本文も選択もフォーカスも持たず、永続化もしない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_set_search_term(struct folio_state *_Nonnull state, const char16_t *_Nonnull units,
                            size_t count);
/* 覚えている語（UTF-8・終端付き）。無ければ空文字列。次の set まで有効。 */
[[nodiscard]] const char *_Nonnull folio_state_search_term(
    const struct folio_state *_Nonnull state);
[[nodiscard]] size_t folio_state_search_term_length(const struct folio_state *_Nonnull state);
/* 直前の方向（`/` と「次へ」なら前方、`?` と「前へ」なら後方）。初期値は前方。
 * n / N の N は逆向きに 1 回進むだけで、覚えている方向は変えない。 */
[[nodiscard]] enum search_direction
folio_state_search_direction(const struct folio_state *_Nonnull state);
void folio_state_set_search_direction(struct folio_state *_Nonnull state,
                                      enum search_direction direction);
/* いまの表示モード。 */
[[nodiscard]] enum pane_mode folio_state_pane_mode(const struct folio_state *_Nonnull state);
/* 選択中のノートの本文（UTF-8・終端付き）。何も選んでいなければ空文字列。次の意図まで有効。 */
[[nodiscard]] const char *_Nonnull folio_state_pane_text(const struct folio_state *_Nonnull state);
[[nodiscard]] size_t folio_state_pane_text_length(const struct folio_state *_Nonnull state);
/* 右ペインの頭の表示値。 */
[[nodiscard]] struct pane_title_view
folio_state_pane_title(const struct folio_state *_Nonnull state);
/* 右ペインに写す RTF（終端付き）。何も選んでいなければ空の文書。次の意図まで有効。 */
[[nodiscard]] const char *_Nonnull folio_state_pane_rtf(const struct folio_state *_Nonnull state);
[[nodiscard]] size_t folio_state_pane_rtf_length(const struct folio_state *_Nonnull state);
/* 現在のノートの履歴の一覧を開く（FR-033 / ADR 0038 の決定 7）。前の一覧は先に捨てる。
 * port の read_history で 1〜note_history_depth を順に読み、無い版は飛ばし、読めない版
 * （UTF-8 でない・読めない）は本文の無い行として版番号順に持つ。md も履歴も書かない。
 * 未選択は NOTHING_SELECTED、無題は NAME_REQUIRED、1 版も無ければ HISTORY_EMPTY で、
 * どれも一覧を持たない。確保に失敗すれば OUT_OF_MEMORY で、途中まで読んだ版も捨てる。 */
[[nodiscard]] enum folio_state_outcome folio_state_open_history(struct folio_state *_Nonnull state);
/* 開いている一覧の行数。開いていなければ 0。 */
[[nodiscard]] size_t folio_state_history_count(const struct folio_state *_Nonnull state);
/* 一覧の index 行の表示値（決定 8）。範囲の外なら false で out は触らない。
 * 見出しは本文の最初の空でない論理行（note_text_first_line）で、時刻は持たない。 */
[[nodiscard]] bool folio_state_history_row(const struct folio_state *_Nonnull state, size_t index,
                                           struct history_row_view *_Nonnull out);
/* 一覧の index 行の本文。読めない版と範囲の外は nullptr。次の意図まで有効。 */
[[nodiscard]] const struct note_text *_Nullable folio_state_history_body(
    const struct folio_state *_Nonnull state, size_t index);
/* index 行の版へ戻す意図（決定 1 / 9）。読めない行と範囲の外は HISTORY_UNREADABLE で何も変えない。
 * 閲覧中なら folio_state_begin_edit と同じ遷移で編集モードへ入り、READY を返す。
 * 本文は folio_state_history_body で取り、UI が note_pane_replace で流し込む（未保存の変更）。
 * ファイルは書かず、一覧も捨てない（閉じるのは UI の close）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_restore_history(struct folio_state *_Nonnull state, size_t index);
/* 一覧を捨てる（決定 10）。面を閉じるときに UI が呼ぶ。別の文書へ移る・保存する・破棄の経路でも
 * application が自分で呼ぶので、古い版を持ち越さない。 */
void folio_state_close_history(struct folio_state *_Nonnull state);
/* READY 以外の結果を利用者に見せる 1 行（UTF-8・終端付き・静的）。READY は空文字列。 */
[[nodiscard]] const char *_Nonnull folio_state_failure_line(enum folio_state_outcome outcome,
                                                            enum folio_language language);
void folio_state_destroy(struct folio_state *_Nullable state);

#endif
