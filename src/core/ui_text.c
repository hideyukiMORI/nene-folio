#include "ui_text.h"

#include "ui_text_request.h"

#include <string.h>

/* 利用者に見える文言の唯一の表（ADR 0030 の決定 1）。日本語の 1 列で、単位 C が
 * [UI_TEXT_X] = {"…", "…", "…"} の 2 次元にする（ADR 0029 の決定 4）。
 * 文言は #74 の前のソースから機械的に写した（out/design/2026-09-22/ui-text-probe/）。
 * 1 字でも変えるときは同じコミットで tests/unit/ui_text_tests.c の期待表も変える。 */
static const char *_Nonnull const catalog[] = {
    [UI_TEXT_EMPTY] = "",
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
    [UI_TEXT_FAILURE_NOTE_MALFORMED] = "編集中の本文に壊れた文字があります。保存していません。",
    [UI_TEXT_FAILURE_NOTE_STORE_FAILED] =
        "ノートを書き戻せませんでした。編集中の本文はそのままです。",
    [UI_TEXT_FAILURE_HISTORY_FAILED] =
        "履歴を書けなかったので保存していません。編集中の本文は残っています。",
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
        "名前の変更が途中で止まっています。同じ名前変更をやり直してください。",
    [UI_TEXT_FAILURE_RENAME_UNLOCKED] =
        "data/ に書けないため名前を変更できません。何も変えていません。",
    [UI_TEXT_FAILURE_RENAME_UNSUPPORTED] = "この data/ ではノート名を変更できません（ローカルの "
                                           "NTFS 以外、またはシンボリックリンク／junction）。",
    [UI_TEXT_FAILURE_RENAME_IDENTITY_FAILED] =
        "元のファイルを確かめられないので名前を変更できません。何も変えていません。",
    [UI_TEXT_FAILURE_RENAME_JOURNAL_FAILED] =
        "名前変更の記録（data/.rename.json）を書けませんでした。何も変えていません。",
    [UI_TEXT_FAILURE_RENAME_JOURNAL_BROKEN] =
        "名前変更の記録（data/.rename.json）が版 1 の形ではありません。消していません。",
    [UI_TEXT_FAILURE_RENAME_HALTED] = "名前変更の記録と実ファイルが一致しません。data/.rename.json "
                                      "と data/<カテゴリ>/ を確認してください。",
    [UI_TEXT_FAILURE_SEARCH_MALFORMED] = "検索する語に壊れた文字があります。語は前のままです。",
    [UI_TEXT_FAILURE_FILTERED] = "絞り込み中は並び替えと開閉ができません。",
    [UI_TEXT_FAILURE_SETTINGS_UNREADABLE] =
        "設定（data/settings.json）を読めません。既定値で始め、直すまで上書きしません。",
    [UI_TEXT_FAILURE_SETTINGS_STORE_FAILED] =
        "設定（data/settings.json）を書けませんでした。設定は変えていません。",
    [UI_TEXT_FAILURE_PANE_UNAVAILABLE] = "表示中の本文を取り出せませんでした。探していません。",
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
    [UI_TEXT_FAILURE_REPLACE_STALE] =
        "本文か入力が変わったので、この置換は当てられません。もう一度入力してください。",
    [UI_TEXT_FAILURE_REPLACE_BAD_SPAN] = "選択範囲が正しくありません。本文は変えていません。",
    [UI_TEXT_FAILURE_OUT_OF_MEMORY] = "記憶域が足りません。",
    [UI_TEXT_FAILURE_NAME_REQUIRED] =
        "無題のノートに名前をつけて保存してください。本文は残っています。",
    [UI_TEXT_FAILURE_INVALID_NAME] = "使えない名前です。予約名・末尾の空白やピリオド・区切りを避け"
                                     "、.mdを含め255バイト以内で指定してください。",
    [UI_TEXT_FAILURE_ALREADY_NAMED] = "このノートには名前があります。別名保存（:"
                                      "saveas）または名前変更（:rename）を使ってください。",
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
    [UI_TEXT_COMMAND_FIND] = "このノート内を検索",
    [UI_TEXT_COMMAND_SET] = "設定を変える（:set number）",
    [UI_TEXT_COMMAND_TOGGLE_NUMBER] = "行番号の表示を切り替える",
    [UI_TEXT_COMMAND_REPLACE] = "置換",
    [UI_TEXT_COMMAND_SUBSTITUTE] = "正規表現で置換（:%s/前/後/g）",
    [UI_TEXT_COMMAND_SETTINGS] = "設定",
    [UI_TEXT_HELP_PALETTE] = "一覧  ↑↓ 選択 / Enter 実行 / Esc 戻る / Tab 説明",
    [UI_TEXT_HELP_EDITOR_MOTION] = "編集本文  Ctrl+h/j/k/l ←/↓/↑/→",
    [UI_TEXT_HELP_GLOBAL_KEYS] = "全区画  F1 ヘルプ / Ctrl+P 一覧 / Ctrl+F このノート内を検索",
    [UI_TEXT_HELP_GLOBAL_FILTER] = "全区画  Ctrl+Shift+F すべてのノートを検索（索引を絞り込む）",
    [UI_TEXT_HELP_FILTER_FIELD] =
        "絞り込み欄  Esc・Enter 索引へ戻る（絞り込みは残る）/ 空にすると解除",
    [UI_TEXT_HELP_FILTER_LIMITS] =
        "絞り込み中  並び替えと折畳/展開はできない（色・保存・編集は可）",
    [UI_TEXT_HELP_FILE_KEYS] =
        "入力欄でも  Ctrl+N 新規 / Ctrl+S 保存 / Ctrl+Shift+S 別名保存 / F2 名前変更",
    [UI_TEXT_HELP_INDEX_COMMAND] = "索引・閲覧本文  : コマンド / i 編集",
    [UI_TEXT_HELP_EX_SET_NUMBER] =
        "Ex  :set number / :set nonumber / :set nu!（編集中の原文の行番号）",
    [UI_TEXT_HELP_REPLACE_FIELD] =
        "置換欄  Tab 欄を移動 / Enter 1 件 / Ctrl+Enter すべて / Esc 閉じる",
    [UI_TEXT_HELP_EX_SUBSTITUTE] =
        "Ex  :%s/パターン/置換/[g]（g なしは各行の最初の一致・正規表現）",
    [UI_TEXT_HELP_SEARCH_KEYS] = "索引・閲覧本文  / 次を検索 / ? 前を検索 / n・N 繰り返し",
    [UI_TEXT_HELP_SEARCH_STEP] = "全区画  F3 次の一致 / Shift+F3 前の一致（向きは変えない）",
    [UI_TEXT_HELP_SEARCH_FIELD] = "検索欄  Enter 次 / Shift+Enter 逆 / Esc 閉じる（選択は残る）",
    [UI_TEXT_HELP_INDEX_MOTION] = "索引  j/k 次/前 / gg/G 先頭/末尾",
    [UI_TEXT_HELP_INDEX_FOLD] = "索引  h/l 折畳/展開 / Enter 本文",
    [UI_TEXT_HELP_INDEX_SCROLL] = "索引  ↑↓ / PgUp/PgDn スクロール",
    [UI_TEXT_HELP_EDITOR_ESCAPE] = "本文  Esc 保存して索引へ（編集は維持）",
    [UI_TEXT_TITLE_UNTITLED] = "無題（未保存）",
    [UI_TEXT_TITLE_RECOVERING] = "名前変更の復旧待ち",
    [UI_TEXT_CHIP_VIEW] = "閲覧",
    [UI_TEXT_CHIP_EDIT] = "編集",
    [UI_TEXT_PLACEHOLDER_FILTER] = "すべてのノートを検索",
    [UI_TEXT_STATUS_SEARCH_FORWARD] = "このノート内を検索 ／ 次へ  ",
    [UI_TEXT_STATUS_SEARCH_BACKWARD] = "このノート内を検索 ／ 前へ  ",
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
    [UI_TEXT_STATUS_PALETTE_PAGES] = "{k}/{n}",
    [UI_TEXT_ACTION_KEYS_HIDE] = "キー操作を閉じる",
    [UI_TEXT_ACTION_KEYS_SHOW] = "キー操作を表示",
    [UI_TEXT_ACTION_PALETTE_PREVIOUS] = "↑ 前",
    [UI_TEXT_ACTION_PALETTE_NEXT] = "↓ 次",
    [UI_TEXT_ACTION_OPERATIONS] = "操作 ▾",
    [UI_TEXT_SETTINGS_THEME] = "テーマ",
    [UI_TEXT_SETTINGS_THEME_SYSTEM] = "OS に従う",
    [UI_TEXT_SETTINGS_THEME_LIGHT] = "ライト",
    [UI_TEXT_SETTINGS_THEME_DARK] = "ダーク",
    [UI_TEXT_SETTINGS_GUIDANCE] = "↑↓ 選択 / Enter 採用 / Esc 戻る",
    [UI_TEXT_APP_LOGO] = "NENE FOLIO",
    [UI_TEXT_GLYPH_MINUS] = "−",
    [UI_TEXT_GLYPH_PLUS] = "+",
    [UI_TEXT_PROMPT_PENDING_EXPLANATION] =
        "「再試行」で同じ名前変更を続けます。「閉じる」は取り消しではありません。",
    [UI_TEXT_PROMPT_PENDING_RENAME] = "{from} → {to}",
    [UI_TEXT_PROMPT_TITLE_FIRST_SAVE] = "名前をつけて保存",
    [UI_TEXT_PROMPT_TITLE_SAVE_AS] = "別名で保存",
    [UI_TEXT_PROMPT_TITLE_RENAME] = "名前を変更",
    [UI_TEXT_PROMPT_HINT_PENDING] = "この名前変更を最後まで終えるまで、ほかの操作へ進めません。",
    [UI_TEXT_PROMPT_HINT_FIRST_SAVE] = "名前の末尾に .md を補います。",
    [UI_TEXT_PROMPT_HINT_SAVE_AS] = "元の保存内容を保ち、別の .md を作ります。",
    [UI_TEXT_PROMPT_HINT_RENAME] = "md と履歴を新しい名前へ移します。",
    [UI_TEXT_PROMPT_ACCEPT_RETRY] = "再試行",
    [UI_TEXT_PROMPT_ACCEPT_SAVE] = "保存",
    [UI_TEXT_PROMPT_ACCEPT_RENAME] = "変更",
    [UI_TEXT_PROMPT_CLOSE_PENDING] = "閉じる",
    [UI_TEXT_PROMPT_CLOSE_CANCEL] = "キャンセル",
    [UI_TEXT_PROMPT_LABEL_NAME] = "ノートの名前",
    [UI_TEXT_PROMPT_LABEL_CATEGORY] = "保存先カテゴリ",
    [UI_TEXT_APP_NO_MODULE_PATH] = "実行ファイルの場所が長すぎるか、取得できません。",
    [UI_TEXT_APP_DATA_IN_USE] = "同じ data/ を別の NeNe Folio が使っています。先に閉じてください。",
    [UI_TEXT_APP_RECOVERY_LOCKED] = "名前変更の復旧記録（data/.rename.json）がありますが、data/ "
                                    "を占有できないので復旧できません。",
    [UI_TEXT_APP_OUT_OF_MEMORY] = "記憶域が足りません。",
    [UI_TEXT_APP_NO_WINDOW] = "窓を作れませんでした。",
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
    /* 言語は引数で受けるが、この単位では列が 1 つなので添字には使わない（決定 1）。 */
    (void)language;
    return catalog[id];
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
