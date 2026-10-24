/* 文言の正本の単体（FR なし・ADR 0030 の「検証」）。
 * 期待表は **#74 の前のソースから機械的に写した**もので、ui_text の表と 1 字ずつ突き合わせる
 * （#65 と同じ手順。道具は out/design/2026-09-22/ui-text-probe/）。
 *
 * **どこにも残らない文言が 2 つある**ので期待表には無い。どちらも全値の switch の後ろの
 * 到達しない既定で、表示されたことが無い:
 *   - "このノート内を検索  "（folio_window.c の search_direction_label）
 *   - "実行ファイルの場所が取得できません。"（main.c の adapter_failure）
 * 確保に失敗したときの退避の文言（name_prompt.c の「エラー表示の記憶域が不足しています。…」）も、
 * 確保そのものが無くなるので消える（決定 5）。
 *
 * #76（ADR 0032）で期待表が動いたのは次の 2 種類だけである:
 *   - 構文の見本を ASCII に揃えた 2 件（HELP_EX_SUBSTITUTE と COMMAND_SUBSTITUTE の
 *     「パターン／置換」→ `pattern` / `replacement`。決定 3）
 *   - この単位で新しく足した 5 つの ID（HELP_EX_SET_LANGUAGE・SETTINGS_LANGUAGE と 3 つの自称） */
#include "folio_command.h"
#include "folio_language.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "unit_tests.h"
#include "utf16_text.h"

#include <string.h>

/* 変更前の文言（機械的に写した）。ui_text.c の表と 1 字も違ってはいけない。 */
static const char *_Nonnull const expected[] = {
    [UI_TEXT_ACCESSIBLE_COMMAND] = "コマンド入力",
    [UI_TEXT_ACCESSIBLE_PALETTE] = "コマンド一覧",
    [UI_TEXT_ACCESSIBLE_PATTERN] = "検索する正規表現",
    [UI_TEXT_ACCESSIBLE_REPLACEMENT] = "置換文字列",
    [UI_TEXT_ACCESSIBLE_CURRENT] = "現在の設定",
    [UI_TEXT_ACCESSIBLE_NO_RESULTS] = "候補がありません。",
    [UI_TEXT_ACCESSIBLE_HEADER] = "共通操作",
    [UI_TEXT_ACCESSIBLE_PANE_TOOLS] = "本文の操作帯",
    [UI_TEXT_ACCESSIBLE_MODE] = "ノートのモード",
    [UI_TEXT_ACCESSIBLE_BODY] = "ノート本文",
    [UI_TEXT_ACCESSIBLE_NAMED_BODY] = "{name} — ノート本文",
    [UI_TEXT_HELP_ACCESSIBILITY] = "F6 / Shift+F6 区画の移動 · Tab / Shift+Tab 区画内の移動",
    [UI_TEXT_EMPTY] = "",
    [UI_TEXT_FAILURE_APPEARANCE_UNAVAILABLE] =
        "Windowsの外観を取得できません。前の配色（初回は通常配色）を保ちます。",
    [UI_TEXT_HIGH_CONTRAST_ACTIVE] = "Windowsの高コントラストを適用中",
    [UI_TEXT_FAILURE_DATA_UNREADABLE] = "data/ を読めませんでした。",
    [UI_TEXT_FAILURE_LEDGER_MALFORMED] =
        "data/ の台帳（categories.json / index.json）が版 1 の形ではありません。",
    [UI_TEXT_FAILURE_STORE_FAILED] = "data/ の台帳（categories.json / "
                                     "index.json）に書き戻せませんでした。表示は変えていません。",
    [UI_TEXT_FAILURE_NO_SUCH_CATEGORY] = "索引に無いカテゴリが操作されました。",
    [UI_TEXT_FAILURE_NO_SUCH_NOTE] = "索引に無いノートが操作されました。",
    [UI_TEXT_FAILURE_NOTE_UNREADABLE] = "ノートを読めませんでした。表示は変えていません。",
    [UI_TEXT_FAILURE_NOTHING_SELECTED] = "ノートを選んでから編集してください。",
    [UI_TEXT_FAILURE_NOT_EDITING] = "編集モードではありません。",
    [UI_TEXT_FAILURE_NOTE_MALFORMED] =
        "編集中の本文に壊れた文字、または扱えない文字があります。保存していません。",
    [UI_TEXT_FAILURE_NOTE_TOO_LARGE] =
        "本文が大きすぎて保存できません。上限は 16 MiB です。本文を減らしてから保存してください。",
    [UI_TEXT_FAILURE_NOTE_UNEDITABLE] = "この本文には編集で保てない文字（U+0007・U+2028・U+"
                                        "2029）があります。閲覧のままにしました。",
    [UI_TEXT_FAILURE_NOTE_STORE_FAILED] =
        "ノートを書き戻せませんでした。編集中の本文はそのままです。"
        "別名で保存するか、編集を破棄して読み直せます。",
    [UI_TEXT_FAILURE_HISTORY_FAILED] =
        "履歴を書けなかったので保存していません。編集中の本文は残っています。"
        "編集を破棄して読み直せます。",
    [UI_TEXT_FAILURE_UNSAVED_CHANGES] =
        "未保存の変更があります。保存するか、未保存変更を破棄して終了してください。",
    [UI_TEXT_FAILURE_NAME_TAKEN] =
        "同じ名前のノートがあります。別の名前を指定してください。既存ファイルは変更していません。",
    [UI_TEXT_FAILURE_LEDGER_STALE] =
        "mdは反映しましたが、台帳（index."
        "json）を書き戻せませんでした。保存を再試行するか、次回の起動で揃います。",
    [UI_TEXT_FAILURE_LEDGER_UNSYNCED] =
        "前回の台帳（index."
        "json）をまだ書き戻せていません。今回の操作は行っていないので、保存を再試行してください。",
    [UI_TEXT_FAILURE_RENAME_PENDING] =
        "名前の変更が途中で止まっています。「名前を変更」から再試行してください。",
    [UI_TEXT_FAILURE_RENAME_UNLOCKED] =
        "data/ に書けないため名前を変更できません。名前は変更していません。",
    [UI_TEXT_FAILURE_RENAME_UNSUPPORTED] = "この data/ では名前を変更できません（ローカルの NTFS "
                                           "以外、またはシンボリックリンク／junction）。",
    [UI_TEXT_FAILURE_RENAME_IDENTITY_FAILED] =
        "元のファイルやカテゴリを確かめられないので名前を変更できません。名前は変更していません。",
    [UI_TEXT_FAILURE_RENAME_JOURNAL_FAILED] =
        "名前変更の記録（data/.rename.json）を書けませんでした。名前は変更していません。",
    [UI_TEXT_FAILURE_RENAME_JOURNAL_BROKEN] =
        "名前変更の記録（data/.rename.json）の版か形を読めません。記録は残しています。",
    [UI_TEXT_FAILURE_RENAME_HALTED] = "名前変更の記録と実ファイルが一致しません。data/.rename.json "
                                      "と data/<カテゴリ>/ を確認してください。",
    [UI_TEXT_FAILURE_SEARCH_MALFORMED] = "検索する語に壊れた文字があります。語は前のままです。",
    [UI_TEXT_FAILURE_FILTERED] =
        "絞り込み中はこの操作ができません。絞り込みを解いてからやり直してください。",
    [UI_TEXT_FAILURE_SETTINGS_UNREADABLE] =
        "設定（data/settings.json）を読めません。既定値で始め、直すまで上書きしません。",
    [UI_TEXT_FAILURE_SETTINGS_STORE_FAILED] =
        "設定（data/settings.json）を書けませんでした。設定は変えていません。",
    [UI_TEXT_FAILURE_PANE_UNAVAILABLE] = "表示中の本文を取り出せませんでした。探していません。",
    [UI_TEXT_FAILURE_PANE_LOAD_FAILED] = "本文読込失敗。同じモードで再試行。",
    [UI_TEXT_FAILURE_PANE_RECOLOR_FAILED] =
        "本文の配色を更新できませんでした。本文は保持しています。テーマの再選択で再試行できます。",
    [UI_TEXT_FAILURE_REPLACE_NO_PATTERN] = "置換するパターンを入れてください。",
    [UI_TEXT_FAILURE_REPLACE_BAD_PATTERN] = "正規表現の書き方が違います。",
    [UI_TEXT_FAILURE_REPLACE_BAD_TEMPLATE] =
        "置換後の文字列の書き方が違います。使えるのは & \\0〜\\9 \\r \\n \\\\ \\& \\/ です。",
    [UI_TEXT_FAILURE_REPLACE_TIMED_OUT] =
        "このパターンは時間がかかりすぎるので止めました。本文は変えていません。",
    [UI_TEXT_FAILURE_REPLACE_TOO_COMPLEX] =
        "このパターンは複雑すぎて当てられません。本文は変えていません。",
    [UI_TEXT_FAILURE_REPLACE_TOO_MANY] = "一致が多すぎます。パターンを狭めてください。",
    [UI_TEXT_FAILURE_REPLACE_TOO_LARGE] = "置き換えた本文が大きすぎます。本文は変えていません。",
    [UI_TEXT_FAILURE_REPLACE_STALE] = "下見が古くなりました。もう一度入力してください。",
    [UI_TEXT_FAILURE_REPLACE_BAD_SPAN] = "選択範囲が正しくありません。本文は変えていません。",
    [UI_TEXT_FAILURE_OUT_OF_MEMORY] = "記憶域が足りません。",
    [UI_TEXT_FAILURE_NAME_REQUIRED] =
        "無題のノートに名前をつけて保存してください。本文は残っています。",
    [UI_TEXT_FAILURE_INVALID_NAME] = "使えない名前です。予約名・末尾の空白やピリオド・区切りを避け"
                                     "、.mdを含め255バイト以内で指定してください。",
    [UI_TEXT_FAILURE_ALREADY_NAMED] = "このノートには名前があります。別名保存（:"
                                      "saveas）または名前変更（:rename）を使ってください。",
    [UI_TEXT_FAILURE_HISTORY_EMPTY] = "履歴はありません。",
    [UI_TEXT_FAILURE_HISTORY_UNREADABLE] = "その版は読めません。",
    [UI_TEXT_FAILURE_CATEGORY_NAME_INVALID] =
        "使えないカテゴリ名です。先頭のピリオド・予約名・末尾の空白やピリオド・区切りを避け、"
        "255バイト以内で指定してください。",
    [UI_TEXT_FAILURE_CATEGORY_NAME_TAKEN] =
        "同じ名前のカテゴリがあります。別の名前を指定してください。何も変えていません。",
    [UI_TEXT_FAILURE_CATEGORY_NOT_CREATED] =
        "カテゴリのフォルダを作れませんでした。何も変えていません。"
        "data/ に書き込めるか確かめてください。",
    [UI_TEXT_FAILURE_CATEGORY_LEDGER_STALE] =
        "カテゴリは反映しましたが、台帳（categories.json）を書き戻せませんでした。"
        "表示はフォルダに従い、次に台帳を書くときに揃います。",
    [UI_TEXT_FAILURE_TRASH_UNAVAILABLE] =
        "この場所ではごみ箱を使えないため、ノートを移していません。",
    [UI_TEXT_FAILURE_TRASH_BUSY] = "ほかのプログラムが使用中のため、ノートを移していません。",
    [UI_TEXT_FAILURE_TRASH_FAILED] =
        "ごみ箱への移動を確認できませんでした。ファイルとごみ箱を確認してください。"
        "履歴だけ移っている場合があります。",
    [UI_TEXT_FAILURE_CATEGORY_NOT_EMPTY] =
        "ノートが残っているカテゴリや、無題のノートの保存先は削除できません。",
    [UI_TEXT_FAILURE_CATEGORY_HAS_FILES] =
        "フォルダにほかのファイルが残っているので削除していません。フォルダを確認してください。",
    [UI_TEXT_FAILURE_CATEGORY_HISTORY_NOT_RECYCLED] =
        "履歴のごみ箱への移動を確認できないため、カテゴリのフォルダを削除していません。"
        "履歴とごみ箱を確認してください。",
    [UI_TEXT_FAILURE_CATEGORY_REMOVE_FAILED] =
        "カテゴリのフォルダの削除を確認できませんでした。フォルダとごみ箱を確認してください。"
        "履歴や索引だけ処理済みの場合があります。",
    [UI_TEXT_FAILURE_FONT_BUNDLE_UNAVAILABLE] =
        "同梱の書体を読めなかったので、OS の書体で表示します。",
    [UI_TEXT_COMMAND_SAVE] = "保存",
    [UI_TEXT_COMMAND_QUIT] = "保存済みなら終了",
    [UI_TEXT_COMMAND_SAVE_QUIT] = "保存して終了",
    [UI_TEXT_COMMAND_FORCE_QUIT] = "未保存変更を破棄して終了",
    [UI_TEXT_COMMAND_HELP] = "ヘルプ",
    [UI_TEXT_COMMAND_EDIT] = "編集",
    [UI_TEXT_COMMAND_VIEW] = "保存して閲覧",
    [UI_TEXT_COMMAND_NEW] = "新しいノート",
    [UI_TEXT_COMMAND_SAVE_AS] = "別名で保存",
    [UI_TEXT_COMMAND_RENAME] = "名前を変更",
    [UI_TEXT_COMMAND_RENAME_CATEGORY] = "カテゴリ名を変更",
    [UI_TEXT_MENU_RENAME_CATEGORY] = "カテゴリ名を変更…",
    [UI_TEXT_HELP_RENAME_CATEGORY] = "Ex  :renamecategory / :rencat 名前 カテゴリ名を変更",
    [UI_TEXT_COMMAND_FIND] = "このノート内を検索",
    [UI_TEXT_COMMAND_SET] = "設定を変える（:set number）",
    [UI_TEXT_COMMAND_TOGGLE_NUMBER] = "行番号の表示を切り替える",
    [UI_TEXT_COMMAND_REPLACE] = "置換",
    /* #76 で構文の見本だけ ASCII に揃えた（括弧の外の日本語はそのまま・決定 3）。 */
    [UI_TEXT_COMMAND_SUBSTITUTE] = "正規表現で置換（:%s/pattern/replacement/g）",
    [UI_TEXT_COMMAND_SETTINGS] = "設定",
    [UI_TEXT_COMMAND_LAYOUT_DRAWER] = "一覧のみ表示",
    [UI_TEXT_COMMAND_LAYOUT_BOTH] = "一覧とノートを表示",
    [UI_TEXT_COMMAND_LAYOUT_NOTE] = "ノートのみ表示",
    [UI_TEXT_LAYOUT_BUTTON_DRAWER] = "一覧",
    [UI_TEXT_LAYOUT_BUTTON_BOTH] = "両方",
    [UI_TEXT_LAYOUT_BUTTON_NOTE] = "ノート",
    [UI_TEXT_LAYOUT_GROUP] = "表示",
    [UI_TEXT_LAYOUT_KEY_DRAWER] = "Ctrl+Shift+1",
    [UI_TEXT_LAYOUT_KEY_BOTH] = "Ctrl+Shift+2",
    [UI_TEXT_LAYOUT_KEY_NOTE] = "Ctrl+Shift+3",
    [UI_TEXT_LAYOUT_NARROW_HINT] = "\n窓を細くします。",
    [UI_TEXT_TOOLTIP_NAMED_KEY] = "{name}\n{from}{to}",
    [UI_TEXT_LAYOUT_MENU] = "{name}\t{from}",
    [UI_TEXT_LAYOUT_HELP] = "{from}  {name}",
    [UI_TEXT_TOOLTIP_SETTINGS] = "設定を開く",
    [UI_TEXT_TOOLTIP_CLOSE] = "閉じる",
    /* #348（ADR0060 の決定 5）で tooltip を名前とキーの 1 つの元から組むために足した。 */
    [UI_TEXT_TOOLTIP_KEY] = "{from}{to}",
    [UI_TEXT_TOOLTIP_OPERATIONS] = "このノートの操作",
    [UI_TEXT_TOOLTIP_EDIT] = "本文の編集を始める",
    [UI_TEXT_KEY_NEW] = "Ctrl+N",
    [UI_TEXT_KEY_SAVE] = "Ctrl+S",
    [UI_TEXT_KEY_HELP] = "F1",
    [UI_TEXT_KEY_FIND] = "Ctrl+F",
    [UI_TEXT_KEY_CLOSE] = "Alt+F4",
    [UI_TEXT_COMMAND_DISCARD_EDITS] = "編集を破棄して読み直す",
    [UI_TEXT_COMMAND_HISTORY] = "履歴から戻す",
    [UI_TEXT_COMMAND_TRASH_NOTE] = "ノートをごみ箱へ移す",
    [UI_TEXT_MENU_TRASH_NOTE] = "ごみ箱へ移す",
    [UI_TEXT_TRASH_NOTE_COMPLETED] = "一覧から除きました。",
    [UI_TEXT_NOTE_OPENED_IN_VIEW] = "閲覧で開きました。編集で保てない文字があります。",
    [UI_TEXT_HELP_TRASH_NOTE] = "Ex  :trashnote / :trash ノートと履歴をごみ箱へ移す（確認なし）",
    [UI_TEXT_COMMAND_DELETE_CATEGORY] = "カテゴリを削除",
    [UI_TEXT_MENU_DELETE_CATEGORY] = "カテゴリを削除",
    [UI_TEXT_FAILURE_CATEGORY_NOT_SELECTED] = "カテゴリを選んでください。",
    [UI_TEXT_HELP_DELETE_CATEGORY] = "Ex  :deletecategory / :delcat 空カテゴリを削除（確認なし）",
    [UI_TEXT_COMMAND_NEW_CATEGORY] = "新しいカテゴリ",
    [UI_TEXT_MENU_RECOLOR] = "色を変える…",
    [UI_TEXT_MENU_RENAME_NOTE] = "名前を変更…",
    [UI_TEXT_MENU_NEW_CATEGORY] = "新しいカテゴリ…",
    [UI_TEXT_MENU_NEW_NOTE_IN_CATEGORY] = "このカテゴリに新しいノート",
    [UI_TEXT_HELP_PALETTE] = "一覧  ↑↓ 選択 / Enter 実行 / Esc 閉じる / Tab 説明",
    [UI_TEXT_HELP_EDITOR_MOTION] = "編集本文  Ctrl+h/j/k/l ←/↓/↑/→",
    [UI_TEXT_HELP_GLOBAL_KEYS] = "全区画  {name} ヘルプ / {from} 一覧 / {to} このノート内を検索",
    [UI_TEXT_HELP_GLOBAL_FILTER] = "全区画  {name} すべてのノートを検索（索引を絞り込む）",
    [UI_TEXT_HELP_FILTER_FIELD] =
        "絞り込み欄  Esc・Enter 索引へ戻る（絞り込みは残る）/ 空にすると解除",
    [UI_TEXT_HELP_FILTER_LIMITS] =
        "絞り込み中  並び替えと折畳/展開はできない（色・保存・編集は可）",
    [UI_TEXT_HELP_FILE_KEYS] = "入力欄でも  {name} 新規 / {from} 保存 / {to} 別名保存",
    [UI_TEXT_HELP_INDEX_COMMAND] = "索引・閲覧本文  : コマンド / i 編集",
    [UI_TEXT_HELP_EX_SET_NUMBER] =
        "Ex  :set number / :set nonumber / :set nu!（編集中の原文の行番号）",
    [UI_TEXT_HELP_EX_SET_THEME] = "Ex  :set theme=dracula / :set theme=system",
    [UI_TEXT_HELP_EX_SET_LANGUAGE] =
        "Ex  :set language=ja / :set language=en / :set language=zh-Hans",
    [UI_TEXT_HELP_REPLACE_FIELD] =
        "置換欄  Tab 欄を移動 / Enter 1 件 / Ctrl+Enter すべて / Esc 閉じる",
    [UI_TEXT_HELP_EX_SUBSTITUTE] =
        "Ex  :%s/pattern/replacement/[g]（g なしは各行の最初の一致・正規表現）",
    [UI_TEXT_HELP_SEARCH_KEYS] = "索引・閲覧本文  / 次を検索 / ? 前を検索 / n・N 繰り返し",
    [UI_TEXT_HELP_SEARCH_STEP] = "全区画  {name} 次の一致 / {from} 前の一致（向きは変えない）",
    [UI_TEXT_HELP_SEARCH_FIELD] = "検索欄  {name} 次 / {from} 逆 / Esc 閉じる（選択は残る）",
    [UI_TEXT_HELP_INDEX_MOTION] = "索引  j/k 次/前 / gg/G 先頭/末尾",
    [UI_TEXT_HELP_INDEX_FOLD] = "索引  h/l 折畳/展開 / Enter 本文",
    [UI_TEXT_HELP_INDEX_SCROLL] = "索引  ↑↓ / PgUp/PgDn スクロール",
    [UI_TEXT_HELP_EDITOR_ESCAPE] = "本文  Esc 保存して索引へ（編集は維持）",
    [UI_TEXT_SESSION_UNTITLED] = "無題のノートを保存してから終了してください。",
    [UI_TEXT_SESSION_RENAME_PENDING] = "名前変更の復旧を完了してから終了してください。",
    [UI_TEXT_SESSION_COMPOSING] = "文字の変換を確定してから終了してください。",
    [UI_TEXT_SESSION_BUSY] = "開いている操作を終えてから終了してください。",
    [UI_TEXT_SESSION_SAVE_INCOMPLETE] = "保存処理が未完了です。保存を再試行してください。",
    [UI_TEXT_TITLE_UNTITLED] = "無題",
    [UI_TEXT_TITLE_RECOVERING] = "名前変更の復旧待ち",
    [UI_TEXT_TITLE_CATEGORY_RECOVERING] = "復旧待ち",
    [UI_TEXT_CHIP_VIEW] = "閲覧",
    [UI_TEXT_CHIP_EDIT] = "編集",
    [UI_TEXT_PLACEHOLDER_FILTER] = "すべてのノートを検索",
    /* #86 で足した。索引の頭の帯の右に出す「一致 / 総数」（ADR 0024 の 2026-09-22 の補正 7）。 */
    [UI_TEXT_STATUS_FILTER_COUNT] = "{k} / {n}",
    [UI_TEXT_STATUS_SEARCH_FORWARD] = "このノート内を検索 / Enter: 次へ",
    [UI_TEXT_STATUS_SEARCH_BACKWARD] = "このノート内を検索 / Enter: 前へ",
    [UI_TEXT_STATUS_SEARCH_MALFORMED] = "検索できない文字があります",
    [UI_TEXT_STATUS_SEARCH_BAD_SPAN] = "選択の範囲を読めません",
    [UI_TEXT_STATUS_SEARCH_NOT_FOUND] = "見つかりません",
    [UI_TEXT_STATUS_SEARCH_FOUND] = "{k} / {n} 件",
    [UI_TEXT_ACTION_SEARCH_PREVIOUS] = "◀ 前へ",
    [UI_TEXT_ACTION_SEARCH_NEXT] = "次へ ▶",
    [UI_TEXT_STATUS_REPLACE_POSITION] = " 位置 {offset}",
    [UI_TEXT_STATUS_REPLACE_COUNT] = "{name} / {k} 件",
    [UI_TEXT_ACTION_REPLACE_ONE] = "1 件",
    [UI_TEXT_ACTION_REPLACE_ALL] = "すべて",
    [UI_TEXT_STATUS_COMMAND_NOT_FOUND] = "一致する操作がありません。",
    [UI_TEXT_STATUS_PALETTE_CLICK] = "操作をクリックすると実行します。",
    [UI_TEXT_STATUS_PALETTE_FILTER] = "下の欄に名前を入力すると絞り込めます。",
    /* #69（main）が足したヘルプのページ送りの札。変更前の #74 の木には無く、
     * 連結で作っていた「1/2」を置換子に移した（取り込みのときの 1 件）。 */
    [UI_TEXT_STATUS_PALETTE_PAGES] = "{k}/{n}",
    [UI_TEXT_ACTION_KEYS_HIDE] = "キー操作を閉じる",
    [UI_TEXT_ACTION_KEYS_SHOW] = "キー操作を表示",
    [UI_TEXT_ACTION_PALETTE_PREVIOUS] = "↑ 前",
    [UI_TEXT_ACTION_PALETTE_NEXT] = "↓ 次",
    [UI_TEXT_ACTION_OPERATIONS] = "操作 ▾",
    [UI_TEXT_THEME_TITLE] = "テーマを選ぶ",
    [UI_TEXT_THEME_CHOOSE] = "テーマを選ぶ…",
    [UI_TEXT_THEME_FILTER] = "テーマ名で検索",
    [UI_TEXT_THEME_BACK] = "‹ 設定",
    [UI_TEXT_THEME_GUIDANCE] = "↑↓でプレビュー・Enterで確定・Escで戻す",
    [UI_TEXT_THEME_COUNT] = "{k} / {n} 件",
    [UI_TEXT_THEME_EMPTY] = "一致するテーマがありません",
    [UI_TEXT_THEME_SAMPLE] = "Aa",
    [UI_TEXT_SETTINGS_THEME] = "テーマ",
    [UI_TEXT_SETTINGS_THEME_SYSTEM] = "OS に従う",
    [UI_TEXT_SETTINGS_THEME_LIGHT] = "ライト",
    [UI_TEXT_SETTINGS_THEME_DARK] = "ダーク",
    [UI_TEXT_SETTINGS_LANGUAGE] = "言語",
    [UI_TEXT_SETTINGS_LANGUAGE_JA] = "日本語",
    [UI_TEXT_SETTINGS_LANGUAGE_EN] = "English",
    [UI_TEXT_SETTINGS_LANGUAGE_ZH_HANS] = "简体中文",
    [UI_TEXT_SETTINGS_NUMBER] = "行番号",
    [UI_TEXT_SETTINGS_NUMBER_ON] = "表示する",
    [UI_TEXT_SETTINGS_NUMBER_OFF] = "表示しない",
    [UI_TEXT_SETTINGS_GUIDANCE] = "矢印 / Tab 選択 · Enter / Space 採用 · Esc 閉じる",
    [UI_TEXT_SETTINGS_IMMEDIATE] = "変更はすぐに反映",
    [UI_TEXT_SETTINGS_NUMBER_CHECK] = "行番号を表示",
    [UI_TEXT_SETTINGS_VERSION] = "版 {name}",
    [UI_TEXT_HISTORY_GUIDANCE] = "1 が最新 / ↑↓ 選択 / Enter 戻す / Esc 閉じる",
    [UI_TEXT_HISTORY_ROW] = "{n}  {name}",
    [UI_TEXT_HISTORY_ROW_UNREADABLE] = "{n}  （読めません）",
    [UI_TEXT_APP_LOGO] = "NENE FOLIO",
    [UI_TEXT_TITLE_CATEGORIES] = "カテゴリ",
    [UI_TEXT_PROMPT_PENDING_EXPLANATION] =
        "「再試行」で同じ名前変更を続けます。「閉じる」は取り消しではありません。",
    [UI_TEXT_PROMPT_PENDING_RENAME] = "{from} → {to}",
    [UI_TEXT_PROMPT_TITLE_LEAVE] = "保存しますか",
    [UI_TEXT_PROMPT_HINT_LEAVE] =
        "保存しますか。保存しないと内容は失われます。名前の末尾に .md を補います。",
    [UI_TEXT_PROMPT_DISCARD] = "保存しない(&D)",
    [UI_TEXT_PROMPT_TITLE_FIRST_SAVE] = "名前をつけて保存",
    [UI_TEXT_PROMPT_TITLE_SAVE_AS] = "別名で保存",
    [UI_TEXT_PROMPT_TITLE_RENAME] = "名前を変更",
    [UI_TEXT_PROMPT_TITLE_RENAME_CATEGORY] = "カテゴリ名を変更",
    [UI_TEXT_PROMPT_HINT_RENAME_CATEGORY] = "フォルダと履歴を新しい名前へ移します。",
    [UI_TEXT_PROMPT_HINT_RETRY_RENAME] =
        "先にこの名前変更を終えます。いま頼んだ操作はまだ始めていません。",
    [UI_TEXT_PROMPT_TITLE_NEW_CATEGORY] = "新しいカテゴリ",
    [UI_TEXT_PROMPT_HINT_PENDING] = "この名前変更はまだ完了していません。",
    [UI_TEXT_PROMPT_HINT_FIRST_SAVE] = "名前の末尾に .md を補います。",
    [UI_TEXT_PROMPT_HINT_SAVE_AS] = "元の保存内容を保ち、別の .md を作ります。",
    [UI_TEXT_PROMPT_HINT_RENAME] = "md と履歴を新しい名前へ移します。",
    [UI_TEXT_PROMPT_HINT_NEW_CATEGORY] = "data/ にカテゴリのフォルダを作ります。",
    [UI_TEXT_PROMPT_ACCEPT_RETRY] = "再試行",
    [UI_TEXT_PROMPT_ACCEPT_SAVE] = "保存(&S)",
    [UI_TEXT_PROMPT_ACCEPT_RENAME] = "変更",
    [UI_TEXT_PROMPT_ACCEPT_CREATE] = "作成",
    [UI_TEXT_PROMPT_OK] = "OK",
    [UI_TEXT_PROMPT_CLOSE_PENDING] = "閉じる",
    [UI_TEXT_PROMPT_CLOSE_CANCEL] = "キャンセル(&C)",
    [UI_TEXT_PROMPT_LABEL_NAME] = "名前",
    [UI_TEXT_PROMPT_LABEL_CATEGORY] = "保存先",
    [UI_TEXT_APP_NO_MODULE_PATH] = "実行ファイルの場所が長すぎるか、取得できません。",
    [UI_TEXT_APP_DATA_IN_USE] = "同じ data/ を別の NeNe Folio が使っています。先に閉じてください。",
    [UI_TEXT_APP_RECOVERY_LOCKED] = "名前変更の復旧記録（data/.rename.json）がありますが、data/ "
                                    "を占有できないので復旧できません。",
    [UI_TEXT_APP_OUT_OF_MEMORY] = "記憶域が足りません。",
    [UI_TEXT_APP_NO_WINDOW] = "窓を作れませんでした。",
    /* #349 で足した ID（期待表へ手で足した。3 言語は verify_action_bar_texts が見る）。 */
    [UI_TEXT_ACCESSIBLE_OPERATIONS] = "操作",
    [UI_TEXT_ACCESSIBLE_NOTE_ACTIONS] = "ノートの操作",
    [UI_TEXT_ACCESSIBLE_BREADCRUMB] = "{from} / {name}",
    [UI_TEXT_ACCESSIBLE_BREADCRUMB_UNSAVED] = "{from} / {name}（未保存）",
    [UI_TEXT_STATUS_UNSAVED] = "未保存の変更があります",
    [UI_TEXT_STATUS_SAVED] = "保存済み",
    [UI_TEXT_NOTIFY_SAVED] = "保存しました",
    [UI_TEXT_TOOLTIP_HELP] = "ヘルプとコマンド一覧（F1 / Ctrl+P）",
    [UI_TEXT_TOOLTIP_NEW_NOTE] = "新しいノート — {name}へ（{from}）",
    [UI_TEXT_NEW_NOTE_NO_CATEGORY] = "カテゴリがありません。先にカテゴリを作ります",
    [UI_TEXT_EMPTY_HEADING] = "ノートが選ばれていません",
    [UI_TEXT_EMPTY_BODY] = "左の一覧からノートを選ぶか、新しいノートを作ります",
    [UI_TEXT_EMPTY_KEYS] = "すべてのノートを検索: Ctrl+Shift+F ／ ヘルプ: F1",
    [UI_TEXT_COMMAND_CAPTION] = "{name}",
    [UI_TEXT_COMMAND_INPUT_CAPTION] = "{name}…",
    [UI_TEXT_MENU_ACCESS_CAPTION] = "{name} ({from})",
    [UI_TEXT_KEY_SAVE_AS] = "Ctrl+Shift+S",
    [UI_TEXT_KEY_RENAME] = "F2",
    [UI_TEXT_KEY_PALETTE] = "Ctrl+P",
    [UI_TEXT_KEY_FILTER] = "Ctrl+Shift+F",
    [UI_TEXT_KEY_SEARCH_FORWARD] = "F3",
    [UI_TEXT_KEY_SEARCH_BACKWARD] = "Shift+F3",
    [UI_TEXT_KEY_SEARCH_NEXT] = "Enter",
    [UI_TEXT_KEY_SEARCH_PREVIOUS] = "Shift+Enter",
    [UI_TEXT_KEY_SUBSTITUTE] = ":%s",
    [UI_TEXT_FAILURE_COMMAND_NOTE_REQUIRED] = "「{name}」はノートを開くと使えます。",
    [UI_TEXT_NOTIFY_NOTE_REMOVED] = "「{name}」を一覧から除きました。",
    [UI_TEXT_HELP_RECYCLE] = "ごみ箱へ移せたノートは Windows のごみ箱から戻せます。",
    [UI_TEXT_TOOLTIP_HELP_KEYS] = "ヘルプとコマンド一覧（{from} / {to}）",
    [UI_TEXT_MENU_SCROLL_UP] = "上に項目があります",
    [UI_TEXT_MENU_SCROLL_DOWN] = "下に項目があります",
    [UI_TEXT_MENU_LOCAL_KEY] = "{name}（メニュー内）",
    [UI_TEXT_KEY_CONTEXT_MENU] = "Shift+F10",
    [UI_TEXT_KEY_APPS] = "Apps",
    [UI_TEXT_FAILURE_NAME_UNCHANGED] = "同じ名前です。",
    [UI_TEXT_PROMPT_NAME_EMPTY] = "名前を入力してください。",
    [UI_TEXT_PROMPT_TITLE_UNTITLED] = "無題のノート",
    [UI_TEXT_PROMPT_CATEGORY_READONLY] = "{name}（変更できません）",
    [UI_TEXT_FAILURE_COMMAND_REASON] = "{name}\r\n{from}",
    [UI_TEXT_STATUS_REPLACE_COUNT_ONLY] = "{k} 件",
    [UI_TEXT_HELP_TAB_COMMANDS] = "操作",
    [UI_TEXT_HELP_TAB_KEYS] = "キー",
    [UI_TEXT_HELP_GROUP_NOTE] = "ノート",
    [UI_TEXT_HELP_GROUP_CATEGORY] = "カテゴリ",
    [UI_TEXT_HELP_GROUP_LAYOUT] = "表示",
    [UI_TEXT_HELP_GROUP_APP] = "アプリ",
    [UI_TEXT_HELP_GROUP_QUIT] = "終了",
    [UI_TEXT_MENU_EX_HINT] = ":{name}",
    [UI_TEXT_HELP_RENAME_KEY] = "{name} / {from} 名前変更",

};

constexpr size_t expected_count = sizeof expected / sizeof expected[0];
/* 置換子は 6 つだけ（決定 3）。表の全 ID がこの集合に収まることを固定する。 */
static const char *_Nonnull const known_placeholders[] = {"{k}",    "{n}",    "{offset}",
                                                          "{name}", "{from}", "{to}"};

static size_t utf16_units_of(const char *_Nonnull utf8)
{
    char16_t units[ui_text_unit_limit];
    size_t written = 0;
    require(utf16_text_fill(utf8, units, ui_text_unit_limit, &written) == UTF16_TEXT_FILL_READY,
            "every catalog line fits ui_text_unit_limit");
    return written;
}

/* 表の全 ID が、変更前と 1 字も違わないこと（同一性）。 */
static void verify_identity(void)
{
    require(expected_count == UI_TEXT_MENU_EX_HINT + 1,
            "the expected table covers every id in the enumeration");
    for (size_t index = 0; index < expected_count; ++index)
    {
        const char *_Nonnull actual = ui_text_line((enum ui_text)index, FOLIO_LANGUAGE_JA);
        require(same_text(actual, expected[index]), "the catalog line is byte for byte the same");
    }
}

/* 空なのは UI_TEXT_EMPTY だけ。全 ID × 全言語が上限に収まる（ADR 0032 の検証）。 */
static void verify_lines(void)
{
    for (size_t index = 0; index < expected_count; ++index)
    {
        for (size_t column = 0; column < folio_language_count; ++column)
        {
            const char *_Nonnull line =
                ui_text_line((enum ui_text)index, (enum folio_language)column);
            require((index == UI_TEXT_EMPTY) == (line[0] == '\0'),
                    "only UI_TEXT_EMPTY is the empty string, in every column");
            require(utf16_units_of(line) <= ui_text_unit_limit, "the line fits in UTF-16");
        }
    }
}

/* `{` は置換子の始まりにしか現れない（決定 3）。 */
static bool known_at(const char *_Nonnull line, size_t at)
{
    for (size_t index = 0; index < sizeof known_placeholders / sizeof known_placeholders[0];
         ++index)
    {
        size_t width = strlen(known_placeholders[index]);
        if (strncmp(line + at, known_placeholders[index], width) == 0)
        {
            return true;
        }
    }
    return false;
}

static void verify_braces_of(const char *_Nonnull line)
{
    for (size_t at = 0; line[at] != '\0'; ++at)
    {
        require(line[at] != '}' || at > 0, "a closing brace never starts a line");
        require(line[at] != '{' || known_at(line, at),
                "every brace in the catalog starts one of the six placeholders");
    }
}

static void verify_placeholders(void)
{
    for (size_t index = 0; index < expected_count; ++index)
    {
        for (size_t column = 0; column < folio_language_count; ++column)
        {
            verify_braces_of(ui_text_line((enum ui_text)index, (enum folio_language)column));
        }
    }
}

static struct ui_text_request request_for(enum ui_text id)
{
    struct ui_text_request request = {.id = id,
                                      .language = FOLIO_LANGUAGE_JA,
                                      .k = 0,
                                      .n = 0,
                                      .offset = 0,
                                      .name = nullptr,
                                      .from = nullptr,
                                      .to = nullptr};
    return request;
}

static void expect_format(const struct ui_text_request *_Nonnull request,
                          const char *_Nonnull wanted)
{
    char line[512];
    require(ui_text_format(request, line, sizeof line) == UI_TEXT_FORMAT_READY, "the phrase fits");
    require(same_text(line, wanted), "the filled phrase");
}

static void verify_format(void)
{
    /* 置換子の無い句はそのまま写る。 */
    struct ui_text_request plain = request_for(UI_TEXT_STATUS_SEARCH_NOT_FOUND);
    expect_format(&plain, "見つかりません");
    /* 数は 10 進・先頭のゼロ無し・0 は "0"（append_number と同じ規則）。 */
    struct ui_text_request found = request_for(UI_TEXT_STATUS_SEARCH_FOUND);
    found.k = 3;
    found.n = 12;
    expect_format(&found, "3 / 12 件");
    found.k = 0;
    found.n = 0;
    expect_format(&found, "0 / 0 件");
    found.k = 1000000;
    found.n = 4294967296;
    expect_format(&found, "1000000 / 4294967296 件");
    /* NULL の name は空文字列。 */
    struct ui_text_request count = request_for(UI_TEXT_STATUS_REPLACE_COUNT);
    count.k = 2;
    expect_format(&count, " / 2 件");
    count.name = "覚え書き";
    expect_format(&count, "覚え書き / 2 件");
    /* 埋めた内容は再走査しない: `{k}` という名前でも壊れない。 */
    count.name = "{k}";
    expect_format(&count, "{k} / 2 件");
    /* 同じ句に置換子が 2 回（{from} と {to}）。 */
    struct ui_text_request renamed = request_for(UI_TEXT_PROMPT_PENDING_RENAME);
    renamed.from = "旧";
    renamed.to = "新";
    expect_format(&renamed, "旧 → 新");
    /* 位置の句は先頭のスペース込み。 */
    struct ui_text_request position = request_for(UI_TEXT_STATUS_REPLACE_POSITION);
    position.offset = 7;
    expect_format(&position, " 位置 7");
}

static void verify_format_limits(void)
{
    struct ui_text_request found = request_for(UI_TEXT_STATUS_SEARCH_FOUND);
    found.k = 3;
    found.n = 12;
    char exact[11]; /* "3 / 12 件" は 10 バイト＋終端 */
    require(ui_text_format(&found, exact, sizeof exact) == UI_TEXT_FORMAT_READY,
            "a phrase that fits exactly is written");
    require(same_text(exact, "3 / 12 件"), "the phrase that fits exactly");
    char tight[10];
    require(ui_text_format(&found, tight, sizeof tight) == UI_TEXT_FORMAT_TOO_LONG,
            "one byte short is refused");
    require(tight[0] == '\0', "a refused phrase leaves the empty string");
    char none[1];
    require(ui_text_format(&found, none, sizeof none) == UI_TEXT_FORMAT_TOO_LONG,
            "only the terminator does not fit either");
    require(none[0] == '\0', "the empty string still fits");
    /* ノート名は 255 バイトまで来るので、実行時に溢れうる（決定 3）。 */
    struct ui_text_request count = request_for(UI_TEXT_STATUS_REPLACE_COUNT);
    static char long_name[300];
    for (size_t index = 0; index + 1 < sizeof long_name; ++index)
    {
        long_name[index] = 'a';
    }
    count.name = long_name;
    char room[64];
    require(ui_text_format(&count, room, sizeof room) == UI_TEXT_FORMAT_TOO_LONG,
            "a long note name overflows the caller's buffer");
    require(room[0] == '\0', "and leaves the empty string");
}

static void verify_fill(void)
{
    char16_t units[8];
    size_t written = 99;
    require(utf16_text_fill("", units, 8, &written) == UTF16_TEXT_FILL_READY, "the empty string");
    require(written == 0 && units[0] == u'\0', "writes only the terminator");
    require(utf16_text_fill("abc", units, 4, &written) == UTF16_TEXT_FILL_READY, "exactly fits");
    require(written == 3 && units[3] == u'\0', "three units and a terminator");
    written = 99;
    require(utf16_text_fill("abcd", units, 4, &written) == UTF16_TEXT_FILL_TOO_LONG,
            "one unit over is refused");
    require(written == 99, "a refused fill does not touch written");
    /* サロゲートペア（U+1F600）は 2 単位。 */
    require(utf16_text_fill("\xF0\x9F\x98\x80", units, 8, &written) == UTF16_TEXT_FILL_READY,
            "a supplementary code point");
    require(written == 2 && units[0] == 0xD83D && units[1] == 0xDE00, "is a surrogate pair");
    require(utf16_text_fill("\xF0\x9F\x98\x80", units, 2, &written) == UTF16_TEXT_FILL_TOO_LONG,
            "and needs room for both units");
    written = 99;
    require(utf16_text_fill("\xC3", units, 8, &written) == UTF16_TEXT_FILL_MALFORMED,
            "a truncated sequence is refused");
    require(written == 99, "a malformed fill does not touch written");
}

/* 訳してはいけない鍵の名前（ADR 0032 の決定 3 の一覧）。語順や区切りは訳してよいが、
 * この綴りが ja の行にあれば en と zh-Hans の同じ行にも無ければならない。 */
static const char *_Nonnull const key_names[] = {
    "Ctrl+h/j/k/l",
    "Ctrl+P",
    "Ctrl+F",
    "Ctrl+Shift+F",
    "Ctrl+N",
    "Ctrl+S",
    "Ctrl+Shift+S",
    "Ctrl+Enter",
    "F1",
    "F2",
    "F3",
    "Shift+F3",
    "Shift+Enter",
    "PgUp",
    "PgDn",
    "Esc",
    "Enter",
    "Tab",
    "gg",
    "G",
    "j",
    "k",
    "h",
    "l",
    "n",
    "N",
    "i",
    ":",
    "/",
    "?",
    "↑",
    "↓",
    "←",
    "→",
    "↑↓",
};

/* `:set` の語。folio_command.c の options[] が正本なので、綴りは parse で確かめてから使う
 * （表から語を消すとこの単体が先に落ちる）。 */
static const char *_Nonnull const option_words[] = {
    "number",       "nu",
    "nonumber",     "nonu",
    "number!",      "nu!",
    "invnumber",    "invnu",
    "theme=system", "theme=light",
    "theme=dark",   "language=ja",
    "language=en",  "language=zh-Hans",
};

static bool holds(const char *_Nonnull line, const char *_Nonnull word)
{
    return strstr(line, word) != nullptr;
}

/* ja の行にある語彙は、同じ行の en と zh-Hans にも現れる（決定 3）。 */
static void expect_vocabulary(enum ui_text id, const char *_Nonnull word)
{
    if (!holds(ui_text_line(id, FOLIO_LANGUAGE_JA), word))
    {
        return;
    }
    require(holds(ui_text_line(id, FOLIO_LANGUAGE_EN), word),
            "a word of the closed vocabulary survives into English");
    require(holds(ui_text_line(id, FOLIO_LANGUAGE_ZH_HANS), word),
            "a word of the closed vocabulary survives into simplified Chinese");
}

static void verify_vocabulary_of(enum ui_text id)
{
    for (size_t index = 0; index < sizeof key_names / sizeof key_names[0]; ++index)
    {
        expect_vocabulary(id, key_names[index]);
    }
    for (size_t index = 0; index < sizeof option_words / sizeof option_words[0]; ++index)
    {
        expect_vocabulary(id, option_words[index]);
    }
    for (size_t item = 0; item < folio_command_count(); ++item)
    {
        enum folio_command command = folio_command_at(item);
        for (size_t alias = 0; alias < folio_command_alias_count(command); ++alias)
        {
            expect_vocabulary(id, folio_command_alias(command, alias));
        }
    }
}

/* 表は言語ごとの列を持ち、閉じた語彙は訳さない（ADR 0032 の決定 2・3）。 */
static void verify_language(void)
{
    require(folio_language_count == 3, "the catalog has three columns");
    require(strcmp(ui_text_line(UI_TEXT_COMMAND_SAVE, FOLIO_LANGUAGE_EN),
                   ui_text_line(UI_TEXT_COMMAND_SAVE, FOLIO_LANGUAGE_JA)) != 0,
            "the English column is a different line");
    /* 言語の名前は各言語の自称なので 3 列とも同じである（決定 7）。 */
    for (size_t column = 0; column < folio_language_count; ++column)
    {
        require(same_text(ui_text_line(UI_TEXT_SETTINGS_LANGUAGE_JA, (enum folio_language)column),
                          ui_text_line(UI_TEXT_SETTINGS_LANGUAGE_JA, FOLIO_LANGUAGE_JA)),
                "the endonym does not change with the column");
    }
    /* `:set` の語は folio_command が解けるものだけを語彙に置く。 */
    for (size_t index = 0; index < sizeof option_words / sizeof option_words[0]; ++index)
    {
        enum folio_option option = FOLIO_OPTION_NUMBER_SHOW;
        enum folio_theme_choice theme = FOLIO_THEME_CHOICE_SYSTEM;
        size_t length = strlen(option_words[index]);
        require(folio_command_parse_option(option_words[index], length, &option) ||
                    folio_command_parse_theme(option_words[index], length, &theme),
                "every word of the vocabulary is a word folio_command knows");
    }
    for (size_t id = UI_TEXT_COMMAND_SAVE; id <= UI_TEXT_COMMAND_DISCARD_EDITS; ++id)
    {
        verify_vocabulary_of((enum ui_text)id);
    }
    for (size_t id = UI_TEXT_HELP_PALETTE; id <= UI_TEXT_HELP_EDITOR_ESCAPE; ++id)
    {
        verify_vocabulary_of((enum ui_text)id);
    }
    verify_vocabulary_of(UI_TEXT_SETTINGS_GUIDANCE);
    /* 入力後も残る対象表示は、どの言語でもEnterの動きを明記する（ADR0052）。 */
    verify_vocabulary_of(UI_TEXT_STATUS_SEARCH_FORWARD);
    verify_vocabulary_of(UI_TEXT_STATUS_SEARCH_BACKWARD);
}

static void verify_trash_notice(void)
{
    const char *const lines[] = {"一覧から除きました。", "Removed from the list.",
                                 "已从列表移除。"};
    for (size_t column = 0; column < folio_language_count; ++column)
    {
        require(same_text(ui_text_line(UI_TEXT_TRASH_NOTE_COMPLETED, (enum folio_language)column),
                          lines[column]),
                "the notice reports removal from the list without asserting a move");
    }
    verify_vocabulary_of(UI_TEXT_HELP_TRASH_NOTE);
}

static void verify_opened_in_view_notice(void)
{
    const char *const lines[] = {"閲覧で開きました。編集で保てない文字があります。",
                                 "Opened in view mode. It has characters the editor cannot keep.",
                                 "已以查看模式打开。正文含有编辑时无法保留的字符。"};
    for (size_t column = 0; column < folio_language_count; ++column)
    {
        require(same_text(ui_text_line(UI_TEXT_NOTE_OPENED_IN_VIEW, (enum folio_language)column),
                          lines[column]),
                "the notice states the note opened in view mode and why");
    }
}

static void verify_category_not_selected(void)
{
    const char *const lines[] = {"カテゴリを選んでください。", "Select a category.",
                                 "请选择分类。"};
    for (size_t column = 0; column < folio_language_count; ++column)
    {
        require(same_text(ui_text_line(UI_TEXT_FAILURE_CATEGORY_NOT_SELECTED,
                                       (enum folio_language)column),
                          lines[column]),
                "the targetless category operation asks for a category in each language");
    }
    verify_vocabulary_of(UI_TEXT_HELP_DELETE_CATEGORY);
    verify_vocabulary_of(UI_TEXT_HELP_ACCESSIBILITY);
}

static void verify_rename_vocabulary(void)
{
    const enum ui_text ids[] = {UI_TEXT_TITLE_CATEGORY_RECOVERING, UI_TEXT_FAILURE_REPLACE_STALE,
                                UI_TEXT_FAILURE_RENAME_PENDING,
                                UI_TEXT_FAILURE_RENAME_JOURNAL_BROKEN};
    const char *const lines[][folio_language_count] = {
        {"復旧待ち", "Recovering", "等待恢复"},
        {"下見が古くなりました。もう一度入力してください。",
         "The preview is out of date. Type again.", "预览已过期。请重新输入。"},
        {"名前の変更が途中で止まっています。「名前を変更」から再試行してください。",
         "A rename stopped halfway. Open Rename to retry.", "重命名中途停止。请打开“重命名”重试。"},
        {"名前変更の記録（data/.rename.json）の版か形を読めません。記録は残しています。",
         "The rename record (data/.rename.json) has an unreadable version or format. The record "
         "was kept.",
         "无法读取重命名记录（data/.rename.json）的版本或形式。记录已保留。"},
    };
    for (size_t row = 0; row < sizeof ids / sizeof ids[0]; ++row)
    {
        for (size_t column = 0; column < folio_language_count; ++column)
        {
            require(
                same_text(ui_text_line(ids[row], (enum folio_language)column), lines[row][column]),
                "rename/STALE wording describes both kinds in all three languages");
        }
    }
}

static void verify_product_version(void)
{
    const char *const expected_lines[] = {"版 65535.65535.65535.65535",
                                          "Version 65535.65535.65535.65535",
                                          "版本 65535.65535.65535.65535"};
    for (size_t column = 0; column < folio_language_count; ++column)
    {
        struct ui_text_request request = {.id = UI_TEXT_SETTINGS_VERSION,
                                          .language = (enum folio_language)column,
                                          .name = "65535.65535.65535.65535"};
        char line[64];
        require(ui_text_format(&request, line, sizeof line) == UI_TEXT_FORMAT_READY,
                "the maximum Windows version fits the settings display");
        require(same_text(line, expected_lines[column]), "the product version label is localized");
    }
}

/* #349: 本文操作帯・保存状態・空状態の文言を 3 言語で固定し、組み立ての完成形を見る。 */
static void verify_action_bar_texts(void)
{
    const enum ui_text ids[] = {UI_TEXT_TITLE_UNTITLED,
                                UI_TEXT_CHIP_VIEW,
                                UI_TEXT_ACCESSIBLE_OPERATIONS,
                                UI_TEXT_ACCESSIBLE_NOTE_ACTIONS,
                                UI_TEXT_STATUS_UNSAVED,
                                UI_TEXT_STATUS_SAVED,
                                UI_TEXT_NOTIFY_SAVED,
                                UI_TEXT_TOOLTIP_HELP,
                                UI_TEXT_NEW_NOTE_NO_CATEGORY,
                                UI_TEXT_EMPTY_HEADING,
                                UI_TEXT_EMPTY_BODY,
                                UI_TEXT_EMPTY_KEYS};
    const char *const lines[][folio_language_count] = {
        {"無題", "Untitled", "无标题"},
        {"閲覧", "Read", "查看"},
        {"操作", "Actions", "操作"},
        {"ノートの操作", "Note actions", "笔记操作"},
        {"未保存の変更があります", "Unsaved changes", "有未保存的更改"},
        {"保存済み", "Saved", "已保存"},
        {"保存しました", "Saved.", "已保存。"},
        {"ヘルプとコマンド一覧（F1 / Ctrl+P）", "Help and command list (F1 / Ctrl+P)",
         "帮助和命令列表（F1 / Ctrl+P）"},
        {"カテゴリがありません。先にカテゴリを作ります",
         "There are no categories. Create a category first", "没有分类。请先创建分类"},
        {"ノートが選ばれていません", "No note is selected", "未选择笔记"},
        {"左の一覧からノートを選ぶか、新しいノートを作ります",
         "Choose a note from the list on the left, or create a new note",
         "从左侧列表选择笔记，或新建笔记"},
        {"すべてのノートを検索: Ctrl+Shift+F ／ ヘルプ: F1",
         "Search all notes: Ctrl+Shift+F / Help: F1", "搜索全部笔记: Ctrl+Shift+F ／ 帮助: F1"},
    };
    for (size_t row = 0; row < sizeof ids / sizeof ids[0]; ++row)
    {
        for (size_t column = 0; column < folio_language_count; ++column)
        {
            require(
                same_text(ui_text_line(ids[row], (enum folio_language)column), lines[row][column]),
                "the action bar and save state wording is fixed in all three languages");
        }
    }
}

/* 組み立ての完成形と、外した語（題の「未保存」と UIA 名の▾）。 */
static void verify_action_bar_formats(void)
{
    /* 無題の印は丸と UIA 名が持つので、題には「未保存」を書かない（ADR0060 の決定 8）。 */
    require(!holds(ui_text_line(UI_TEXT_TITLE_UNTITLED, FOLIO_LANGUAGE_JA), "未保存"),
            "the untitled title no longer carries the unsaved word");
    /* ▾は表示だけの記号で、UIA 名の元には入れない。 */
    require(!holds(ui_text_line(UI_TEXT_ACCESSIBLE_OPERATIONS, FOLIO_LANGUAGE_EN), "▾"),
            "the operations name has no chevron");
    const char *const tooltips[] = {"新しいノート — 仕事へ（Ctrl+N）",
                                    "New note — in 仕事 (Ctrl+N)", "新建笔记 — 放入仕事（Ctrl+N）"};
    const char *const crumbs[] = {"仕事 / メモ（未保存）", "仕事 / メモ (unsaved)",
                                  "仕事 / メモ（未保存）"};
    for (size_t column = 0; column < folio_language_count; ++column)
    {
        struct ui_text_request request = {.id = UI_TEXT_TOOLTIP_NEW_NOTE,
                                          .language = (enum folio_language)column,
                                          .name = "仕事",
                                          .from = "Ctrl+N"};
        char line[256];
        require(ui_text_format(&request, line, sizeof line) == UI_TEXT_FORMAT_READY &&
                    same_text(line, tooltips[column]),
                "the new note tooltip names the destination category and the key");
        request = (struct ui_text_request){.id = UI_TEXT_ACCESSIBLE_BREADCRUMB_UNSAVED,
                                           .language = (enum folio_language)column,
                                           .name = "メモ",
                                           .from = "仕事"};
        require(ui_text_format(&request, line, sizeof line) == UI_TEXT_FORMAT_READY &&
                    same_text(line, crumbs[column]),
                "the breadcrumb name carries the unsaved word only in the unsaved form");
        request.id = UI_TEXT_ACCESSIBLE_BREADCRUMB;
        require(ui_text_format(&request, line, sizeof line) == UI_TEXT_FORMAT_READY &&
                    same_text(line, "仕事 / メモ"),
                "the saved breadcrumb name is the category and the note");
    }
}

/* 帯の件数は長い対象名を複製せず、箱とreadonly欄は語を欠落させない。 */
static void verify_surface_formats(void)
{
    require(same_text(ui_text_line(UI_TEXT_ACCESSIBLE_PATTERN, FOLIO_LANGUAGE_EN), "Regex pattern"),
            "the persistent replacement label explicitly says that the pattern is a regex");
    const char *const counts[] = {"12 件", "12 matches", "12 处"};
    const char *const categories[] = {"仕事（変更できません）", "仕事 (cannot be changed)",
                                      "仕事（无法更改）"};
    for (size_t column = 0; column < folio_language_count; ++column)
    {
        struct ui_text_request request = {.id = UI_TEXT_STATUS_REPLACE_COUNT_ONLY,
                                          .language = (enum folio_language)column,
                                          .k = 12,
                                          .name = "an unrelated long target name"};
        expect_format(&request, counts[column]);
        request = (struct ui_text_request){.id = UI_TEXT_PROMPT_CATEGORY_READONLY,
                                           .language = (enum folio_language)column,
                                           .name = "仕事"};
        expect_format(&request, categories[column]);
        request = (struct ui_text_request){.id = UI_TEXT_FAILURE_COMMAND_REASON,
                                           .language = (enum folio_language)column,
                                           .name = "operation",
                                           .from = "reason"};
        expect_format(&request, "operation\r\nreason");
    }
}

/* 3言語のnative captionが同じS/D/Cを一意に持つ。global shortcutを増やす表ではない。 */
static void verify_prompt_mnemonics(void)
{
    const enum ui_text ids[] = {UI_TEXT_PROMPT_ACCEPT_SAVE, UI_TEXT_PROMPT_DISCARD,
                                UI_TEXT_PROMPT_CLOSE_CANCEL};
    const char keys[] = {'S', 'D', 'C'};
    for (size_t column = 0; column < folio_language_count; ++column)
    {
        for (size_t index = 0; index < sizeof ids / sizeof ids[0]; ++index)
        {
            const char *_Nonnull line = ui_text_line(ids[index], (enum folio_language)column);
            const char *_Nullable marker = strchr(line, '&');
            require(marker != nullptr && marker[1] == keys[index] &&
                        strchr(marker + 1, '&') == nullptr,
                    "native prompt captions have one matching S/D/C mnemonic in every language");
        }
    }
}

void run_ui_text_tests(void)
{
    verify_surface_formats();
    verify_prompt_mnemonics();
    verify_action_bar_texts();
    verify_action_bar_formats();
    verify_identity();
    verify_lines();
    verify_placeholders();
    verify_format();
    verify_format_limits();
    verify_fill();
    verify_language();
    verify_trash_notice();
    verify_opened_in_view_notice();
    verify_category_not_selected();
    verify_rename_vocabulary();
    verify_product_version();
}
