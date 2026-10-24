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
    [UI_TEXT_ACCESSIBLE_COMMAND] = {"コマンド入力", "Command input", "命令输入"},
    [UI_TEXT_ACCESSIBLE_PALETTE] = {"コマンド一覧", "Command palette", "命令列表"},
    [UI_TEXT_ACCESSIBLE_PATTERN] = {"検索する正規表現", "Regex pattern", "搜索正则表达式"},
    [UI_TEXT_ACCESSIBLE_REPLACEMENT] = {"置換文字列", "Replacement text", "替换文本"},
    [UI_TEXT_ACCESSIBLE_CURRENT] = {"現在の設定", "Current setting", "当前设置"},
    [UI_TEXT_ACCESSIBLE_NO_RESULTS] = {"候補がありません。", "No results.", "没有候选项。"},
    [UI_TEXT_ACCESSIBLE_HEADER] = {"共通操作", "Main toolbar", "主工具栏"},
    [UI_TEXT_ACCESSIBLE_PANE_TOOLS] = {"本文の操作帯", "Note toolbar", "笔记工具栏"},
    [UI_TEXT_ACCESSIBLE_MODE] = {"ノートのモード", "Note mode", "笔记模式"},
    [UI_TEXT_ACCESSIBLE_BODY] = {"ノート本文", "Note body", "笔记正文"},
    [UI_TEXT_ACCESSIBLE_NAMED_BODY] = {"{name} — ノート本文", "{name} — Note body",
                                       "{name} — 笔记正文"},
    [UI_TEXT_HELP_ACCESSIBILITY] =
        {"F6 / Shift+F6 区画の移動 · Tab / Shift+Tab 区画内の移動",
         "F6 / Shift+F6: move between regions · Tab / Shift+Tab: move within a region",
         "F6 / Shift+F6：切换区域 · Tab / Shift+Tab：在区域内移动"},
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
    [UI_TEXT_FAILURE_NOTE_TOO_LARGE] =
        {"本文が大きすぎて保存できません。上限は 16 MiB です。本文を減らしてから保存してください。",
         "This note is too large to save. The limit is 16 MiB. Shorten the note, then save again.",
         "笔记过大，无法保存。上限为 16 MiB。请缩短内容后再保存。"},
    [UI_TEXT_FAILURE_NOTE_UNEDITABLE] =
        {"この本文には編集で保てない文字（U+0007・U+2028・U+"
         "2029）があります。閲覧のままにしました。",
         "This note contains characters the editor cannot keep (U+0007, U+2028 or U+2029). It "
         "stays in view mode.",
         "正文包含编辑时无法保留的字符（U+0007、U+2028 或 U+2029），已保持为查看模式。"},
    [UI_TEXT_FAILURE_NOTE_MALFORMED] =
        {"編集中の本文に壊れた文字、または扱えない文字があります。保存していません。",
         "The body being edited has invalid or unsupported characters. It was not saved.",
         "编辑中的正文含有无效或不支持的字符。未保存。"},
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
        {"名前の変更が途中で止まっています。「名前を変更」から再試行してください。",
         "A rename stopped halfway. Open Rename to retry.", "重命名中途停止。请打开“重命名”重试。"},
    [UI_TEXT_FAILURE_RENAME_UNLOCKED] =
        {"data/ に書けないため名前を変更できません。名前は変更していません。",
         "Cannot rename because data/ is not writable. The name was not changed.",
         "因无法写入 data/，不能重命名。名称未更改。"},
    [UI_TEXT_FAILURE_RENAME_UNSUPPORTED] =
        {"この data/ では名前を変更できません（ローカルの "
         "NTFS 以外、またはシンボリックリンク／junction）。",
         "Names cannot be changed in this data/ (not local NTFS, "
         "or a symbolic link / junction).",
         "在此 data/ 中无法更改名称（非本地 NTFS，或为符号链接／junction）。"},
    [UI_TEXT_FAILURE_RENAME_IDENTITY_FAILED] =
        {"元のファイルやカテゴリを確かめられないので名前を変更できません。名前は変更していません。",
         "Cannot verify the original file or category, so the name was not changed.",
         "无法确认原文件或分类，因此不能重命名。名称未更改。"},
    [UI_TEXT_FAILURE_RENAME_JOURNAL_FAILED] =
        {"名前変更の記録（data/.rename.json）を書けませんでした。名前は変更していません。",
         "Could not write the rename record (data/.rename.json). The name was not changed.",
         "无法写入重命名记录（data/.rename.json）。名称未更改。"},
    [UI_TEXT_FAILURE_RENAME_JOURNAL_BROKEN] =
        {"名前変更の記録（data/.rename.json）の版か形を読めません。記録は残しています。",
         "The rename record (data/.rename.json) has an unreadable version or format. "
         "The record was kept.",
         "无法读取重命名记录（data/.rename.json）的版本或形式。记录已保留。"},
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
    [UI_TEXT_FAILURE_FILTERED] =
        {"絞り込み中はこの操作ができません。絞り込みを解いてからやり直してください。",
         "This cannot run while filtering. Clear the filter and try again.",
         "筛选中无法执行此操作。请清除筛选后重试。"},
    [UI_TEXT_FAILURE_SETTINGS_UNREADABLE] =
        {"設定（data/settings.json）を読めません。既定値で始め、直すまで上書きしません。",
         "Cannot read data/settings.json. Using defaults without overwriting it.",
         "无法读取设置（data/settings.json）。以默认值启动，且不会覆盖。"},
    [UI_TEXT_FAILURE_APPEARANCE_UNAVAILABLE] =
        {"Windowsの外観を取得できません。前の配色（初回は通常配色）を保ちます。",
         "Windows appearance is unavailable. Keeping the previous colors (standard colors at "
         "startup).",
         "无法获取 Windows 外观。保留之前的配色（首次启动时使用标准配色）。"},
    [UI_TEXT_HIGH_CONTRAST_ACTIVE] = {"Windowsの高コントラストを適用中",
                                      "Windows high contrast is active",
                                      "正在使用 Windows 高对比度"},
    [UI_TEXT_FAILURE_SETTINGS_STORE_FAILED] =
        {"設定（data/settings.json）を書けませんでした。設定は変えていません。",
         "Could not write data/settings.json. The settings are unchanged.",
         "无法写入设置（data/settings.json）。设置未改变。"},
    [UI_TEXT_FAILURE_PANE_LOAD_FAILED] = {"本文読込失敗。同じモードで再試行。",
                                          "Load failed. Retry the current mode.",
                                          "正文加载失败。请重试当前模式。"},
    [UI_TEXT_FAILURE_PANE_RECOLOR_FAILED] =
        {"本文の配色を更新できませんでした。本文は保持しています。テーマの再選択で再試行できます。",
         "Could not update the body colors. Your text is kept. Select a theme again to retry.",
         "无法更新正文配色。正文已保留。重新选择主题即可重试。"},
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
    [UI_TEXT_FAILURE_REPLACE_STALE] = {"下見が古くなりました。もう一度入力してください。",
                                       "The preview is out of date. Type again.",
                                       "预览已过期。请重新输入。"},
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
    [UI_TEXT_FAILURE_FONT_BUNDLE_UNAVAILABLE] =
        {"同梱の書体を読めなかったので、OS の書体で表示します。",
         "The bundled fonts could not be loaded, so the OS fonts are used.",
         "无法载入随附的字体，因此使用系统字体显示。"},
    [UI_TEXT_FAILURE_HISTORY_EMPTY] = {"履歴はありません。", "No history.", "没有历史记录。"},
    [UI_TEXT_FAILURE_HISTORY_UNREADABLE] = {"その版は読めません。", "That version cannot be read.",
                                            "无法读取该版本。"},
    [UI_TEXT_FAILURE_CATEGORY_NAME_INVALID] =
        {"使えないカテゴリ名です。先頭のピリオド・予約名・末尾の空白やピリオド・区切りを避け、"
         "255バイト以内で指定してください。",
         "That category name cannot be used. Avoid a leading period, reserved names, trailing "
         "spaces or periods, and separators; max 255 bytes.",
         "该分类名不可用。请避开开头的句点、保留名、末尾的空格或句点以及分隔符，并将名称控制在 "
         "255 字节以内。"},
    [UI_TEXT_FAILURE_CATEGORY_NAME_TAKEN] =
        {"同じ名前のカテゴリがあります。別の名前を指定してください。何も変えていません。",
         "A category with that name exists. Give another name. Nothing was changed.",
         "已存在同名分类。请指定其他名称。未做任何改变。"},
    [UI_TEXT_FAILURE_CATEGORY_NOT_CREATED] =
        {"カテゴリのフォルダを作れませんでした。何も変えていません。"
         "data/ に書き込めるか確かめてください。",
         "Could not create the category folder. Nothing was changed. Check that data/ is "
         "writable.",
         "无法创建分类文件夹。未做任何改变。请确认 data/ 可以写入。"},
    [UI_TEXT_FAILURE_CATEGORY_LEDGER_STALE] =
        {"カテゴリは反映しましたが、台帳（categories.json）を書き戻せませんでした。"
         "表示はフォルダに従い、次に台帳を書くときに揃います。",
         "Category applied, but categories.json could not be written back. "
         "The index follows the folders until the next write.",
         "分类已应用，但无法写回台账（categories.json）。"
         "显示以文件夹为准，下次写入台账时将自动对齐。"},
    [UI_TEXT_FAILURE_TRASH_UNAVAILABLE] =
        {"この場所ではごみ箱を使えないため、ノートを移していません。",
         "The Recycle Bin is unavailable here. The note was not moved.",
         "此位置无法使用回收站。笔记未移动。"},
    [UI_TEXT_FAILURE_TRASH_BUSY] = {"ほかのプログラムが使用中のため、ノートを移していません。",
                                    "Another program is using it. The note was not moved.",
                                    "其他程序正在使用它。笔记未移动。"},
    [UI_TEXT_FAILURE_TRASH_FAILED] =
        {"ごみ箱への移動を確認できませんでした。ファイルとごみ箱を確認してください。"
         "履歴だけ移っている場合があります。",
         "Could not confirm the move to the Recycle Bin. Check the files and Recycle Bin; "
         "history may have moved first.",
         "无法确认是否已移入回收站。请检查文件和回收站，历史记录可能已先行移动。"},
    [UI_TEXT_FAILURE_CATEGORY_NOT_EMPTY] =
        {"ノートが残っているカテゴリや、無題のノートの保存先は削除できません。",
         "Cannot remove a category with notes or the destination of an untitled note.",
         "无法删除仍有笔记的分类或无标题笔记的保存位置。"},
    [UI_TEXT_FAILURE_CATEGORY_HAS_FILES] =
        {"フォルダにほかのファイルが残っているので削除していません。フォルダを確認してください。",
         "The folder has other files and was not removed. Check the folder.",
         "文件夹中仍有其他文件，因此未删除。请检查文件夹。"},
    [UI_TEXT_FAILURE_CATEGORY_HISTORY_NOT_RECYCLED] =
        {"履歴のごみ箱への移動を確認できないため、カテゴリのフォルダを削除していません。"
         "履歴とごみ箱を確認してください。",
         "Could not confirm history recycling, so the category folder was not removed. "
         "Check the history and Recycle Bin.",
         "无法确认历史记录是否已移入回收站，因此未删除分类文件夹。请检查历史记录和回收站。"},
    [UI_TEXT_FAILURE_CATEGORY_REMOVE_FAILED] =
        {"カテゴリのフォルダの削除を確認できませんでした。フォルダとごみ箱を確認してください。"
         "履歴や索引だけ処理済みの場合があります。",
         "Could not confirm category folder removal. Check the folder and Recycle Bin; "
         "history or the index may already have been processed.",
         "无法确认分类文件夹是否已删除。请检查文件夹和回收站，历史记录或索引可能已处理。"},
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
    [UI_TEXT_COMMAND_RENAME_CATEGORY] = {"カテゴリ名を変更", "Rename category", "重命名分类"},
    [UI_TEXT_MENU_RENAME_CATEGORY] = {"カテゴリ名を変更…", "Rename category…", "重命名分类…"},
    [UI_TEXT_HELP_RENAME_CATEGORY] = {"Ex  :renamecategory / :rencat 名前 カテゴリ名を変更",
                                      "Ex  :renamecategory / :rencat name Rename a category",
                                      "Ex  :renamecategory / :rencat 名称 重命名分类"},
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
    [UI_TEXT_COMMAND_LAYOUT_DRAWER] = {"一覧のみ表示", "Show list only", "仅显示列表"},
    [UI_TEXT_COMMAND_LAYOUT_BOTH] = {"一覧とノートを表示", "Show list and note", "显示列表和笔记"},
    [UI_TEXT_COMMAND_LAYOUT_NOTE] = {"ノートのみ表示", "Show note only", "仅显示笔记"},
    [UI_TEXT_LAYOUT_BUTTON_DRAWER] = {"一覧", "List", "列表"},
    [UI_TEXT_LAYOUT_BUTTON_BOTH] = {"両方", "Both", "双栏"},
    [UI_TEXT_LAYOUT_BUTTON_NOTE] = {"ノート", "Note", "笔记"},
    [UI_TEXT_LAYOUT_GROUP] = {"表示", "View", "显示"},
    [UI_TEXT_LAYOUT_KEY_DRAWER] = {"Ctrl+Shift+1", "Ctrl+Shift+1", "Ctrl+Shift+1"},
    [UI_TEXT_LAYOUT_KEY_BOTH] = {"Ctrl+Shift+2", "Ctrl+Shift+2", "Ctrl+Shift+2"},
    [UI_TEXT_LAYOUT_KEY_NOTE] = {"Ctrl+Shift+3", "Ctrl+Shift+3", "Ctrl+Shift+3"},
    [UI_TEXT_LAYOUT_NARROW_HINT] = {"\n窓を細くします。", "\nNarrows the window.", "\n缩窄窗口。"},
    [UI_TEXT_TOOLTIP_NAMED_KEY] = {"{name}\n{from}{to}", "{name}\n{from}{to}",
                                   "{name}\n{from}{to}"},
    [UI_TEXT_LAYOUT_MENU] = {"{name}\t{from}", "{name}\t{from}", "{name}\t{from}"},
    [UI_TEXT_LAYOUT_HELP] = {"{from}  {name}", "{from}  {name}", "{from}  {name}"},
    [UI_TEXT_TOOLTIP_SETTINGS] = {"設定を開く", "Open settings", "打开设置"},
    [UI_TEXT_TOOLTIP_CLOSE] = {"閉じる", "Close", "关闭"},
    [UI_TEXT_TOOLTIP_KEY] = {"{from}{to}", "{from}{to}", "{from}{to}"},
    [UI_TEXT_TOOLTIP_OPERATIONS] = {"このノートの操作", "Actions for this note", "本笔记的操作"},
    [UI_TEXT_TOOLTIP_EDIT] = {"本文の編集を始める", "Start editing the note", "开始编辑正文"},
    [UI_TEXT_KEY_NEW] = {"Ctrl+N", "Ctrl+N", "Ctrl+N"},
    [UI_TEXT_KEY_SAVE] = {"Ctrl+S", "Ctrl+S", "Ctrl+S"},
    [UI_TEXT_KEY_HELP] = {"F1", "F1", "F1"},
    [UI_TEXT_KEY_FIND] = {"Ctrl+F", "Ctrl+F", "Ctrl+F"},
    [UI_TEXT_KEY_CLOSE] = {"Alt+F4", "Alt+F4", "Alt+F4"},
    [UI_TEXT_COMMAND_DISCARD_EDITS] = {"編集を破棄して読み直す", "Discard edits and reload",
                                       "放弃编辑并重新载入"},
    [UI_TEXT_COMMAND_HISTORY] = {"履歴から戻す", "Restore from history", "从历史恢复"},
    [UI_TEXT_COMMAND_TRASH_NOTE] = {"ノートをごみ箱へ移す", "Move note to Recycle Bin",
                                    "将笔记移到回收站"},
    [UI_TEXT_MENU_TRASH_NOTE] = {"ごみ箱へ移す", "Move to Recycle Bin", "移到回收站"},
    [UI_TEXT_TRASH_NOTE_COMPLETED] = {"一覧から除きました。", "Removed from the list.",
                                      "已从列表移除。"},
    [UI_TEXT_NOTE_OPENED_IN_VIEW] =
        {"閲覧で開きました。編集で保てない文字があります。",
         "Opened in view mode. It has characters the editor cannot keep.",
         "已以查看模式打开。正文含有编辑时无法保留的字符。"},
    [UI_TEXT_HELP_TRASH_NOTE] =
        {"Ex  :trashnote / :trash ノートと履歴をごみ箱へ移す（確認なし）",
         "Ex  :trashnote / :trash Move note and history to Recycle Bin (no confirmation)",
         "Ex  :trashnote / :trash 将笔记和历史移到回收站（不确认）"},
    [UI_TEXT_COMMAND_NEW_CATEGORY] = {"新しいカテゴリ", "New category", "新建分类"},
    [UI_TEXT_COMMAND_DELETE_CATEGORY] = {"カテゴリを削除", "Delete category", "删除分类"},
    [UI_TEXT_MENU_DELETE_CATEGORY] = {"カテゴリを削除", "Delete category", "删除分类"},
    [UI_TEXT_FAILURE_CATEGORY_NOT_SELECTED] = {"カテゴリを選んでください。", "Select a category.",
                                               "请选择分类。"},
    [UI_TEXT_HELP_DELETE_CATEGORY] =
        {"Ex  :deletecategory / :delcat 空カテゴリを削除（確認なし）",
         "Ex  :deletecategory / :delcat Delete an empty category (no confirmation)",
         "Ex  :deletecategory / :delcat 删除空分类（不确认）"},
    [UI_TEXT_MENU_RECOLOR] = {"色を変える…", "Change color…", "更改颜色…"},
    [UI_TEXT_MENU_RENAME_NOTE] = {"名前を変更…", "Rename…", "重命名…"},
    [UI_TEXT_MENU_NEW_CATEGORY] = {"新しいカテゴリ…", "New category…", "新建分类…"},
    [UI_TEXT_MENU_NEW_NOTE_IN_CATEGORY] = {"このカテゴリに新しいノート",
                                           "New note in this category", "在此分类中新建笔记"},
    [UI_TEXT_HELP_PALETTE] = {"一覧  ↑↓ 選択 / Enter 実行 / Esc 閉じる / Tab 説明",
                              "List  ↑↓ Select / Enter Run / Esc Close / Tab Keys",
                              "列表  ↑↓ 选择 / Enter 执行 / Esc 关闭 / Tab 按键"},
    [UI_TEXT_HELP_EDITOR_MOTION] = {"編集本文  Ctrl+h/j/k/l ←/↓/↑/→",
                                    "Editor  Ctrl+h/j/k/l ←/↓/↑/→",
                                    "编辑正文  Ctrl+h/j/k/l ←/↓/↑/→"},
    [UI_TEXT_HELP_GLOBAL_KEYS] = {"全区画  {name} ヘルプ / {from} 一覧 / {to} このノート内を検索",
                                  "Anywhere  {name} Help / {from} List / {to} Find in this note",
                                  "所有区域  {name} 帮助 / {from} 列表 / {to} 在本笔记中查找"},
    [UI_TEXT_HELP_GLOBAL_FILTER] = {"全区画  {name} すべてのノートを検索（索引を絞り込む）",
                                    "Anywhere  {name} Search all notes (filters the index)",
                                    "所有区域  {name} 搜索全部笔记（筛选索引）"},
    [UI_TEXT_HELP_FILTER_FIELD] =
        {"絞り込み欄  Esc・Enter 索引へ戻る（絞り込みは残る）/ 空にすると解除",
         "Filter box  Esc, Enter Back to index (filter stays) / Empty clears",
         "筛选栏  Esc、Enter 返回索引（保留筛选）/ 清空则取消"},
    [UI_TEXT_HELP_FILTER_LIMITS] =
        {"絞り込み中  並び替えと折畳/展開はできない（色・保存・編集は可）",
         "While filtering  No reorder or fold/unfold (color, save, edit are OK)",
         "筛选中  不能排序与折叠/展开（颜色、保存、编辑可用）"},
    [UI_TEXT_HELP_FILE_KEYS] = {"入力欄でも  {name} 新規 / {from} 保存 / {to} 別名保存",
                                "In boxes  {name} New / {from} Save / {to} Save as",
                                "输入栏中也可  {name} 新建 / {from} 保存 / {to} 另存为"},
    [UI_TEXT_HELP_INDEX_COMMAND] = {"索引・閲覧本文  : コマンド / i 編集",
                                    "Index, view  : Command / i Edit",
                                    "索引、查看正文  : 命令 / i 编辑"},
    [UI_TEXT_HELP_EX_SET_NUMBER] =
        {"Ex  :set number / :set nonumber / :set nu!（編集中の原文の行番号）",
         "Ex  :set number / :set nonumber / :set nu! (line numbers in edit mode)",
         "Ex  :set number / :set nonumber / :set nu!（编辑时的原文行号）"},
    [UI_TEXT_HELP_EX_SET_THEME] = {"Ex  :set theme=dracula / :set theme=system",
                                   "Ex  :set theme=dracula / :set theme=system",
                                   "Ex  :set theme=dracula / :set theme=system"},
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
    [UI_TEXT_HELP_SEARCH_STEP] = {"全区画  {name} 次の一致 / {from} 前の一致（向きは変えない）",
                                  "Anywhere  {name} Next match / {from} Previous match "
                                  "(keeps direction)",
                                  "所有区域  {name} 下一处 / {from} 上一处（不改变方向）"},
    [UI_TEXT_HELP_SEARCH_FIELD] = {"検索欄  {name} 次 / {from} 逆 / Esc 閉じる（選択は残る）",
                                   "Find  {name} Next / {from} Back / Esc Close "
                                   "(selection stays)",
                                   "查找栏  {name} 下一个 / {from} 反向 / Esc 关闭（保留选择）"},
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
    [UI_TEXT_SESSION_UNTITLED] = {"無題のノートを保存してから終了してください。",
                                  "Save the untitled note to close.",
                                  "请先保存未命名笔记再结束会话。"},
    [UI_TEXT_SESSION_RENAME_PENDING] = {"名前変更の復旧を完了してから終了してください。",
                                        "Finish rename recovery to close.",
                                        "请先完成重命名恢复再结束会话。"},
    [UI_TEXT_SESSION_COMPOSING] = {"文字の変換を確定してから終了してください。",
                                   "Finish text input to close.", "请先完成文字输入再结束会话。"},
    [UI_TEXT_SESSION_BUSY] = {"開いている操作を終えてから終了してください。",
                              "Finish the current operation.", "请先完成当前操作再结束会话。"},
    [UI_TEXT_SESSION_SAVE_INCOMPLETE] = {"保存処理が未完了です。保存を再試行してください。",
                                         "Saving incomplete. Please retry.",
                                         "保存尚未完成。请重试保存。"},
    [UI_TEXT_TITLE_UNTITLED] = {"無題", "Untitled", "无标题"},
    [UI_TEXT_TITLE_RECOVERING] = {"名前変更の復旧待ち", "Rename recovery pending",
                                  "等待重命名恢复"},
    [UI_TEXT_TITLE_CATEGORY_RECOVERING] = {"復旧待ち", "Recovering", "等待恢复"},
    [UI_TEXT_CHIP_VIEW] = {"閲覧", "Read", "查看"},
    [UI_TEXT_CHIP_EDIT] = {"編集", "Edit", "编辑"},
    [UI_TEXT_PLACEHOLDER_FILTER] = {"すべてのノートを検索", "Search all notes", "搜索全部笔记"},
    /* 絞り込み中の件数（#86）。数だけなので 3 列とも同じ。 */
    [UI_TEXT_STATUS_FILTER_COUNT] = {"{k} / {n}", "{k} / {n}", "{k} / {n}"},
    [UI_TEXT_STATUS_SEARCH_FORWARD] = {"このノート内を検索 / Enter: 次へ",
                                       "Find in this note / Enter: Next",
                                       "在本笔记中查找 / Enter: 下一个"},
    [UI_TEXT_STATUS_SEARCH_BACKWARD] = {"このノート内を検索 / Enter: 前へ",
                                        "Find in this note / Enter: Previous",
                                        "在本笔记中查找 / Enter: 上一个"},
    [UI_TEXT_STATUS_SEARCH_MALFORMED] = {"検索できない文字があります",
                                         "Invalid characters in search text",
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
    [UI_TEXT_THEME_TITLE] = {"テーマを選ぶ", "Choose a theme", "选择主题"},
    [UI_TEXT_THEME_CHOOSE] = {"テーマを選ぶ…", "Choose a theme…", "选择主题…"},
    [UI_TEXT_THEME_FILTER] = {"テーマ名で検索", "Search theme names", "搜索主题名称"},
    [UI_TEXT_THEME_BACK] = {"‹ 設定", "‹ Settings", "‹ 设置"},
    [UI_TEXT_THEME_GUIDANCE] = {"↑↓で移動・Enterで適用・Escで戻る",
                                "↑↓ Move · Enter Apply · Esc Back",
                                "↑↓ 移动 · Enter 应用 · Esc 返回"},
    [UI_TEXT_THEME_COUNT] = {"{k} / {n} 件", "{k} / {n} themes", "{k} / {n} 个主题"},
    [UI_TEXT_THEME_EMPTY] = {"一致するテーマがありません", "No matching themes", "没有匹配的主题"},
    [UI_TEXT_THEME_SAMPLE] = {"Aa", "Aa", "Aa"},
    [UI_TEXT_SETTINGS_THEME] = {"テーマ", "Theme", "主题"},
    [UI_TEXT_SETTINGS_THEME_SYSTEM] = {"OS に従う", "Follow the OS", "跟随系统"},
    [UI_TEXT_SETTINGS_THEME_LIGHT] = {"ライト", "Light", "浅色"},
    [UI_TEXT_SETTINGS_THEME_DARK] = {"ダーク", "Dark", "深色"},
    [UI_TEXT_SETTINGS_LANGUAGE] = {"言語", "Language", "语言"},
    /* 言語の名前は各言語の自称なので 3 列とも同じである（ADR 0032 の決定 7）。 */
    [UI_TEXT_SETTINGS_LANGUAGE_JA] = {"日本語", "日本語", "日本語"},
    [UI_TEXT_SETTINGS_LANGUAGE_EN] = {"English", "English", "English"},
    [UI_TEXT_SETTINGS_LANGUAGE_ZH_HANS] = {"简体中文", "简体中文", "简体中文"},
    [UI_TEXT_SETTINGS_NUMBER] = {"行番号", "Line numbers", "行号"},
    [UI_TEXT_SETTINGS_NUMBER_ON] = {"表示する", "Show", "显示"},
    [UI_TEXT_SETTINGS_NUMBER_OFF] = {"表示しない", "Hide", "隐藏"},
    [UI_TEXT_SETTINGS_VERSION] = {"版 {name}", "Version {name}", "版本 {name}"},
    [UI_TEXT_SETTINGS_GUIDANCE] = {"矢印 / Tab 選択 · Enter / Space 採用 · Esc 閉じる",
                                   "Arrows / Tab select · Enter / Space apply · Esc close",
                                   "方向键 / Tab 选择 · Enter / Space 应用 · Esc 关闭"},
    [UI_TEXT_SETTINGS_IMMEDIATE] = {"変更はすぐに反映", "Changes apply immediately",
                                    "更改立即生效"},
    [UI_TEXT_SETTINGS_NUMBER_CHECK] = {"行番号を表示", "Show line numbers", "显示行号"},
    [UI_TEXT_HISTORY_GUIDANCE] = {"1 が最新 / ↑↓ 選択 / Enter 戻す / Esc 閉じる",
                                  "1 is newest / ↑↓ Select / Enter Restore / Esc Close",
                                  "1 为最新 / ↑↓ 选择 / Enter 恢复 / Esc 关闭"},
    /* 履歴の 1 行（ADR 0038 の決定 12）。{n} は版番号・{name} は本文の最初の空でない行。 */
    [UI_TEXT_HISTORY_ROW] = {"{n}  {name}", "{n}  {name}", "{n}  {name}"},
    [UI_TEXT_HISTORY_ROW_UNREADABLE] = {"{n}  （読めません）", "{n}  (cannot be read)",
                                        "{n}  （无法读取）"},
    [UI_TEXT_APP_LOGO] = {"NENE FOLIO", "NENE FOLIO", "NENE FOLIO"},
    [UI_TEXT_TITLE_CATEGORIES] = {"カテゴリ", "Categories", "分类"},
    [UI_TEXT_PROMPT_PENDING_EXPLANATION] =
        {"「再試行」で同じ名前変更を続けます。「閉じる」は取り消しではありません。",
         "Retry continues the same rename. Close does not cancel it.",
         "“重试”继续同一次重命名。“关闭”不是取消。"},
    [UI_TEXT_PROMPT_PENDING_RENAME] = {"{from} → {to}", "{from} → {to}", "{from} → {to}"},
    [UI_TEXT_PROMPT_TITLE_LEAVE] = {"保存しますか", "Save this note?", "要保存吗？"},
    [UI_TEXT_PROMPT_HINT_LEAVE] =
        {"保存しますか。保存しないと内容は失われます。名前の末尾に .md を補います。",
         "Save this note? Unsaved text will be lost. .md is added to the name.",
         "要保存吗？不保存的话，内容将会丢失。名称末尾会补上 .md。"},
    [UI_TEXT_PROMPT_DISCARD] = {"保存しない(&D)", "&Don't save", "不保存(&D)"},
    [UI_TEXT_PROMPT_TITLE_FIRST_SAVE] = {"名前をつけて保存", "Name and save", "命名并保存"},
    [UI_TEXT_PROMPT_TITLE_SAVE_AS] = {"別名で保存", "Save as", "另存为"},
    [UI_TEXT_PROMPT_TITLE_RENAME] = {"名前を変更", "Rename", "重命名"},
    [UI_TEXT_PROMPT_TITLE_RENAME_CATEGORY] = {"カテゴリ名を変更", "Rename category", "重命名分类"},
    [UI_TEXT_PROMPT_HINT_RENAME_CATEGORY] = {"フォルダと履歴を新しい名前へ移します。",
                                             "Moves the folder and its history to the new name.",
                                             "将文件夹及其历史移到新名称。"},
    [UI_TEXT_PROMPT_HINT_RETRY_RENAME] =
        {"先にこの名前変更を終えます。いま頼んだ操作はまだ始めていません。",
         "Finish this rename first. The action you asked for has not started.",
         "先完成这次重命名。您请求的操作尚未开始。"},
    [UI_TEXT_PROMPT_TITLE_NEW_CATEGORY] = {"新しいカテゴリ", "New category", "新建分类"},
    [UI_TEXT_PROMPT_HINT_PENDING] = {"この名前変更はまだ完了していません。",
                                     "This rename is not finished yet.", "这次重命名尚未完成。"},
    [UI_TEXT_PROMPT_HINT_FIRST_SAVE] = {"名前の末尾に .md を補います。",
                                        ".md is added to the end of the name.",
                                        "名称末尾会补上 .md。"},
    [UI_TEXT_PROMPT_HINT_SAVE_AS] = {"元の保存内容を保ち、別の .md を作ります。",
                                     "Keeps the saved note and creates another .md.",
                                     "保留原有保存内容，另建一个 .md。"},
    [UI_TEXT_PROMPT_HINT_RENAME] = {"md と履歴を新しい名前へ移します。",
                                    "Moves the md and its history to the new name.",
                                    "将 md 与历史移到新名称。"},
    [UI_TEXT_PROMPT_HINT_NEW_CATEGORY] = {"data/ にカテゴリのフォルダを作ります。",
                                          "Creates a folder for the category in data/.",
                                          "在 data/ 中为该分类创建文件夹。"},
    [UI_TEXT_PROMPT_ACCEPT_RETRY] = {"再試行", "Retry", "重试"},
    [UI_TEXT_PROMPT_ACCEPT_SAVE] = {"保存(&S)", "&Save", "保存(&S)"},
    [UI_TEXT_PROMPT_ACCEPT_RENAME] = {"変更", "Rename", "重命名"},
    [UI_TEXT_PROMPT_ACCEPT_CREATE] = {"作成", "Create", "创建"},
    [UI_TEXT_PROMPT_OK] = {"OK", "OK", "确定"},
    [UI_TEXT_PROMPT_CLOSE_PENDING] = {"閉じる", "Close", "关闭"},
    [UI_TEXT_PROMPT_CLOSE_CANCEL] = {"キャンセル(&C)", "&Cancel", "取消(&C)"},
    [UI_TEXT_PROMPT_LABEL_NAME] = {"名前", "Name", "名称"},
    [UI_TEXT_PROMPT_LABEL_CATEGORY] = {"保存先", "Save to", "保存到"},
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
    /* 本文操作帯と保存状態（#349・ADR0060 の決定 8〜14）。▾は表示だけの記号で UIA 名に入れない。 */
    [UI_TEXT_ACCESSIBLE_OPERATIONS] = {"操作", "Actions", "操作"},
    [UI_TEXT_ACCESSIBLE_NOTE_ACTIONS] = {"ノートの操作", "Note actions", "笔记操作"},
    [UI_TEXT_ACCESSIBLE_BREADCRUMB] = {"{from} / {name}", "{from} / {name}", "{from} / {name}"},
    [UI_TEXT_ACCESSIBLE_BREADCRUMB_UNSAVED] = {"{from} / {name}（未保存）",
                                               "{from} / {name} (unsaved)",
                                               "{from} / {name}（未保存）"},
    [UI_TEXT_STATUS_UNSAVED] = {"未保存の変更があります", "Unsaved changes", "有未保存的更改"},
    [UI_TEXT_STATUS_SAVED] = {"保存済み", "Saved", "已保存"},
    [UI_TEXT_NOTIFY_SAVED] = {"保存しました", "Saved.", "已保存。"},
    [UI_TEXT_TOOLTIP_HELP] = {"ヘルプとコマンド一覧（F1 / Ctrl+P）",
                              "Help and command list (F1 / Ctrl+P)",
                              "帮助和命令列表（F1 / Ctrl+P）"},
    [UI_TEXT_TOOLTIP_NEW_NOTE] = {"新しいノート — {name}へ（{from}）",
                                  "New note — in {name} ({from})",
                                  "新建笔记 — 放入{name}（{from}）"},
    [UI_TEXT_NEW_NOTE_NO_CATEGORY] = {"カテゴリがありません。先にカテゴリを作ります",
                                      "There are no categories. Create a category first",
                                      "没有分类。请先创建分类"},
    [UI_TEXT_EMPTY_HEADING] = {"ノートが選ばれていません", "No note is selected", "未选择笔记"},
    [UI_TEXT_EMPTY_BODY] = {"左の一覧からノートを選ぶか、新しいノートを作ります",
                            "Choose a note from the list on the left, or create a new note",
                            "从左侧列表选择笔记，或新建笔记"},
    [UI_TEXT_EMPTY_KEYS] = {"すべてのノートを検索: Ctrl+Shift+F ／ ヘルプ: F1",
                            "Search all notes: Ctrl+Shift+F / Help: F1",
                            "搜索全部笔记: Ctrl+Shift+F ／ 帮助: F1"},
    [UI_TEXT_COMMAND_CAPTION] = {"{name}", "{name}", "{name}"},
    [UI_TEXT_COMMAND_INPUT_CAPTION] = {"{name}…", "{name}…", "{name}…"},
    [UI_TEXT_MENU_ACCESS_CAPTION] = {"{name} ({from})", "{name} ({from})", "{name} ({from})"},
    [UI_TEXT_KEY_SAVE_AS] = {"Ctrl+Shift+S", "Ctrl+Shift+S", "Ctrl+Shift+S"},
    [UI_TEXT_KEY_RENAME] = {"F2", "F2", "F2"},
    [UI_TEXT_KEY_PALETTE] = {"Ctrl+P", "Ctrl+P", "Ctrl+P"},
    [UI_TEXT_KEY_FILTER] = {"Ctrl+Shift+F", "Ctrl+Shift+F", "Ctrl+Shift+F"},
    [UI_TEXT_KEY_SEARCH_FORWARD] = {"F3", "F3", "F3"},
    [UI_TEXT_KEY_SEARCH_BACKWARD] = {"Shift+F3", "Shift+F3", "Shift+F3"},
    [UI_TEXT_KEY_SEARCH_NEXT] = {"Enter", "Enter", "Enter"},
    [UI_TEXT_KEY_SEARCH_PREVIOUS] = {"Shift+Enter", "Shift+Enter", "Shift+Enter"},
    [UI_TEXT_KEY_SUBSTITUTE] = {":%s", ":%s", ":%s"},
    [UI_TEXT_FAILURE_COMMAND_NOTE_REQUIRED] = {"「{name}」はノートを開くと使えます。",
                                               "Open a note to use {name}.",
                                               "打开笔记后即可使用“{name}”。"},
    [UI_TEXT_NOTIFY_NOTE_REMOVED] = {"「{name}」を一覧から除きました。",
                                     "Removed {name} from the list.", "已将“{name}”从列表中移除。"},
    [UI_TEXT_HELP_RECYCLE] =
        {"ごみ箱へ移せたノートは Windows のごみ箱から戻せます。",
         "Notes moved to the Recycle Bin can be restored from the Windows Recycle Bin.",
         "移入回收站的笔记可以从 Windows 回收站还原。"},
    [UI_TEXT_TOOLTIP_HELP_KEYS] = {"ヘルプとコマンド一覧（{from} / {to}）",
                                   "Help and command list ({from} / {to})",
                                   "帮助和命令列表（{from} / {to}）"},
    [UI_TEXT_MENU_SCROLL_UP] = {"上に項目があります", "More items above", "上方还有项目"},
    [UI_TEXT_MENU_SCROLL_DOWN] = {"下に項目があります", "More items below", "下方还有项目"},

    [UI_TEXT_KEY_CONTEXT_MENU] = {"Shift+F10", "Shift+F10", "Shift+F10"},
    [UI_TEXT_KEY_APPS] = {"Apps", "Apps", "Apps"},
    [UI_TEXT_MENU_LOCAL_KEY] = {"{name}（メニュー内）", "{name} (menu)", "{name}（菜单内）"},

    [UI_TEXT_FAILURE_NAME_UNCHANGED] = {"同じ名前です。", "The name is unchanged.", "名称相同。"},
    [UI_TEXT_PROMPT_NAME_EMPTY] = {"名前を入力してください。", "Enter a name.", "请输入名称。"},
    [UI_TEXT_PROMPT_TITLE_UNTITLED] = {"無題のノート", "Untitled note", "无标题笔记"},
    [UI_TEXT_PROMPT_CATEGORY_READONLY] = {"{name}（変更できません）", "{name} (cannot be changed)",
                                          "{name}（无法更改）"},
    [UI_TEXT_FAILURE_COMMAND_REASON] = {"{name}\r\n{from}", "{name}\r\n{from}", "{name}\r\n{from}"},
    [UI_TEXT_STATUS_REPLACE_COUNT_ONLY] = {"{k} 件", "{k} matches", "{k} 处"},
    [UI_TEXT_HELP_TAB_COMMANDS] = {"操作", "Actions", "操作"},
    [UI_TEXT_HELP_TAB_KEYS] = {"キー", "Keys", "按键"},
    [UI_TEXT_HELP_GROUP_NOTE] = {"ノート", "Note", "笔记"},
    [UI_TEXT_HELP_GROUP_CATEGORY] = {"カテゴリ", "Category", "分类"},
    [UI_TEXT_HELP_GROUP_LAYOUT] = {"表示", "View", "显示"},
    [UI_TEXT_HELP_GROUP_APP] = {"アプリ", "App", "应用"},
    [UI_TEXT_HELP_GROUP_QUIT] = {"終了", "Quit", "退出"},
    [UI_TEXT_MENU_EX_HINT] = {":{name}", ":{name}", ":{name}"},
    [UI_TEXT_HELP_RENAME_KEY] = {"{name} / {from} 名前変更", "{name} / {from} Rename",
                                 "{name} / {from} 重命名"},

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
