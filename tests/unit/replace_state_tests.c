/* 置換の下見と適用（FR-023 / ADR 0028 の決定 6）。偽の正規表現ポートで、application が
 * 何を覚え、何を断り、どの結果へ写すかだけを測る。ICU はここには現れない。 */
#include "appearance_port.h"
#include "folio_state.h"
#include "persistence_port.h"
#include "regex_matches.h"
#include "regex_port.h"
#include "regex_request.h"
#include "replace_edit.h"
#include "unit_tests.h"

#include <string.h>

/* 偽の adapter。application が不完全型としてしか知らない型をここで定義する（C-006）。 */
struct regex_adapter
{
    enum regex_scan_outcome outcome; /* READY 以外なら走査せずにそれを返す */
    size_t offset;                   /* BAD_PATTERN のときに報せる位置 */
    size_t scans;                    /* 走った回数（2 周の確認） */
};

static struct regex_adapter script = {.outcome = REGEX_SCAN_READY, .offset = 0, .scans = 0};

/* パターンを素の語として重ならずに探す。判断は ICU の仕事なので偽物は literal だけを見る。 */
static size_t collect(const struct regex_request *_Nonnull request,
                      struct regex_matches *_Nonnull matches)
{
    size_t width = request->pattern_length;
    size_t count = 0;
    size_t at = 0;
    while (width > 0 && at + width <= request->length)
    {
        if (memcmp(request->text + at, request->pattern, width * sizeof *request->text) != 0)
        {
            at += 1;
            continue;
        }
        if (count < matches->capacity)
        {
            struct regex_match match = {.whole = {.start = at, .end = at + width}, .groups = {}};
            match.groups[0].span = match.whole; /* 群 1 は一致の全体 */
            match.groups[0].present = true;
            matches->items[count] = match;
        }
        count += 1;
        at += width;
    }
    return count;
}

static enum regex_scan_outcome fake_scan(struct regex_adapter *_Nonnull adapter,
                                         const struct regex_request *_Nonnull request,
                                         struct regex_matches *_Nonnull matches,
                                         struct regex_pattern_error *_Nonnull error)
{
    adapter->scans += 1;
    matches->count = 0;
    error->offset = 0;
    if (adapter->outcome != REGEX_SCAN_READY)
    {
        error->offset = adapter->offset;
        return adapter->outcome;
    }
    matches->count = collect(request, matches);
    return REGEX_SCAN_READY;
}

struct regex_port test_regex_port(void)
{
    struct regex_port port = {.adapter = &script, .scan = fake_scan};
    return port;
}

static void reset_script(void)
{
    script.outcome = REGEX_SCAN_READY;
    script.offset = 0;
    script.scans = 0;
}

static const char *const categories_text =
    "{\"version\": 1, \"categories\": ["
    "{\"name\": \"A\", \"color\": \"#112233\", \"expanded\": true}]}";
static const char *const notes_text = "{\"version\": 1, \"notes\": [\"one\", \"two\"]}";

static size_t units_length(const char16_t *_Nonnull units)
{
    size_t length = 0;
    while (units[length] != u'\0')
    {
        length += 1;
    }
    return length;
}

static bool same_units(const char16_t *_Nonnull actual, const char16_t *_Nonnull expected)
{
    size_t index = 0;
    while (actual[index] != u'\0' && actual[index] == expected[index])
    {
        index += 1;
    }
    return actual[index] == expected[index];
}

static struct replace_request request_for(const char16_t *_Nonnull text,
                                          const char16_t *_Nonnull pattern,
                                          const char16_t *_Nonnull replacement)
{
    struct replace_request request = {.text = text,
                                      .length = units_length(text),
                                      .pattern = pattern,
                                      .pattern_length = units_length(pattern),
                                      .replacement = replacement,
                                      .replacement_length = units_length(replacement)};
    return request;
}

static struct replace_apply apply_for(const char16_t *_Nonnull text, enum replace_scope scope,
                                      size_t anchor)
{
    struct replace_apply apply = {.text = text,
                                  .length = units_length(text),
                                  .anchor = {.start = anchor, .end = anchor},
                                  .scope = scope};
    return apply;
}

/* 1 つのノートを開いて編集中にした state。adapter は呼び出し側が生かし続ける。 */
static struct folio_state *_Nonnull edited_state(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = test_adapter_port(adapter);
    struct appearance_port looks = test_appearance_port();
    struct regex_port finder = test_regex_port();
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "the state takes four ports");
    require(state != nullptr, "the state is owned");
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "a note is selected");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "the note is being edited");
    return state;
}

static void verify_guards(void)
{
    reset_script();
    struct persistence_adapter *adapter = test_adapter_create(categories_text, notes_text);
    struct persistence_port port = test_adapter_port(adapter);
    struct appearance_port looks = test_appearance_port();
    struct regex_port finder = test_regex_port();
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state");
    struct replace_request request = request_for(u"a-a", u"a", u"X");
    /* 閲覧中は下見も適用も断る（決定 8(c)）。 */
    require(folio_state_preview_replace(state, &request) == FOLIO_STATE_NOT_EDITING,
            "viewing refuses the preview");
    struct replace_apply apply = apply_for(u"a-a", REPLACE_ALL, 0);
    struct replace_edit *edit = nullptr;
    require(folio_state_apply_replace(state, &apply, &edit) == FOLIO_STATE_NOT_EDITING,
            "viewing refuses to apply");
    require(edit == nullptr && script.scans == 0, "nothing was scanned");
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "edit");
    /* 下見が無いあいだは適用できない（決定 6）。 */
    require(folio_state_apply_replace(state, &apply, &edit) == FOLIO_STATE_REPLACE_STALE,
            "applying without a preview is stale");
    struct replace_request empty = request_for(u"a-a", u"", u"X");
    require(folio_state_preview_replace(state, &empty) == FOLIO_STATE_REPLACE_NO_PATTERN,
            "an empty pattern is refused before the port is called");
    require(script.scans == 0, "an empty pattern never reaches the adapter");
    require(folio_state_replace_count(state) == 0, "no preview means no matches");
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
}

/* 走査の失敗はすべて閉じた値へ写り、**前の下見を捨てる**（決定 6 の補足・レビュー B1）。
 * 下見は常に「最後の入力の結果」か「無い」で、無ければ適用は STALE になる。 */
static void verify_failures(void)
{
    static const struct
    {
        enum regex_scan_outcome scanned;
        enum folio_state_outcome expected;
    } table[] = {
        {REGEX_SCAN_BAD_PATTERN, FOLIO_STATE_REPLACE_BAD_PATTERN},
        {REGEX_SCAN_TIMED_OUT, FOLIO_STATE_REPLACE_TIMED_OUT},
        {REGEX_SCAN_TOO_COMPLEX, FOLIO_STATE_REPLACE_TOO_COMPLEX},
        {REGEX_SCAN_TOO_MANY, FOLIO_STATE_REPLACE_TOO_MANY},
        {REGEX_SCAN_OUT_OF_MEMORY, FOLIO_STATE_OUT_OF_MEMORY},
    };
    struct persistence_adapter *adapter = test_adapter_create(categories_text, notes_text);
    struct folio_state *state = edited_state(adapter);
    struct replace_request request = request_for(u"a-a", u"a", u"X");
    struct replace_apply all = apply_for(u"a-a", REPLACE_ALL, 0);
    for (size_t index = 0; index < sizeof table / sizeof table[0]; ++index)
    {
        reset_script();
        require(folio_state_preview_replace(state, &request) == FOLIO_STATE_READY,
                "a good preview");
        require(folio_state_replace_count(state) == 2, "two matches, zero width counted alike");
        script.outcome = table[index].scanned;
        script.offset = 7;
        require(folio_state_preview_replace(state, &request) == table[index].expected,
                "every scan failure maps to one closed value");
        struct replace_edit *dropped = nullptr;
        require(folio_state_replace_count(state) == 0 &&
                    folio_state_apply_replace(state, &all, &dropped) == FOLIO_STATE_REPLACE_STALE &&
                    dropped == nullptr,
                "a failed preview drops the previous one; apply then returns STALE");
    }
    require(folio_state_replace_error_offset(state) == 0,
            "only a bad pattern leaves a position behind");
    script.outcome = REGEX_SCAN_BAD_PATTERN;
    script.offset = 3;
    require(folio_state_preview_replace(state, &request) == FOLIO_STATE_REPLACE_BAD_PATTERN,
            "a bad pattern");
    require(folio_state_replace_error_offset(state) == 3, "the position comes from the adapter");
    reset_script();
    /* 壊れた置換文字列は走査のあとで断られ、位置は残らない（決定 5 / 9）。 */
    struct replace_request broken = request_for(u"a-a", u"a", u"\\q");
    require(folio_state_preview_replace(state, &broken) == FOLIO_STATE_REPLACE_BAD_TEMPLATE,
            "a bad replacement text is refused");
    require(folio_state_replace_error_offset(state) == 0, "a good pattern clears the position");
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
}

static void expect_applied(struct folio_state *_Nonnull state,
                           const struct replace_apply *_Nonnull apply,
                           const char16_t *_Nonnull units)
{
    struct replace_edit *edit = nullptr;
    require(folio_state_apply_replace(state, apply, &edit) == FOLIO_STATE_READY, "applied");
    require(edit != nullptr, "an edit is owned");
    require(same_units(replace_edit_units(edit), units), "the new text");
    replace_edit_destroy(edit);
}

static void verify_apply(void)
{
    reset_script();
    struct persistence_adapter *adapter = test_adapter_create(categories_text, notes_text);
    struct folio_state *state = edited_state(adapter);
    struct replace_request request = request_for(u"a-a-a", u"a", u"[\\1]");
    require(folio_state_preview_replace(state, &request) == FOLIO_STATE_READY, "preview");
    require(folio_state_replace_count(state) == 3, "three matches");
    struct replace_apply all = apply_for(u"a-a-a", REPLACE_ALL, 0);
    expect_applied(state, &all, u"[a]-[a]-[a]");
    /* 1 件は anchor の開始以降で最初の一致。範囲は一致そのもの（決定 6 / 7）。 */
    struct replace_apply one = apply_for(u"a-a-a", REPLACE_ONE, 1);
    struct replace_edit *edit = nullptr;
    require(folio_state_apply_replace(state, &one, &edit) == FOLIO_STATE_READY, "one applied");
    require(edit != nullptr && replace_edit_span(edit).start == 2 &&
                replace_edit_span(edit).end == 3,
            "the span is the match, not the whole body");
    replace_edit_destroy(edit);
    /* `g` の無い `:%s` は各論理行の最初の一致だけ（決定 8(b)）。 */
    struct replace_request lines = request_for(u"a-a\ra-a", u"a", u"Z");
    require(folio_state_preview_replace(state, &lines) == FOLIO_STATE_READY, "preview over lines");
    struct replace_apply first = apply_for(u"a-a\ra-a", REPLACE_LINE_FIRST, 0);
    expect_applied(state, &first, u"Z-a\rZ-a");
    /* 一致が無ければ成功のまま何も渡さない。 */
    struct replace_request none = request_for(u"a-a-a", u"zz", u"X");
    require(folio_state_preview_replace(state, &none) == FOLIO_STATE_READY, "no match preview");
    require(folio_state_replace_count(state) == 0, "no matches");
    edit = nullptr;
    require(folio_state_apply_replace(state, &all, &edit) == FOLIO_STATE_READY, "nothing to do");
    require(edit == nullptr, "no edit is handed back");
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
}

/* 宛先と本文の照合（決定 6）。捨てる契機を数え上げずに古い下見を無効にする。 */
static void verify_stale(void)
{
    reset_script();
    struct persistence_adapter *adapter = test_adapter_create(categories_text, notes_text);
    struct folio_state *state = edited_state(adapter);
    struct replace_request request = request_for(u"a-a", u"a", u"X");
    require(folio_state_preview_replace(state, &request) == FOLIO_STATE_READY, "preview");
    struct replace_apply changed = apply_for(u"a-b", REPLACE_ALL, 0);
    struct replace_edit *edit = nullptr;
    require(folio_state_apply_replace(state, &changed, &edit) == FOLIO_STATE_REPLACE_STALE,
            "a body that no longer matches the copy is refused");
    struct replace_apply longer = apply_for(u"a-a-", REPLACE_ALL, 0);
    require(folio_state_apply_replace(state, &longer, &edit) == FOLIO_STATE_REPLACE_STALE,
            "a longer body is refused too");
    /* 反転した anchor は黙って直さず拒む。 */
    struct replace_apply inverted = apply_for(u"a-a", REPLACE_ONE, 0);
    inverted.anchor.start = 2;
    inverted.anchor.end = 1;
    require(folio_state_apply_replace(state, &inverted, &edit) == FOLIO_STATE_REPLACE_BAD_SPAN,
            "an inverted anchor is refused");
    require(edit == nullptr, "nothing was built");
    /* 同じ本文でも宛先が違えば当てない（別のノートへ同じ文字列が入っている場合）。 */
    require(folio_state_select_note(state, 0, 1) == FOLIO_STATE_READY, "another note");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "editing it");
    struct replace_apply same = apply_for(u"a-a", REPLACE_ALL, 0);
    require(folio_state_apply_replace(state, &same, &edit) == FOLIO_STATE_REPLACE_STALE,
            "the same body in another note is refused");
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
}

/* 総数が入れ物を超えたら広げて 1 度だけ走査し直す（決定 2）。 */
static void verify_second_pass(void)
{
    reset_script();
    static char16_t text[200];
    for (size_t index = 0; index < sizeof text / sizeof text[0] - 1; ++index)
    {
        text[index] = u'a';
    }
    struct persistence_adapter *adapter = test_adapter_create(categories_text, notes_text);
    struct folio_state *state = edited_state(adapter);
    struct replace_request request = request_for(text, u"a", u"b");
    require(folio_state_preview_replace(state, &request) == FOLIO_STATE_READY, "preview");
    require(folio_state_replace_count(state) == 199, "every unit matched");
    require(script.scans == 2, "the first pass counted, the second pass filled");
    struct replace_apply all = apply_for(text, REPLACE_ALL, 0);
    struct replace_edit *edit = nullptr;
    require(folio_state_apply_replace(state, &all, &edit) == FOLIO_STATE_READY, "applied");
    require(edit != nullptr && replace_edit_length(edit) == 199, "all of them were replaced");
    replace_edit_destroy(edit);
    /* 一度広げた入れ物は次からもう 1 周しない。 */
    script.scans = 0;
    require(folio_state_preview_replace(state, &request) == FOLIO_STATE_READY, "preview again");
    require(script.scans == 1, "the grown buffer holds them all");
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
    reset_script();
}

void run_replace_state_tests(void)
{
    verify_guards();
    verify_failures();
    verify_apply();
    verify_stale();
    verify_second_pass();
}
