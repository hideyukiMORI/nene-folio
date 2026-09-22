/* 文言の正本の単体（FR なし・ADR 0030 の「検証」）。
 * 期待表は **#74 の前のソースから機械的に写した**もので、ui_text の表と 1 字ずつ突き合わせる
 * （#65 と同じ手順。道具は out/design/2026-09-22/ui-text-probe/）。
 *
 * **どこにも残らない文言が 2 つある**ので期待表には無い。どちらも全値の switch の後ろの
 * 到達しない既定で、表示されたことが無い:
 *   - "このノート内を検索  "（folio_window.c の search_direction_label）
 *   - "実行ファイルの場所が取得できません。"（main.c の adapter_failure）
 * 確保に失敗したときの退避の文言（name_prompt.c の「エラー表示の記憶域が不足しています。…」）も、
 * 確保そのものが無くなるので消える（決定 5）。 */
#include "folio_language.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "unit_tests.h"
#include "utf16_text.h"

#include <string.h>

/* 変更前の文言（機械的に写した）。ui_text.c の表と 1 字も違ってはいけない。 */
static const char *_Nonnull const expected[] = {
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
    /* #69（main）が足したヘルプのページ送りの札。変更前の #74 の木には無く、
     * 連結で作っていた「1/2」を置換子に移した（取り込みのときの 1 件）。 */
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
    require(expected_count == UI_TEXT_APP_NO_WINDOW + 1,
            "the expected table covers every id in the enumeration");
    for (size_t index = 0; index < expected_count; ++index)
    {
        const char *_Nonnull actual = ui_text_line((enum ui_text)index, FOLIO_LANGUAGE_JA);
        require(same_text(actual, expected[index]), "the catalog line is byte for byte the same");
    }
}

/* 空なのは UI_TEXT_EMPTY だけ。全 ID が上限に収まる。 */
static void verify_lines(void)
{
    for (size_t index = 0; index < expected_count; ++index)
    {
        const char *_Nonnull line = ui_text_line((enum ui_text)index, FOLIO_LANGUAGE_JA);
        require((index == UI_TEXT_EMPTY) == (line[0] == '\0'),
                "only UI_TEXT_EMPTY is the empty string");
        require(utf16_units_of(line) <= ui_text_unit_limit, "the line fits in UTF-16");
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

static void verify_placeholders(void)
{
    for (size_t index = 0; index < expected_count; ++index)
    {
        const char *_Nonnull line = ui_text_line((enum ui_text)index, FOLIO_LANGUAGE_JA);
        for (size_t at = 0; line[at] != '\0'; ++at)
        {
            require(line[at] != '}' || at > 0, "a closing brace never starts a line");
            require(line[at] != '{' || known_at(line, at),
                    "every brace in the catalog starts one of the six placeholders");
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

static void verify_language(void)
{
    /* この単位では 1 値だけ。単位 C が値を足したら表が 2 次元になる（ADR 0029 の決定 4）。 */
    require(ui_text_line(UI_TEXT_COMMAND_SAVE, FOLIO_LANGUAGE_JA) ==
                ui_text_line(UI_TEXT_COMMAND_SAVE, FOLIO_LANGUAGE_JA),
            "the same id and language give the same line");
}

void run_ui_text_tests(void)
{
    verify_identity();
    verify_lines();
    verify_placeholders();
    verify_format();
    verify_format_limits();
    verify_fill();
    verify_language();
}
