#include "ui_text.h"

#include "ui_text_request.h"

#include <string.h>

/* 利用者に見える文言の唯一の表（ADR 0030 の決定 1・ADR 0032 の決定 2）。
 * 行が ID・列が言語（ja / en / zh-Hans の順で、正本は enum folio_language の並び）。
 * 日本語の列は #74 の前のソースから機械的に写したもので、**この単位で字面が変わるのは
 * 構文の見本を ASCII に揃えた 2 件だけ**（UI_TEXT_HELP_EX_SUBSTITUTE と
 * UI_TEXT_COMMAND_SUBSTITUTE のパターン／置換）である。
 * 1 字でも変えるときは同じコミットで tests/unit/ui_text_tests.c の期待表も変える。
 * 列の網羅（訳し忘れと空の列）は CNF-011 が字句で守る。**简体中文は母語話者の確認を
 * 得ていない**（ADR 0032 の決定 3。後日この 1 列だけを直せる）。 */
static const char *_Nonnull const catalog[][folio_language_count] = {
    [UI_TEXT_EMPTY] = {"", "", ""},
    [UI_TEXT_FAILURE_DATA_UNREADABLE] = {"data/ を読めませんでした。", "Could not read data/.",
                                         "无法读取 data/。"},
    [UI_TEXT_FAILURE_LEDGER_MALFORMED] =
        {"data/ の台帳（categories.json / index.json）が版 1 の形ではありません。",
         "The ledgers in data/ (categories.json / index.json) are not in the version 1 format.",
         "data/ 的台账（categories.json / index.json）不是版本 1 的形式。"},
    [UI_TEXT_FAILURE_STORE_FAILED] =
        {"data/ の台帳（categories.json / "
         "index.json）に書き戻せませんでした。表示は変えていません。",
         "Could not write back to the ledgers in data/ (categories.json / index.json). "
         "The display is unchanged.",
         "无法写回 data/ 的台账（categories.json / index.json）。显示未改变。"},
    [UI_TEXT_FAILURE_NO_SUCH_CATEGORY] = {"索引に無いカテゴリが操作されました。",
                                          "A category that is not in the index was used.",
                                          "操作了不在索引中的分类。"},
    [UI_TEXT_FAILURE_NO_SUCH_NOTE] = {"索引に無いノートが操作されました。",
                                      "A note that is not in the index was used.",
                                      "操作了不在索引中的笔记。"},
    [UI_TEXT_FAILURE_NOTE_UNREADABLE] = {"ノートを読めませんでした。表示は変えていません。",
                                         "Could not read the note. The display is unchanged.",
                                         "无法读取笔记。显示未改变。"},
    [UI_TEXT_FAILURE_NOTHING_SELECTED] = {"ノートを選んでから編集してください。",
                                          "Select a note before editing.", "请先选择笔记再编辑。"},
    [UI_TEXT_FAILURE_NOT_EDITING] = {"編集モードではありません。", "Not in edit mode.",
                                     "当前不是编辑模式。"},
    [UI_TEXT_FAILURE_NOTE_MALFORMED] =
        {"編集中の本文に壊れた文字があります。保存していません。",
         "The body being edited has broken characters. It was not saved.",
         "编辑中的正文含有损坏的字符。未保存。"},
    [UI_TEXT_FAILURE_NOTE_STORE_FAILED] =
        {"ノートを書き戻せませんでした。編集中の本文はそのままです。"
         "別名で保存するか、編集を破棄して読み直せます。",
         "Could not write the note back. The body being edited is kept. "
         "Save it under another name, or discard the edits and reload.",
         "无法写回笔记。编辑中的正文保持不变。可另存为其他名称，或放弃编辑并重新载入。"},
    [UI_TEXT_FAILURE_HISTORY_FAILED] =
        {"履歴を書けなかったので保存していません。編集中の本文は残っています。"
         "編集を破棄して読み直せます。",
         "The history could not be written, so nothing was saved. The body being edited is kept. "
         "Discard the edits and reload.",
         "无法写入历史，因此未保存。编辑中的正文仍然保留。可放弃编辑并重新载入。"},
    [UI_TEXT_FAILURE_UNSAVED_CHANGES] =
        {"未保存の変更があります。保存するか、未保存変更を破棄して終了してください。",
         "There are unsaved changes. Save, or discard them and quit.",
         "有未保存的更改。请保存，或放弃未保存的更改后退出。"},
    [UI_TEXT_FAILURE_NAME_TAKEN] =
        {"同じ名前のノートがあります。別の名前を指定してください。既存ファイルは変更していません。",
         "A note with that name exists. Give another name. The existing file is unchanged.",
         "已存在同名笔记。请指定其他名称。现有文件未改变。"},
    [UI_TEXT_FAILURE_LEDGER_STALE] =
        {"mdは反映しましたが、台帳（index."
         "json）を書き戻せませんでした。保存を再試行するか、次回の起動で揃います。",
         "The md was written, but the ledger (index.json) could not be written back. "
         "Retry the save, or the next start will align them.",
         "md 已写入，但无法写回台账（index.json）。请重试保存；否则将在下次启动时自动对齐。"},
    [UI_TEXT_FAILURE_LEDGER_UNSYNCED] =
        {"前回の台帳（index."
         "json）をまだ書き戻せていません。今回の操作は行っていないので、保存を再試行してください。",
         "The previous ledger (index.json) has not been written back yet. "
         "This action was not run; retry the save.",
         "上次的台账（index.json）尚未写回。本次操作未执行，请重试保存。"},
    [UI_TEXT_FAILURE_RENAME_PENDING] =
        {"名前の変更が途中で止まっています。同じ名前変更をやり直してください。",
         "A rename stopped halfway. Retry the same rename.",
         "重命名中途停止。请重做同一次重命名。"},
    [UI_TEXT_FAILURE_RENAME_UNLOCKED] =
        {"data/ に書けないため名前を変更できません。何も変えていません。",
         "Cannot rename because data/ is not writable. Nothing was changed.",
         "因无法写入 data/，不能重命名。未做任何改变。"},
    [UI_TEXT_FAILURE_RENAME_UNSUPPORTED] =
        {"この data/ ではノート名を変更できません（ローカルの "
         "NTFS 以外、またはシンボリックリンク／junction）。",
         "Note names cannot be changed in this data/ (not local NTFS, "
         "or a symbolic link / junction).",
         "在此 data/ 中无法更改笔记名称（非本地 NTFS，或为符号链接／junction）。"},
    [UI_TEXT_FAILURE_RENAME_IDENTITY_FAILED] =
        {"元のファイルを確かめられないので名前を変更できません。何も変えていません。",
         "Cannot verify the original file, so the rename did not run. Nothing was changed.",
         "无法确认原文件，因此不能重命名。未做任何改变。"},
    [UI_TEXT_FAILURE_RENAME_JOURNAL_FAILED] =
        {"名前変更の記録（data/.rename.json）を書けませんでした。何も変えていません。",
         "Could not write the rename record (data/.rename.json). Nothing was changed.",
         "无法写入重命名记录（data/.rename.json）。未做任何改变。"},
    [UI_TEXT_FAILURE_RENAME_JOURNAL_BROKEN] =
        {"名前変更の記録（data/.rename.json）が版 1 の形ではありません。消していません。",
         "The rename record (data/.rename.json) is not in the version 1 format. It was not "
         "removed.",
         "重命名记录（data/.rename.json）不是版本 1 的形式。未删除。"},
    [UI_TEXT_FAILURE_RENAME_HALTED] =
        {"名前変更の記録と実ファイルが一致しません。data/.rename.json "
         "と data/<カテゴリ>/ を確認してください。",
         "The rename record and the actual files do not match. "
         "Check data/.rename.json and data/<category>/.",
         "重命名记录与实际文件不一致。请检查 data/.rename.json 与 data/<分类>/。"},
    [UI_TEXT_FAILURE_SEARCH_MALFORMED] = {"検索する語に壊れた文字があります。語は前のままです。",
                                          "The search term has broken characters. "
                                          "The term is unchanged.",
                                          "查找词中有损坏的字符。查找词保持不变。"},
    [UI_TEXT_FAILURE_FILTERED] = {"絞り込み中は並び替えと開閉ができません。",
                                  "Reorder and fold cannot run while filtering.",
                                  "筛选中不能排序与折叠展开。"},
    [UI_TEXT_FAILURE_SETTINGS_UNREADABLE] =
        {"設定（data/settings.json）を読めません。既定値で始め、直すまで上書きしません。",
         "Cannot read data/settings.json. Using defaults without overwriting it.",
         "无法读取设置（data/settings.json）。以默认值启动，且不会覆盖。"},
    [UI_TEXT_FAILURE_SETTINGS_STORE_FAILED] =
        {"設定（data/settings.json）を書けませんでした。設定は変えていません。",
         "Could not write data/settings.json. The settings are unchanged.",
         "无法写入设置（data/settings.json）。设置未改变。"},
    [UI_TEXT_FAILURE_PANE_UNAVAILABLE] = {"表示中の本文を取り出せませんでした。探していません。",
                                          "Could not read the displayed body. "
                                          "Nothing was searched.",
                                          "无法取出正在显示的正文。未进行查找。"},
    [UI_TEXT_FAILURE_REPLACE_NO_PATTERN] = {"置換するパターンを入れてください。",
                                            "Enter a pattern to replace.", "请输入要替换的模式。"},
    [UI_TEXT_FAILURE_REPLACE_BAD_PATTERN] = {"正規表現の書き方が違います。",
                                             "The regular expression is not valid.",
                                             "正则表达式的写法有误。"},
    [UI_TEXT_FAILURE_REPLACE_BAD_TEMPLATE] =
        {"置換後の文字列の書き方が違います。使えるのは & \\0〜\\9 \\r \\n \\\\ \\& \\/ です。",
         "The replacement text is not valid. Allowed: & \\0-\\9 \\r \\n \\\\ \\& \\/",
         "替换文本的写法有误。可用：& \\0～\\9 \\r \\n \\\\ \\& \\/"},
    [UI_TEXT_FAILURE_REPLACE_TIMED_OUT] =
        {"このパターンは時間がかかりすぎるので止めました。本文は変えていません。",
         "That pattern took too long, so it was stopped. The body is unchanged.",
         "该模式耗时过长，已停止。正文未改变。"},
    [UI_TEXT_FAILURE_REPLACE_TOO_COMPLEX] =
        {"このパターンは複雑すぎて当てられません。本文は変えていません。",
         "That pattern is too complex to apply. The body is unchanged.",
         "该模式过于复杂，无法应用。正文未改变。"},
    [UI_TEXT_FAILURE_REPLACE_TOO_MANY] = {"一致が多すぎます。パターンを狭めてください。",
                                          "Too many matches. Narrow the pattern.",
                                          "匹配过多。请缩小模式范围。"},
    [UI_TEXT_FAILURE_REPLACE_TOO_LARGE] = {"置き換えた本文が大きすぎます。本文は変えていません。",
                                           "The replaced body is too large. "
                                           "The body is unchanged.",
                                           "替换后的正文过大。正文未改变。"},
    [UI_TEXT_FAILURE_REPLACE_STALE] =
        {"本文か入力が変わったので、この置換は当てられません。もう一度入力してください。",
         "The body or input changed, so this replace cannot apply. Type again.",
         "正文或输入已改变，无法应用此次替换。请重新输入。"},
    [UI_TEXT_FAILURE_REPLACE_BAD_SPAN] = {"選択範囲が正しくありません。本文は変えていません。",
                                          "The selection range is not valid. "
                                          "The body is unchanged.",
                                          "选择范围不正确。正文未改变。"},
    [UI_TEXT_FAILURE_OUT_OF_MEMORY] = {"記憶域が足りません。", "Out of memory.", "内存不足。"},
    [UI_TEXT_FAILURE_NAME_REQUIRED] =
        {"無題のノートに名前をつけて保存してください。本文は残っています。",
         "Give the untitled note a name and save it. The body is kept.",
         "请为无标题笔记命名后保存。正文仍然保留。"},
    [UI_TEXT_FAILURE_INVALID_NAME] =
        {"使えない名前です。予約名・末尾の空白やピリオド・区切りを避け"
         "、.mdを含め255バイト以内で指定してください。",
         "That name cannot be used. Avoid reserved names, trailing spaces or periods, "
         "and separators; keep it within 255 bytes including .md.",
         "该名称不可用。请避开保留名、末尾的空格或句点以及分隔符，并将名称控制在包含 .md 的 255 "
         "字节以内。"},
    [UI_TEXT_FAILURE_ALREADY_NAMED] =
        {"このノートには名前があります。別名保存（:"
         "saveas）または名前変更（:rename）を使ってください。",
         "This note already has a name. "
         "Use Save as (:saveas) or Rename (:rename).",
         "此笔记已有名称。请使用另存为（:saveas）或重命名（:rename）。"},
    [UI_TEXT_COMMAND_SAVE] = {"保存", "Save", "保存"},
    [UI_TEXT_COMMAND_QUIT] = {"保存済みなら終了", "Quit if saved", "已保存则退出"},
    [UI_TEXT_COMMAND_SAVE_QUIT] = {"保存して終了", "Save and quit", "保存并退出"},
    [UI_TEXT_COMMAND_FORCE_QUIT] = {"未保存変更を破棄して終了", "Discard changes and quit",
                                    "放弃未保存的更改并退出"},
    [UI_TEXT_COMMAND_HELP] = {"ヘルプ", "Help", "帮助"},
    [UI_TEXT_COMMAND_EDIT] = {"編集", "Edit", "编辑"},
    [UI_TEXT_COMMAND_VIEW] = {"保存して閲覧", "Save and view", "保存并查看"},
    [UI_TEXT_COMMAND_NEW] = {"新しいノート", "New note", "新建笔记"},
    [UI_TEXT_COMMAND_SAVE_AS] = {"別名で保存", "Save as", "另存为"},
    [UI_TEXT_COMMAND_RENAME] = {"名前を変更", "Rename", "重命名"},
    [UI_TEXT_COMMAND_FIND] = {"このノート内を検索", "Find in this note", "在本笔记中查找"},
    [UI_TEXT_COMMAND_SET] = {"設定を変える（:set number）", "Change a setting (:set number)",
                             "更改设置（:set number）"},
    [UI_TEXT_COMMAND_TOGGLE_NUMBER] = {"行番号の表示を切り替える", "Toggle line numbers",
                                       "切换行号显示"},
    [UI_TEXT_COMMAND_REPLACE] = {"置換", "Replace", "替换"},
    [UI_TEXT_COMMAND_SUBSTITUTE] = {"正規表現で置換（:%s/pattern/replacement/g）",
                                    "Replace with regex (:%s/pattern/replacement/g)",
                                    "用正则替换（:%s/pattern/replacement/g）"},
    [UI_TEXT_COMMAND_SETTINGS] = {"設定", "Settings", "设置"},
    [UI_TEXT_COMMAND_DISCARD_EDITS] = {"編集を破棄して読み直す", "Discard edits and reload",
                                       "放弃编辑并重新载入"},
    [UI_TEXT_HELP_PALETTE] = {"一覧  ↑↓ 選択 / Enter 実行 / Esc 戻る / Tab 説明",
                              "List  ↑↓ Select / Enter Run / Esc Back / Tab Keys",
                              "列表  ↑↓ 选择 / Enter 执行 / Esc 返回 / Tab 按键"},
    [UI_TEXT_HELP_EDITOR_MOTION] = {"編集本文  Ctrl+h/j/k/l ←/↓/↑/→",
                                    "Editor  Ctrl+h/j/k/l ←/↓/↑/→",
                                    "编辑正文  Ctrl+h/j/k/l ←/↓/↑/→"},
    [UI_TEXT_HELP_GLOBAL_KEYS] = {"全区画  F1 ヘルプ / Ctrl+P 一覧 / Ctrl+F このノート内を検索",
                                  "Anywhere  F1 Help / Ctrl+P List / Ctrl+F Find in this note",
                                  "所有区域  F1 帮助 / Ctrl+P 列表 / Ctrl+F 在本笔记中查找"},
    [UI_TEXT_HELP_GLOBAL_FILTER] = {"全区画  Ctrl+Shift+F すべてのノートを検索（索引を絞り込む）",
                                    "Anywhere  Ctrl+Shift+F Search all notes (filters the index)",
                                    "所有区域  Ctrl+Shift+F 搜索全部笔记（筛选索引）"},
    [UI_TEXT_HELP_FILTER_FIELD] =
        {"絞り込み欄  Esc・Enter 索引へ戻る（絞り込みは残る）/ 空にすると解除",
         "Filter box  Esc, Enter Back to index (filter stays) / Empty clears",
         "筛选栏  Esc、Enter 返回索引（保留筛选）/ 清空则取消"},
    [UI_TEXT_HELP_FILTER_LIMITS] =
        {"絞り込み中  並び替えと折畳/展開はできない（色・保存・編集は可）",
         "While filtering  No reorder or fold/unfold (color, save, edit are OK)",
         "筛选中  不能排序与折叠/展开（颜色、保存、编辑可用）"},
    [UI_TEXT_HELP_FILE_KEYS] =
        {"入力欄でも  Ctrl+N 新規 / Ctrl+S 保存 / Ctrl+Shift+S 別名保存 / F2 名前変更",
         "In boxes  Ctrl+N New / Ctrl+S Save / Ctrl+Shift+S Save as / F2 Rename",
         "输入栏中也可  Ctrl+N 新建 / Ctrl+S 保存 / Ctrl+Shift+S 另存为 / F2 重命名"},
    [UI_TEXT_HELP_INDEX_COMMAND] = {"索引・閲覧本文  : コマンド / i 編集",
                                    "Index, view  : Command / i Edit",
                                    "索引、查看正文  : 命令 / i 编辑"},
    [UI_TEXT_HELP_EX_SET_NUMBER] =
        {"Ex  :set number / :set nonumber / :set nu!（編集中の原文の行番号）",
         "Ex  :set number / :set nonumber / :set nu! (line numbers in edit mode)",
         "Ex  :set number / :set nonumber / :set nu!（编辑时的原文行号）"},
    [UI_TEXT_HELP_EX_SET_THEME] = {"Ex  :set theme=system / :set theme=light / :set theme=dark",
                                   "Ex  :set theme=system / :set theme=light / :set theme=dark",
                                   "Ex  :set theme=system / :set theme=light / :set theme=dark"},
    [UI_TEXT_HELP_EX_SET_LANGUAGE] =
        {"Ex  :set language=ja / :set language=en / :set language=zh-Hans",
         "Ex  :set language=ja / :set language=en / :set language=zh-Hans",
         "Ex  :set language=ja / :set language=en / :set language=zh-Hans"},
    [UI_TEXT_HELP_REPLACE_FIELD] =
        {"置換欄  Tab 欄を移動 / Enter 1 件 / Ctrl+Enter すべて / Esc 閉じる",
         "Replace box  Tab Move / Enter One / Ctrl+Enter All / Esc Close",
         "替换栏  Tab 切换栏 / Enter 1 处 / Ctrl+Enter 全部 / Esc 关闭"},
    [UI_TEXT_HELP_EX_SUBSTITUTE] =
        {"Ex  :%s/pattern/replacement/[g]（g なしは各行の最初の一致・正規表現）",
         "Ex  :%s/pattern/replacement/[g] (no g = first match per line, regex)",
         "Ex  :%s/pattern/replacement/[g]（无 g 时每行只换第一处、正则）"},
    [UI_TEXT_HELP_SEARCH_KEYS] = {"索引・閲覧本文  / 次を検索 / ? 前を検索 / n・N 繰り返し",
                                  "Index, view  / Find next / ? Find previous / n, N Repeat",
                                  "索引、查看正文  / 查找下一个 / ? 查找上一个 / n、N 重复"},
    [UI_TEXT_HELP_SEARCH_STEP] = {"全区画  F3 次の一致 / Shift+F3 前の一致（向きは変えない）",
                                  "Anywhere  F3 Next match / Shift+F3 Previous match "
                                  "(keeps direction)",
                                  "所有区域  F3 下一处 / Shift+F3 上一处（不改变方向）"},
    [UI_TEXT_HELP_SEARCH_FIELD] =
        {"検索欄  Enter 次 / Shift+Enter 逆 / Esc 閉じる（選択は残る）",
         "Find  Enter Next / Shift+Enter Back / Esc Close "
         "(selection stays)",
         "查找栏  Enter 下一个 / Shift+Enter 反向 / Esc 关闭（保留选择）"},
    [UI_TEXT_HELP_INDEX_MOTION] = {"索引  j/k 次/前 / gg/G 先頭/末尾",
                                   "Index  j/k Next/Previous / gg/G First/Last",
                                   "索引  j/k 下一个/上一个 / gg/G 开头/末尾"},
    [UI_TEXT_HELP_INDEX_FOLD] = {"索引  h/l 折畳/展開 / Enter 本文",
                                 "Index  h/l Fold/Unfold / Enter Body",
                                 "索引  h/l 折叠/展开 / Enter 正文"},
    [UI_TEXT_HELP_INDEX_SCROLL] = {"索引  ↑↓ / PgUp/PgDn スクロール",
                                   "Index  ↑↓ / PgUp/PgDn Scroll", "索引  ↑↓ / PgUp/PgDn 滚动"},
    [UI_TEXT_HELP_EDITOR_ESCAPE] = {"本文  Esc 保存して索引へ（編集は維持）",
                                    "Body  Esc Save and go to index (stays in edit)",
                                    "正文  Esc 保存并返回索引（保持编辑）"},
    [UI_TEXT_TITLE_UNTITLED] = {"無題（未保存）", "Untitled (unsaved)", "无标题（未保存）"},
    [UI_TEXT_TITLE_RECOVERING] = {"名前変更の復旧待ち", "Rename recovery pending",
                                  "等待重命名恢复"},
    [UI_TEXT_CHIP_VIEW] = {"閲覧", "View", "查看"},
    [UI_TEXT_CHIP_EDIT] = {"編集", "Edit", "编辑"},
    [UI_TEXT_PLACEHOLDER_FILTER] = {"すべてのノートを検索", "Search all notes", "搜索全部笔记"},
    /* 絞り込み中の件数（#86）。数だけなので 3 列とも同じ。 */
    [UI_TEXT_STATUS_FILTER_COUNT] = {"{k} / {n}", "{k} / {n}", "{k} / {n}"},
    [UI_TEXT_STATUS_SEARCH_FORWARD] = {"このノート内を検索 ／ 次へ  ", "Find in this note / Next  ",
                                       "在本笔记中查找 ／ 下一个  "},
    [UI_TEXT_STATUS_SEARCH_BACKWARD] = {"このノート内を検索 ／ 前へ  ",
                                        "Find in this note / Previous  ",
                                        "在本笔记中查找 ／ 上一个  "},
    [UI_TEXT_STATUS_SEARCH_MALFORMED] = {"検索できない文字があります",
                                         "The term has characters that cannot be searched",
                                         "查找词中有无法查找的字符"},
    [UI_TEXT_STATUS_SEARCH_BAD_SPAN] = {"選択の範囲を読めません", "Cannot read the selection range",
                                        "无法读取选择范围"},
    [UI_TEXT_STATUS_SEARCH_NOT_FOUND] = {"見つかりません", "Not found", "未找到"},
    [UI_TEXT_STATUS_SEARCH_FOUND] = {"{k} / {n} 件", "{k} / {n}", "{k} / {n} 处"},
    [UI_TEXT_ACTION_SEARCH_PREVIOUS] = {"◀ 前へ", "◀ Prev", "◀ 上一个"},
    [UI_TEXT_ACTION_SEARCH_NEXT] = {"次へ ▶", "Next ▶", "下一个 ▶"},
    [UI_TEXT_STATUS_REPLACE_POSITION] = {" 位置 {offset}", " at {offset}", " 位置 {offset}"},
    [UI_TEXT_STATUS_REPLACE_COUNT] = {"{name} / {k} 件", "{name} / {k}", "{name} / {k} 处"},
    [UI_TEXT_ACTION_REPLACE_ONE] = {"1 件", "One", "1 处"},
    [UI_TEXT_ACTION_REPLACE_ALL] = {"すべて", "All", "全部"},
    [UI_TEXT_STATUS_COMMAND_NOT_FOUND] = {"一致する操作がありません。", "No matching command.",
                                          "没有匹配的操作。"},
    [UI_TEXT_STATUS_PALETTE_CLICK] = {"操作をクリックすると実行します。",
                                      "Click a command to run it.", "点击操作即可执行。"},
    [UI_TEXT_STATUS_PALETTE_FILTER] = {"下の欄に名前を入力すると絞り込めます。",
                                       "Type a name in the box below to filter.",
                                       "在下方栏中输入名称可筛选。"},
    [UI_TEXT_STATUS_PALETTE_PAGES] = {"{k}/{n}", "{k}/{n}", "{k}/{n}"},
    [UI_TEXT_ACTION_KEYS_HIDE] = {"キー操作を閉じる", "Hide keys", "隐藏按键"},
    [UI_TEXT_ACTION_KEYS_SHOW] = {"キー操作を表示", "Show keys", "显示按键"},
    [UI_TEXT_ACTION_PALETTE_PREVIOUS] = {"↑ 前", "↑ Prev", "↑ 上"},
    [UI_TEXT_ACTION_PALETTE_NEXT] = {"↓ 次", "↓ Next", "↓ 下"},
    [UI_TEXT_ACTION_OPERATIONS] = {"操作 ▾", "Actions ▾", "操作 ▾"},
    [UI_TEXT_SETTINGS_THEME] = {"テーマ", "Theme", "主题"},
    [UI_TEXT_SETTINGS_THEME_SYSTEM] = {"OS に従う", "Follow the OS", "跟随系统"},
    [UI_TEXT_SETTINGS_THEME_LIGHT] = {"ライト", "Light", "浅色"},
    [UI_TEXT_SETTINGS_THEME_DARK] = {"ダーク", "Dark", "深色"},
    [UI_TEXT_SETTINGS_LANGUAGE] = {"言語", "Language", "语言"},
    /* 言語の名前は各言語の自称なので 3 列とも同じである（ADR 0032 の決定 7）。 */
    [UI_TEXT_SETTINGS_LANGUAGE_JA] = {"日本語", "日本語", "日本語"},
    [UI_TEXT_SETTINGS_LANGUAGE_EN] = {"English", "English", "English"},
    [UI_TEXT_SETTINGS_LANGUAGE_ZH_HANS] = {"简体中文", "简体中文", "简体中文"},
    [UI_TEXT_SETTINGS_GUIDANCE] = {"↑↓ 選択 / Enter 採用 / Esc 戻る",
                                   "↑↓ Select / Enter Apply / Esc Back",
                                   "↑↓ 选择 / Enter 应用 / Esc 返回"},
    [UI_TEXT_APP_LOGO] = {"NENE FOLIO", "NENE FOLIO", "NENE FOLIO"},
    [UI_TEXT_PROMPT_PENDING_EXPLANATION] =
        {"「再試行」で同じ名前変更を続けます。「閉じる」は取り消しではありません。",
         "Retry continues the same rename. Close does not cancel it.",
         "“重试”继续同一次重命名。“关闭”不是取消。"},
    [UI_TEXT_PROMPT_PENDING_RENAME] = {"{from} → {to}", "{from} → {to}", "{from} → {to}"},
    [UI_TEXT_PROMPT_TITLE_FIRST_SAVE] = {"名前をつけて保存", "Name and save", "命名并保存"},
    [UI_TEXT_PROMPT_TITLE_SAVE_AS] = {"別名で保存", "Save as", "另存为"},
    [UI_TEXT_PROMPT_TITLE_RENAME] = {"名前を変更", "Rename", "重命名"},
    [UI_TEXT_PROMPT_HINT_PENDING] = {"この名前変更を最後まで終えるまで、ほかの操作へ進めません。",
                                     "No other action can run until this rename finishes.",
                                     "在这次重命名完成之前，无法进行其他操作。"},
    [UI_TEXT_PROMPT_HINT_FIRST_SAVE] = {"名前の末尾に .md を補います。",
                                        ".md is added to the end of the name.",
                                        "名称末尾会补上 .md。"},
    [UI_TEXT_PROMPT_HINT_SAVE_AS] = {"元の保存内容を保ち、別の .md を作ります。",
                                     "Keeps the saved note and creates another .md.",
                                     "保留原有保存内容，另建一个 .md。"},
    [UI_TEXT_PROMPT_HINT_RENAME] = {"md と履歴を新しい名前へ移します。",
                                    "Moves the md and its history to the new name.",
                                    "将 md 与历史移到新名称。"},
    [UI_TEXT_PROMPT_ACCEPT_RETRY] = {"再試行", "Retry", "重试"},
    [UI_TEXT_PROMPT_ACCEPT_SAVE] = {"保存", "Save", "保存"},
    [UI_TEXT_PROMPT_ACCEPT_RENAME] = {"変更", "Rename", "重命名"},
    [UI_TEXT_PROMPT_OK] = {"OK", "OK", "确定"},
    [UI_TEXT_PROMPT_CLOSE_PENDING] = {"閉じる", "Close", "关闭"},
    [UI_TEXT_PROMPT_CLOSE_CANCEL] = {"キャンセル", "Cancel", "取消"},
    [UI_TEXT_PROMPT_LABEL_NAME] = {"ノートの名前", "Note name", "笔记名称"},
    [UI_TEXT_PROMPT_LABEL_CATEGORY] = {"保存先カテゴリ", "Destination category", "目标分类"},
    [UI_TEXT_APP_NO_MODULE_PATH] = {"実行ファイルの場所が長すぎるか、取得できません。",
                                    "The program path is too long or cannot be read.",
                                    "可执行文件的路径过长或无法获取。"},
    [UI_TEXT_APP_DATA_IN_USE] =
        {"同じ data/ を別の NeNe Folio が使っています。先に閉じてください。",
         "Another NeNe Folio is using the same data/. Close it first.",
         "另一个 NeNe Folio 正在使用同一个 data/。请先关闭它。"},
    [UI_TEXT_APP_RECOVERY_LOCKED] = {"名前変更の復旧記録（data/.rename.json）がありますが、data/ "
                                     "を占有できないので復旧できません。",
                                     "A rename record (data/.rename.json) exists, but data/ "
                                     "cannot be locked, so it cannot be recovered.",
                                     "存在重命名的恢复记录（data/.rename.json），"
                                     "但无法独占 data/，因此无法恢复。"},
    [UI_TEXT_APP_OUT_OF_MEMORY] = {"記憶域が足りません。", "Out of memory.", "内存不足。"},
    [UI_TEXT_APP_NO_WINDOW] = {"窓を作れませんでした。", "Could not create the window.",
                               "无法创建窗口。"},
};

/* 置換子の綴り。**並びが意味を決める**: 先頭の 3 つが数（k / n / offset）、残りが文字列
 * （name / from / to）。表の文言に現れてよい `{` はこの 6 つだけで、単体が全 ID を回して固定する。
 */
static const char *_Nonnull const placeholders[] = {"{k}",    "{n}",    "{offset}",
                                                    "{name}", "{from}", "{to}"};

constexpr size_t placeholder_count = sizeof placeholders / sizeof placeholders[0];
constexpr size_t placeholder_numbers = 3;
/* 10 進の桁を組む場所。size_t は 20 桁で足りる（append_number と同じ見積り）。 */
constexpr size_t digit_capacity = 20;

const char *_Nonnull ui_text_line(enum ui_text id, enum folio_language language)
{
    /* 行が ID・列が言語。どちらも閉じた列挙なので表の外へは落ちない（決定 2）。 */
    return catalog[id][language];
}

/* out が nullptr なら数えるだけ。書く量と数える量が同じ道を通る（note_replace と同じ 2 段）。 */
static size_t put(char *_Nullable out, size_t at, const char *_Nonnull bytes, size_t length)
{
    if (out != nullptr)
    {
        memcpy(out + at, bytes, length);
    }
    return at + length;
}

/* 10 進・先頭のゼロ無し・桁区切り無し・0 は "0"（append_number と同じ規則・決定 3）。 */
static size_t put_number(char *_Nullable out, size_t at, size_t value)
{
    char digits[digit_capacity];
    size_t count = 0;
    do
    {
        digits[count] = (char)('0' + value % 10);
        count += 1;
        value /= 10;
    } while (value > 0 && count < digit_capacity);
    char ordered[digit_capacity];
    for (size_t index = 0; index < count; ++index)
    {
        ordered[index] = digits[count - 1 - index];
    }
    return put(out, at, ordered, count);
}

static size_t number_of(const struct ui_text_request *_Nonnull request, size_t index)
{
    if (index == 0)
    {
        return request->k;
    }
    return index == 1 ? request->n : request->offset;
}

static const char *_Nonnull text_of(const struct ui_text_request *_Nonnull request, size_t index)
{
    const char *_Nullable value = request->name;
    if (index == placeholder_numbers + 1)
    {
        value = request->from;
    }
    if (index == placeholder_numbers + 2)
    {
        value = request->to;
    }
    return value == nullptr ? "" : value;
}

/* at の位置で始まる置換子。無ければ placeholder_count。 */
static size_t placeholder_at(const char *_Nonnull line, size_t at)
{
    for (size_t index = 0; index < placeholder_count; ++index)
    {
        if (strncmp(line + at, placeholders[index], strlen(placeholders[index])) == 0)
        {
            return index;
        }
    }
    return placeholder_count;
}

static size_t put_value(char *_Nullable out, size_t at,
                        const struct ui_text_request *_Nonnull request, size_t index)
{
    if (index < placeholder_numbers)
    {
        return put_number(out, at, number_of(request, index));
    }
    const char *_Nonnull value = text_of(request, index);
    return put(out, at, value, strlen(value));
}

/* 表の文言を左から 1 パスでたどる。埋めた内容は二度と読まない（決定 3）。 */
static size_t sweep(const struct ui_text_request *_Nonnull request, char *_Nullable out)
{
    const char *_Nonnull line = ui_text_line(request->id, request->language);
    size_t written = 0;
    size_t at = 0;
    while (line[at] != '\0')
    {
        size_t index = line[at] == '{' ? placeholder_at(line, at) : placeholder_count;
        if (index == placeholder_count)
        {
            written = put(out, written, line + at, 1);
            at += 1;
            continue;
        }
        written = put_value(out, written, request, index);
        at += strlen(placeholders[index]);
    }
    return written;
}

enum ui_text_format_outcome ui_text_format(const struct ui_text_request *_Nonnull request,
                                           char *_Nonnull out, size_t capacity)
{
    size_t needed = sweep(request, nullptr);
    if (capacity == 0)
    {
        return UI_TEXT_FORMAT_TOO_LONG;
    }
    if (needed + 1 > capacity)
    {
        out[0] = '\0';
        return UI_TEXT_FORMAT_TOO_LONG;
    }
    out[sweep(request, out)] = '\0';
    return UI_TEXT_FORMAT_READY;
}
