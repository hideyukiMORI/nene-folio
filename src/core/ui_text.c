#include "ui_text.h"

#include "ui_text_request.h"

#include <string.h>

/* 利用者に見える文言の唯一の表（ADR 0030 の決定 1・ADR 0032 の決定 2）。
 * 行が ID・列が言語（ja / en / zh-Hans の順で、正本は enum folio_language の並び）。
 * 日本語の列は #74 の前のソースから機械的に写したもので、**この単位で字面が変わるのは
 * 構文の見本を ASCII に揃えた 2 件だけ**（UI_TEXT_KEYS_KEY_SUBSTITUTE と
 * UI_TEXT_COMMAND_SUBSTITUTE のパターン／置換）である。
 * 1 字でも変えるときは同じコミットで tests/unit/ui_text_tests.c の期待表も変える。
 * 列の網羅（訳し忘れと空の列）は CNF-011 が字句で守る。**简体中文は母語話者の確認を
 * 得ていない**（ADR 0032 の決定 3。後日この 1 列だけを直せる）。 */
static const char *_Nonnull const catalog[][folio_language_count] = {
    [UI_TEXT_KEYS_GROUP_BASIC] = {"よく使う操作", "Everyday actions", "常用操作"},
    [UI_TEXT_KEYS_DESC_MODE] = {"閲覧・編集を切り替える\n閲覧へ戻るときに保存します。",
                                "Switch between Read and Edit.\nReturning to Read saves the note.",
                                "切换阅读与编辑。\n返回阅读时保存笔记。"},
    [UI_TEXT_KEYS_DESC_MENU] = {"このノートの操作メニューを開く",
                                "Open the actions menu for this note", "打开此笔记的操作菜单"},
    [UI_TEXT_KEYS_DESC_FILTER] = {"すべてのノートを検索\n名前と本文で一覧を絞り込みます。",
                                  "Search all notes.\nFilter the list by name and body text.",
                                  "搜索全部笔记。\n按名称与正文筛选列表。"},
    [UI_TEXT_KEYS_DESC_PALETTE] = {"操作を名前で探す", "Find an action by name", "按名称查找操作"},
    [UI_TEXT_KEYS_DESC_FILE_SCOPE] = {"新規・保存・別名保存は、入力欄からも使えます。",
                                      "New, Save and Save As also work from input fields.",
                                      "新建、保存、另存为也可在输入栏中使用。"},
    [UI_TEXT_KEYS_GROUP_VIEW] = {"表示と区画の移動", "View and focus", "显示与焦点"},
    [UI_TEXT_KEYS_DESC_DRAWER] = {"一覧だけを表示\n窓を細くします。",
                                  "Show only the list.\nNarrows the window.",
                                  "仅显示列表。\n窗口变窄。"},
    [UI_TEXT_KEYS_KEY_F6] = {"F6", "F6", "F6"},
    [UI_TEXT_KEYS_DESC_F6] = {"次の区画へ移る", "Move to the next region", "移到下一个区域"},
    [UI_TEXT_KEYS_KEY_SHIFT_F6] = {"Shift+F6", "Shift+F6", "Shift+F6"},
    [UI_TEXT_KEYS_DESC_SHIFT_F6] = {"前の区画へ移る", "Move to the previous region",
                                    "移到上一个区域"},
    [UI_TEXT_KEYS_KEY_TAB] = {"Tab", "Tab", "Tab"},
    [UI_TEXT_KEYS_DESC_TAB] = {"区画内の次の操作へ移る", "Move to the next control in a region",
                               "移到区域内的下一个控件"},
    [UI_TEXT_KEYS_KEY_SHIFT_TAB] = {"Shift+Tab", "Shift+Tab", "Shift+Tab"},
    [UI_TEXT_KEYS_DESC_SHIFT_TAB] = {"区画内の前の操作へ移る",
                                     "Move to the previous control in a region",
                                     "移到区域内的上一个控件"},
    [UI_TEXT_KEYS_GROUP_NOTE] = {"ノート本文", "Note body", "笔记正文"},
    [UI_TEXT_KEYS_KEY_ESC] = {"Esc", "Esc", "Esc"},
    [UI_TEXT_KEYS_DESC_ESC] = {"保存して一覧へ戻る\n編集モードは維持します。",
                               "Save and return to the list.\nEdit mode stays active.",
                               "保存并返回列表。\n保持编辑模式。"},
    [UI_TEXT_KEYS_KEY_ZOOM_WHEEL] = {"Ctrl+ホイール", "Ctrl+Wheel", "Ctrl+滚轮"},
    [UI_TEXT_KEYS_DESC_ZOOM_WHEEL] = {"本文を拡大・縮小する", "Zoom the note body in or out",
                                      "放大或缩小正文"},
    [UI_TEXT_KEYS_KEY_ZOOM_IN] = {"Ctrl++", "Ctrl++", "Ctrl++"},
    [UI_TEXT_KEYS_DESC_ZOOM_IN] = {"本文を拡大する", "Zoom the note body in", "放大正文"},
    [UI_TEXT_KEYS_KEY_ZOOM_OUT] = {"Ctrl+-", "Ctrl+-", "Ctrl+-"},
    [UI_TEXT_KEYS_DESC_ZOOM_OUT] = {"本文を縮小する", "Zoom the note body out", "缩小正文"},
    [UI_TEXT_KEYS_KEY_ZOOM_RESET] = {"Ctrl+0", "Ctrl+0", "Ctrl+0"},
    [UI_TEXT_KEYS_DESC_ZOOM_RESET] = {"本文の倍率を100%に戻す", "Reset the note body to 100%",
                                      "将正文缩放重置为100%"},
    [UI_TEXT_KEYS_KEY_EDIT_LEFT] = {"Ctrl+h", "Ctrl+h", "Ctrl+h"},
    [UI_TEXT_KEYS_DESC_EDIT_LEFT] = {"編集中のカーソルを左へ動かす", "Move the editor caret left",
                                     "编辑时将光标向左移动"},
    [UI_TEXT_KEYS_KEY_EDIT_DOWN] = {"Ctrl+j", "Ctrl+j", "Ctrl+j"},
    [UI_TEXT_KEYS_DESC_EDIT_DOWN] = {"編集中のカーソルを下へ動かす", "Move the editor caret down",
                                     "编辑时将光标向下移动"},
    [UI_TEXT_KEYS_KEY_EDIT_UP] = {"Ctrl+k", "Ctrl+k", "Ctrl+k"},
    [UI_TEXT_KEYS_DESC_EDIT_UP] = {"編集中のカーソルを上へ動かす", "Move the editor caret up",
                                   "编辑时将光标向上移动"},
    [UI_TEXT_KEYS_KEY_EDIT_RIGHT] = {"Ctrl+l", "Ctrl+l", "Ctrl+l"},
    [UI_TEXT_KEYS_DESC_EDIT_RIGHT] = {"編集中のカーソルを右へ動かす", "Move the editor caret right",
                                      "编辑时将光标向右移动"},
    [UI_TEXT_KEYS_GROUP_INDEX] = {"ノート一覧", "Note list", "笔记列表"},
    [UI_TEXT_KEYS_KEY_INDEX_NEXT] = {"j", "j", "j"},
    [UI_TEXT_KEYS_DESC_INDEX_NEXT] = {"一覧の次の項目へ", "Next item in the list",
                                      "列表中的下一项"},
    [UI_TEXT_KEYS_KEY_INDEX_PREVIOUS] = {"k", "k", "k"},
    [UI_TEXT_KEYS_DESC_INDEX_PREVIOUS] = {"一覧の前の項目へ", "Previous item in the list",
                                          "列表中的上一项"},
    [UI_TEXT_KEYS_KEY_INDEX_FIRST] = {"gg", "gg", "gg"},
    [UI_TEXT_KEYS_DESC_INDEX_FIRST] = {"先頭へ（gを2回）", "First item (press g twice)",
                                       "首项（按两次g）"},
    [UI_TEXT_KEYS_KEY_INDEX_LAST] = {"G", "G", "G"},
    [UI_TEXT_KEYS_DESC_INDEX_LAST] = {"末尾へ（Shift+g）", "Last item (Shift+g)",
                                      "末项（Shift+g）"},
    [UI_TEXT_KEYS_KEY_INDEX_FOLD] = {"h", "h", "h"},
    [UI_TEXT_KEYS_DESC_INDEX_FOLD] = {"カテゴリを折り畳む", "Collapse the category", "折叠分类"},
    [UI_TEXT_KEYS_KEY_INDEX_UNFOLD] = {"l", "l", "l"},
    [UI_TEXT_KEYS_DESC_INDEX_UNFOLD] = {"カテゴリを展開する", "Expand the category", "展开分类"},
    [UI_TEXT_KEYS_DESC_BODY] = {"一覧から本文へ移る", "Move from the list to the note body",
                                "从列表移到正文"},
    [UI_TEXT_KEYS_KEY_SCROLL] = {"↑ / ↓", "↑ / ↓", "↑ / ↓"},
    [UI_TEXT_KEYS_DESC_SCROLL] = {"一覧をスクロールする", "Scroll the list", "滚动列表"},
    [UI_TEXT_KEYS_KEY_PAGES] = {"PgUp / PgDn", "PgUp / PgDn", "PgUp / PgDn"},
    [UI_TEXT_KEYS_DESC_INDEX_PAGES] = {"一覧をページ単位でスクロールする",
                                       "Scroll the list by a page", "按页滚动列表"},
    [UI_TEXT_KEYS_GROUP_SEARCH] = {"検索欄と検索結果", "Find and filter", "查找与筛选"},
    [UI_TEXT_KEYS_DESC_NEXT_MATCH] = {"次の一致へ\n検索の向きは変えません。",
                                      "Go to the next match.\nKeeps the search direction.",
                                      "转到下一处匹配。\n不改变搜索方向。"},
    [UI_TEXT_KEYS_DESC_PREVIOUS_MATCH] = {"前の一致へ\n検索の向きは変えません。",
                                          "Go to the previous match.\nKeeps the search direction.",
                                          "转到上一处匹配。\n不改变搜索方向。"},
    [UI_TEXT_KEYS_DESC_SEARCH_ENTER] =
        {"ノート内検索欄で、現在の向きの次の一致へ",
         "In the find field, move to the next match in the current direction",
         "在笔记内查找栏中，沿当前方向转到下一处匹配"},
    [UI_TEXT_KEYS_DESC_SEARCH_SHIFT_ENTER] =
        {"ノート内検索欄で、逆向きの一致へ",
         "In the find field, move to a match in the opposite direction",
         "在笔记内查找栏中，沿反方向转到匹配处"},
    [UI_TEXT_KEYS_DESC_SEARCH_ESCAPE] = {"ノート内検索を閉じる\n一致の選択は残ります。",
                                         "Close find in note.\nThe match stays selected.",
                                         "关闭笔记内查找。\n保留匹配项的选择。"},
    [UI_TEXT_KEYS_KEY_FILTER_RETURN] = {"Esc / Enter", "Esc / Enter", "Esc / Enter"},
    [UI_TEXT_KEYS_DESC_FILTER_RETURN] =
        {"全ノート検索欄から一覧へ戻る\n絞り込みは残ります。",
         "Return from the all-notes search field to the list.\nThe filter stays active.",
         "从全部笔记搜索栏返回列表。\n保留筛选。"},
    [UI_TEXT_KEYS_DESC_FILTER_CLEAR] =
        {"全ノート検索欄を空にすると絞り込みを解除します。絞り込み中は並び替えと折り畳み・展開がで"
         "きません。色の変更・保存・編集は使えます。",
         "Clear the all-notes search field to remove the filter. Reordering and folding are "
         "unavailable while filtering; color, save and edit remain available.",
         "清空全部笔记搜索栏即可取消筛选。筛选时不能排序或折叠、展开；仍可更改颜色、保存和编辑。"},
    [UI_TEXT_KEYS_GROUP_VIEW_KEYS] = {"一覧・閲覧中の文字キー", "Character keys in list or Read",
                                      "列表或阅读时的字符键"},
    [UI_TEXT_KEYS_KEY_COLON] = {":", ":", ":"},
    [UI_TEXT_KEYS_DESC_COLON] = {"コマンド入力を開く", "Open command input", "打开命令输入"},
    [UI_TEXT_KEYS_KEY_EDIT] = {"i", "i", "i"},
    [UI_TEXT_KEYS_DESC_EDIT] = {"編集を始める", "Start editing", "开始编辑"},
    [UI_TEXT_KEYS_KEY_FIND_FORWARD] = {"/", "/", "/"},
    [UI_TEXT_KEYS_DESC_FIND_FORWARD] = {"前方へ検索を始める", "Start a forward search",
                                        "开始向前搜索"},
    [UI_TEXT_KEYS_KEY_FIND_BACKWARD] = {"?", "?", "?"},
    [UI_TEXT_KEYS_DESC_FIND_BACKWARD] = {"後方へ検索を始める", "Start a backward search",
                                         "开始向后搜索"},
    [UI_TEXT_KEYS_KEY_REPEAT] = {"n", "n", "n"},
    [UI_TEXT_KEYS_DESC_REPEAT] = {"同じ向きで検索を繰り返す",
                                  "Repeat the search in the same direction", "沿相同方向重复搜索"},
    [UI_TEXT_KEYS_KEY_REVERSE] = {"N", "N", "N"},
    [UI_TEXT_KEYS_DESC_REVERSE] = {"逆向きで検索を繰り返す（Shift+n）",
                                   "Repeat the search in reverse (Shift+n)",
                                   "反方向重复搜索（Shift+n）"},
    [UI_TEXT_KEYS_GROUP_REPLACE] = {"置換欄", "Replace fields", "替换栏"},
    [UI_TEXT_KEYS_DESC_REPLACE_TAB] = {"パターン欄と置換欄を移る",
                                       "Move between pattern and replacement fields",
                                       "在模式栏与替换栏之间移动"},
    [UI_TEXT_KEYS_DESC_REPLACE_ONE] = {"1件置き換える", "Replace one match", "替换一处匹配"},
    [UI_TEXT_KEYS_KEY_REPLACE_ALL] = {"Ctrl+Enter", "Ctrl+Enter", "Ctrl+Enter"},
    [UI_TEXT_KEYS_DESC_REPLACE_ALL] = {"すべて置き換える", "Replace all matches", "替换全部匹配"},
    [UI_TEXT_KEYS_DESC_REPLACE_CLOSE] = {"置換欄を閉じる", "Close the replace fields",
                                         "关闭替换栏"},
    [UI_TEXT_KEYS_GROUP_HELP] = {"ヘルプ内の操作", "Inside Help", "帮助内的操作"},
    [UI_TEXT_KEYS_DESC_HELP_TAB] = {"「操作」と「キー」を切り替える",
                                    "Switch between Actions and Keys", "切换“操作”和“按键”"},
    [UI_TEXT_KEYS_DESC_HELP_PAGES] = {"「キー」の前・次のページへ", "Previous or next page in Keys",
                                      "在“按键”中翻到上一页或下一页"},
    [UI_TEXT_KEYS_DESC_HELP_SELECT] = {"「操作」の候補を選ぶ", "Select a candidate in Actions",
                                       "在“操作”中选择候选项"},
    [UI_TEXT_KEYS_DESC_HELP_RUN] = {"「操作」の選んだ候補を実行する",
                                    "Run the selected candidate in Actions",
                                    "执行“操作”中选中的候选项"},
    [UI_TEXT_KEYS_DESC_HELP_CLOSE] = {"ヘルプを閉じる", "Close Help", "关闭帮助"},
    [UI_TEXT_KEYS_GROUP_WIDTH] = {"一覧と本文の境界", "List and note boundary", "列表与正文的边界"},
    [UI_TEXT_KEYS_DESC_WIDTH_FOCUS] =
        {"「両方」で使えます。F6で一覧へ移り、Tabで境界へ移動します。ドラッグでも幅を変えられます。"
         "幅は今回の起動中だけ保持します。",
         "Available in Both view. Use F6 for the list, then Tab for the boundary. You can also "
         "drag it. Width lasts for this session.",
         "在“双栏”视图中可用。按F6移到列表，再按Tab移到边界，也可拖动调整。宽度仅在本次运行期间保留"
         "。"},
    [UI_TEXT_KEYS_KEY_WIDTH_ARROWS] = {"← / →", "← / →", "← / →"},
    [UI_TEXT_KEYS_DESC_WIDTH_ARROWS] = {"幅を少しずつ調整する", "Adjust the width in small steps",
                                        "小幅调整宽度"},
    [UI_TEXT_KEYS_DESC_WIDTH_PAGES] = {"幅を大きく調整する", "Adjust the width in larger steps",
                                       "大幅调整宽度"},
    [UI_TEXT_KEYS_KEY_WIDTH_LIMIT] = {"Home / End", "Home / End", "Home / End"},
    [UI_TEXT_KEYS_DESC_WIDTH_LIMIT] = {"最小・最大の幅へ", "Minimum or maximum width",
                                       "最小或最大宽度"},
    [UI_TEXT_KEYS_DESC_WIDTH_CANCEL] = {"ドラッグ中の幅変更を取り消す", "Cancel a width drag",
                                        "取消拖动调整宽度"},
    [UI_TEXT_KEYS_GROUP_EX] = {"コマンド入力（Ex）", "Command input (Ex)", "命令输入（Ex）"},
    [UI_TEXT_KEYS_KEY_NUMBER_ON] = {":set number", ":set number", ":set number"},
    [UI_TEXT_KEYS_DESC_NUMBER_ON] = {"編集本文の行番号を表示する",
                                     "Show line numbers in the editor", "显示编辑正文的行号"},
    [UI_TEXT_KEYS_KEY_NUMBER_OFF] = {":set nonumber", ":set nonumber", ":set nonumber"},
    [UI_TEXT_KEYS_DESC_NUMBER_OFF] = {"編集本文の行番号を隠す", "Hide line numbers in the editor",
                                      "隐藏编辑正文的行号"},
    [UI_TEXT_KEYS_KEY_NUMBER_TOGGLE] = {":set nu!", ":set nu!", ":set nu!"},
    [UI_TEXT_KEYS_DESC_NUMBER_TOGGLE] = {"編集本文の行番号を切り替える",
                                         "Toggle line numbers in the editor", "切换编辑正文的行号"},
    [UI_TEXT_KEYS_KEY_THEME_DRACULA] = {":set theme=dracula", ":set theme=dracula",
                                        ":set theme=dracula"},
    [UI_TEXT_KEYS_DESC_THEME_DRACULA] = {"テーマをDraculaにする", "Use the Dracula theme",
                                         "使用Dracula主题"},
    [UI_TEXT_KEYS_KEY_THEME_SYSTEM] = {":set theme=system", ":set theme=system",
                                       ":set theme=system"},
    [UI_TEXT_KEYS_DESC_THEME_SYSTEM] = {"テーマをOSに合わせる", "Follow the system theme",
                                        "跟随系统主题"},
    [UI_TEXT_KEYS_KEY_LANGUAGE_JA] = {":set language=ja", ":set language=ja", ":set language=ja"},
    [UI_TEXT_KEYS_DESC_LANGUAGE_JA] = {"表示を日本語にする", "Use Japanese for the interface",
                                       "界面使用日语"},
    [UI_TEXT_KEYS_KEY_LANGUAGE_EN] = {":set language=en", ":set language=en", ":set language=en"},
    [UI_TEXT_KEYS_DESC_LANGUAGE_EN] = {"表示を英語にする", "Use English for the interface",
                                       "界面使用英语"},
    [UI_TEXT_KEYS_KEY_LANGUAGE_ZH] = {":set language=zh-Hans", ":set language=zh-Hans",
                                      ":set language=zh-Hans"},
    [UI_TEXT_KEYS_DESC_LANGUAGE_ZH] = {"表示を簡体字中国語にする",
                                       "Use Simplified Chinese for the interface",
                                       "界面使用简体中文"},
    [UI_TEXT_KEYS_KEY_SUBSTITUTE] = {":%s/pattern/replacement/[g]", ":%s/pattern/replacement/[g]",
                                     ":%s/pattern/replacement/[g]"},
    [UI_TEXT_KEYS_DESC_SUBSTITUTE] =
        {"正規表現で置き換える\ngあり: すべて。gなし: 各行の最初の一致。",
         "Replace using a regular expression.\nWith g: all matches. Without g: first match per "
         "line.",
         "用正则表达式替换。\n有g：全部匹配。无g：每行第一处匹配。"},
    [UI_TEXT_KEYS_DESC_RENAME_CATEGORY] =
        {"カテゴリ名を変更する\n名前を続けて入力するか、名前入力面で指定します。",
         "Rename a category.\nAppend a name or enter it in the name dialog.",
         "重命名分类。\n在命令后输入名称，或在名称对话框中指定。"},
    [UI_TEXT_KEYS_DESC_TRASH] =
        {"ノートと履歴をごみ箱へ移す\n確認なしで実行します。",
         "Move the note and its history to the Recycle Bin.\nRuns without confirmation.",
         "将笔记和历史移到回收站。\n无需确认即执行。"},
    [UI_TEXT_KEYS_DESC_DELETE_CATEGORY] = {"空のカテゴリを削除する\n確認なしで実行します。",
                                           "Delete an empty category.\nRuns without confirmation.",
                                           "删除空分类。\n无需确认即执行。"},
    [UI_TEXT_KEYS_EX_ALIASES] = {":{name} / :{from}", ":{name} / :{from}", ":{name} / :{from}"},
    [UI_TEXT_KEYS_VALUE] = {"{name}", "{name}", "{name}"},
    [UI_TEXT_KEYS_NAME] = {"{name} — {from}: {to}", "{name} — {from}: {to}",
                           "{name} — {from}: {to}"},
    [UI_TEXT_KEYS_NOTE_NAME] = {"{name} — {to}", "{name} — {to}", "{name} — {to}"},

    [UI_TEXT_COMMAND_IMPORT] = {"このカテゴリへMarkdownを取り込む",
                                "Import Markdown into this category", "导入 Markdown 到此分类"},
    [UI_TEXT_COMMAND_EXPORT_NOTE] = {"このノートを書き出す", "Export this note", "导出此笔记"},
    [UI_TEXT_COMMAND_EXPORT_CATEGORY] = {"保存済みカテゴリをZIPに書き出す",
                                         "Export saved category as ZIP", "导出已保存的分类为 ZIP"},
    [UI_TEXT_COMMAND_EXPORT_ALL] = {"保存済み全ノートをZIPに書き出す",
                                    "Export all saved notes as ZIP", "导出全部已保存的笔记为 ZIP"},
    [UI_TEXT_IMPORT_TITLE] = {"「{name}」へ取り込み（同名はスキップ）",
                              "Import into {name} (skip existing names)",
                              "导入到“{name}”（跳过同名文件）"},
    [UI_TEXT_EXPORT_NOTE_TITLE] = {"Markdownを書き出す（編集中の内容を含む）",
                                   "Export Markdown (including unsaved edits)",
                                   "导出 Markdown（包含未保存的编辑）"},
    [UI_TEXT_EXPORT_ZIP_TITLE] = {"保存済みノートを書き出す（設定・履歴を除く）",
                                  "Export saved notes (without settings or history)",
                                  "导出已保存的笔记（不含设置和历史）"},
    [UI_TEXT_IMPORT_RESULT] = {"{n}件を追加、同名{k}件をスキップ、残り{offset}件。\n{from}",
                               "Added {n}, skipped {k} existing names, {offset} remaining.\n{from}",
                               "已添加 {n} 项，跳过同名 {k} 项，剩余 {offset} 项。\n{from}"},
    [UI_TEXT_EXPORT_DONE] = {"書き出しました。元のノートはそのままです。",
                             "Exported. The original notes are unchanged.",
                             "已导出，原笔记保持不变。"},
    [UI_TEXT_FAILURE_EXPORT_TOO_LARGE] =
        {"ZIPの上限は128MiB・65,534件です。カテゴリごとに書き出してください。",
         "ZIP export is limited to 128 MiB and 65,534 notes. Export individual categories.",
         "ZIP 导出上限为 128 MiB、65,534 条笔记，请按分类导出。"},
    [UI_TEXT_FAILURE_EXPORT_EXISTS] = {"その名前のファイルがあります。別の名前を指定してください。",
                                       "A file with that name exists. Choose a different name.",
                                       "同名文件已存在，请使用其他名称。"},
    [UI_TEXT_FAILURE_EXPORT_FAILED] =
        {"書き出せませんでした。保存先の場所と書き込み権限を確認してください。",
         "Export failed. Check the destination and write permission.",
         "导出失败，请检查目标位置和写入权限。"},
    [UI_TEXT_FAILURE_TRANSFER_UNAVAILABLE] =
        {"ファイル選択画面を開けないか、選んだ場所を扱えませんでした。",
         "The file picker or the selected path is unavailable.",
         "无法打开文件选择窗口或处理所选路径。"},
    [UI_TEXT_FILTER_MARKDOWN] = {"Markdown (*.md)", "Markdown (*.md)", "Markdown (*.md)"},
    [UI_TEXT_FILTER_ZIP] = {"ZIP (*.zip)", "ZIP (*.zip)", "ZIP (*.zip)"},

    [UI_TEXT_FAILURE_WEB_LINK_REJECTED] = {"このリンクは開けません。",
                                           "This link cannot be opened.", "无法打开此链接。"},
    [UI_TEXT_FAILURE_WEB_LINK_UNAVAILABLE] = {"既定のブラウザーでリンクを開けませんでした。",
                                              "Could not open the link in the default browser.",
                                              "无法在默认浏览器中打开此链接。"},
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
    [UI_TEXT_DRAWER_WIDTH] = {"一覧の幅", "List width", "列表宽度"},
    [UI_TEXT_DRAWER_WIDTH_HELP] =
        {"境界をドラッグして幅を調整。F6で一覧、Tabで境界へ。左右キーで微調整、PageUp/"
         "PageDownで大きく移動、Home/Endで最小/"
         "最大。ドラッグ中のEscで取消。幅は起動中だけ保持します。",
         "Drag the boundary to resize. Use F6 for the list and Tab for the boundary. Left/Right "
         "adjust the width; PageUp/PageDown take larger steps; Home/End reach the minimum/maximum. "
         "Esc cancels a drag. The width is kept for this session.",
         "拖动边界调整宽度。用F6进入列表，Tab移至边界。左右键微调，PageUp/PageDown大幅调整，Home/"
         "End设为最小/最大。拖动时按Esc取消。宽度仅在本次运行期间保留。"},
    [UI_TEXT_ACCESSIBLE_NAMED_BODY] = {"{name} — ノート本文", "{name} — Note body",
                                       "{name} — 笔记正文"},
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
    [UI_TEXT_COMMAND_FIND] = {"このノート内を検索", "Find in this note", "在本笔记中查找"},
    [UI_TEXT_COMMAND_SET] = {"設定を変える（:set number）", "Change a setting (:set number)",
                             "更改设置（:set number）"},
    [UI_TEXT_COMMAND_TOGGLE_MINIMAP] = {"ミニマップの表示を切り替える", "Toggle minimap",
                                        "切换缩略图"},
    [UI_TEXT_KEY_MINIMAP] = {"Ctrl+Shift+M", "Ctrl+Shift+M", "Ctrl+Shift+M"},
    [UI_TEXT_SETTINGS_MINIMAP] = {"ミニマップを表示", "Show minimap", "显示缩略图"},
    [UI_TEXT_MINIMAP] = {"ミニマップ", "Minimap", "缩略图"},
    [UI_TEXT_MINIMAP_HELP] =
        {"クリックやドラッグで移動。↑↓、PageUp/Down、Home/"
         "Endでも移動できます。狭い窓では自動で隠れます。",
         "Click or drag to scroll. Use ↑↓, PageUp/Down or Home/End. Hidden automatically "
         "in narrow windows.",
         "单击或拖动以滚动。也可使用 ↑↓、PageUp/Down 或 Home/End。窄窗口中自动隐藏。"},
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
    [UI_TEXT_TOOLTIP_MINIMIZE] = {"最小化", "Minimize", "最小化"},
    [UI_TEXT_TOOLTIP_MAXIMIZE] = {"最大化", "Maximize", "最大化"},
    [UI_TEXT_TOOLTIP_RESTORE] = {"元に戻す", "Restore", "还原"},
    [UI_TEXT_LAYOUT_CHOOSER] = {"表示: {name}", "View: {name}", "显示: {name}"},
    [UI_TEXT_TOOLTIP_KEY] = {"{from}{to}", "{from}{to}", "{from}{to}"},
    [UI_TEXT_TOOLTIP_OPERATIONS] = {"このノートの操作", "Actions for this note", "本笔记的操作"},
    [UI_TEXT_TOOLTIP_EDIT] = {"本文の編集を始める", "Start editing the note", "开始编辑正文"},
    [UI_TEXT_TOOLTIP_TOGGLE_MODE] = {"閲覧・編集を切り替える", "Switch between Read and Edit",
                                     "切换阅读和编辑"},
    [UI_TEXT_TOOLTIP_DESCRIPTION_KEY] = {"{name} ({from})", "{name} ({from})", "{name} ({from})"},
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
    [UI_TEXT_COMMAND_NEW_CATEGORY] = {"新しいカテゴリ", "New category", "新建分类"},
    [UI_TEXT_COMMAND_DELETE_CATEGORY] = {"カテゴリを削除", "Delete category", "删除分类"},
    [UI_TEXT_MENU_DELETE_CATEGORY] = {"カテゴリを削除", "Delete category", "删除分类"},
    [UI_TEXT_FAILURE_CATEGORY_NOT_SELECTED] = {"カテゴリを選んでください。", "Select a category.",
                                               "请选择分类。"},
    [UI_TEXT_MENU_RECOLOR] = {"色を変える…", "Change color…", "更改颜色…"},
    [UI_TEXT_MENU_RENAME_NOTE] = {"名前を変更…", "Rename…", "重命名…"},
    [UI_TEXT_MENU_NEW_CATEGORY] = {"新しいカテゴリ…", "New category…", "新建分类…"},
    [UI_TEXT_MENU_NEW_NOTE_IN_CATEGORY] = {"このカテゴリに新しいノート",
                                           "New note in this category", "在此分类中新建笔记"},
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
    [UI_TEXT_STATUS_FILTER_PREPARING] = {"準備中", "Loading", "准备中"},
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
    [UI_TEXT_THEME_GUIDANCE] = {"↑↓でプレビュー・Enterで確定・Escで戻す",
                                "↑↓ Preview · Enter Confirm · Esc Cancel",
                                "↑↓ 预览 · Enter 确认 · Esc 取消"},
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
    [UI_TEXT_SETTINGS_IMMEDIATE] = {"保存先の変更は次回起動から反映",
                                    "Storage changes apply on next launch",
                                    "保存位置更改将在下次启动时生效"},
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
    [UI_TEXT_KEY_TOGGLE_MODE] = {"Ctrl+E", "Ctrl+E", "Ctrl+E"},
    [UI_TEXT_KEY_OPERATIONS] = {"Ctrl+M", "Ctrl+M", "Ctrl+M"},
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
    [UI_TEXT_STORAGE_ENTRY] = {"ノートの保存先…", "Note storage…", "笔记保存位置…"},
    [UI_TEXT_STORAGE_TITLE] = {"ノートの保存先", "Note storage", "笔记保存位置"},
    [UI_TEXT_STORAGE_CURRENT] = {"現在の保存先", "Current folder", "当前文件夹"},
    [UI_TEXT_STORAGE_NEXT] = {"次回起動の保存先", "Folder for next launch", "下次启动的文件夹"},
    [UI_TEXT_STORAGE_FIRST_HINT] = {"ノートを保存するフォルダーを選んでください。",
                                    "Choose a folder for your notes.", "请选择笔记的保存文件夹。"},
    [UI_TEXT_STORAGE_RUNTIME_HINT] =
        {"変更は次回起動から反映します。既存のノートは移動しません。",
         "Changes take effect on the next launch. Existing notes will stay where they are.",
         "更改将在下次启动时生效。现有笔记不会被移动。"},
    [UI_TEXT_STORAGE_BROWSE] = {"参照…", "Browse…", "浏览…"},
    [UI_TEXT_STORAGE_DEFAULT] = {"既定の場所", "Default folder", "默认文件夹"},
    [UI_TEXT_STORAGE_ACCEPT] = {"次回から使う", "Use on next launch", "下次启动时使用"},
    [UI_TEXT_STORAGE_FIRST_ACCEPT] = {"この場所を使う", "Use this folder", "使用此文件夹"},
    [UI_TEXT_STORAGE_PENDING] = {"保存先の変更は次回起動から有効です。",
                                 "The new folder will be used on the next launch.",
                                 "新的保存位置将在下次启动时使用。"},
    [UI_TEXT_STORAGE_INVALID_PATH] =
        {"フォルダーの絶対パスを入力してください。相対パスや「..」は使えません。",
         "Enter an absolute folder path. Relative paths and '..' are not supported.",
         "请输入文件夹的绝对路径。不支持相对路径和“..”。"},
    [UI_TEXT_STORAGE_LOCATION_UNAVAILABLE] =
        {"保存先の初期位置を取得できませんでした。",
         "The default storage location could not be determined.", "无法获取默认保存位置。"},
    [UI_TEXT_STORAGE_DIRECTORY_MISSING] =
        {"保存先が見つかりません。接続を確認するか、使う場所を選んでください。",
         "The folder is missing. Check its connection or choose another location.",
         "找不到保存文件夹。请检查连接或选择其他位置。"},
    [UI_TEXT_STORAGE_DIRECTORY_UNREADABLE] =
        {"このフォルダーを読み取れません。権限と接続を確認してください。",
         "This folder cannot be read. Check permissions and the connection.",
         "无法读取此文件夹。请检查权限和连接。"},
    [UI_TEXT_STORAGE_DIRECTORY_UNWRITABLE] =
        {"このフォルダーへ保存できません。書き込み権限と親フォルダーを確認してください。",
         "This folder is not writable. Check write permissions and its parent folder.",
         "无法写入此文件夹。请检查写入权限和上级文件夹。"},
    [UI_TEXT_STORAGE_DIRECTORY_IN_USE] =
        {"この保存先は別のNeNe Folioが使用中です。閉じてから選び直してください。",
         "Another NeNe Folio is using this folder. Close it before choosing the folder again.",
         "另一个NeNe Folio正在使用此文件夹。请先关闭它，再重新选择。"},
    [UI_TEXT_STORAGE_SETTINGS_UNREADABLE] =
        {"nenefolio-startup.jsonを読み取れません。起動設定の権限を確認してください。",
         "nenefolio-startup.json cannot be read. Check its permissions.",
         "无法读取nenefolio-startup.json。请检查权限。"},
    [UI_TEXT_STORAGE_SETTINGS_MALFORMED] =
        {"nenefolio-startup.jsonの形式が不正です。ファイルを保全して修復してください。",
         "nenefolio-startup.json is invalid. Preserve the file before repairing it.",
         "nenefolio-startup.json格式无效。请保留文件后再修复。"},
    [UI_TEXT_STORAGE_SETTINGS_UNSUPPORTED] =
        {"nenefolio-startup.jsonは未対応の版です。対応する新しいNeNe Folioで開いてください。",
         "This version of nenefolio-startup.json is not supported. Use a compatible newer NeNe "
         "Folio.",
         "不支持此版本的nenefolio-startup.json。请使用兼容的新版NeNe Folio。"},
    [UI_TEXT_STORAGE_SETTINGS_STORE_FAILED] =
        {"保存先の設定を書き込めませんでした。以前の設定を保っています。",
         "The storage setting could not be saved. The previous setting is unchanged.",
         "无法保存存储设置。原设置未更改。"},
    [UI_TEXT_STORAGE_SETTINGS_CHANGED] =
        {"別の起動中アプリが保存先設定を変更しました。上書きせず、再起動して読み直してください。",
         "Another running instance changed the storage setting. Restart to reload it; it was not "
         "overwritten.",
         "另一个实例更改了保存位置。未覆盖该设置，请重启以重新读取。"},
    [UI_TEXT_STORAGE_PICKER_FAILED] =
        {"フォルダー選択を開けませんでした。パスを直接入力できます。",
         "The folder picker could not be opened. You can enter the path directly.",
         "无法打开文件夹选择窗口。可以直接输入路径。"},
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
