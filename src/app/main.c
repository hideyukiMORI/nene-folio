/* 合成ルート（ARC-006）。ポートに実装を結び、窓を起動し、メッセージループと終了コードを所有する。
 * 出してよいのは「起動できなかった理由」と「起動は続けるが設定を読めなかった知らせ」の 1 行だけ
 * （FR-015 / ADR 0025 の決定 6）。どちらも文言は application が作る。 */
#include "appearance_adapter.h"
#include "folio_state.h"
#include "folio_window.h"
#include "persistence_adapter.h"
#include "regex_adapter.h"
#include "ui_text.h"
#include "utf16_text.h"

#include <string.h>
#include <windows.h>

static const wchar_t title[] = L"NeNe Folio";

/* UTF-8 の 1 行を確保せずに UTF-16 へ写して見せる（ADR 0030 の決定 5）。
 * 写せなければ何も出さない（表の値が上限に収まることは単体が全 ID を回して固定する）。 */
static void report(const char *_Nonnull line)
{
    char16_t units[ui_text_unit_limit];
    size_t written = 0;
    if (utf16_text_fill(line, units, ui_text_unit_limit, &written) != UTF16_TEXT_FILL_READY)
    {
        return;
    }
    MessageBoxW(nullptr, units, title, MB_OK | MB_ICONERROR);
}

/* アダプタを作れなかった理由ごとの文言の ID（ADR 0030 の決定 5）。文言は core の ui_text が
 * 持ち、ここは対応だけを持つ。全値がちょうど 1 度ずつ並ぶことは CNF-009 が守る。 */
static const enum ui_text adapter_lines[] = {
    [PERSISTENCE_ADAPTER_CREATED] = UI_TEXT_EMPTY,
    [PERSISTENCE_ADAPTER_NO_MODULE_PATH] = UI_TEXT_APP_NO_MODULE_PATH,
    [PERSISTENCE_ADAPTER_DATA_IN_USE] = UI_TEXT_APP_DATA_IN_USE,
    [PERSISTENCE_ADAPTER_RECOVERY_LOCKED] = UI_TEXT_APP_RECOVERY_LOCKED,
    [PERSISTENCE_ADAPTER_OUT_OF_MEMORY] = UI_TEXT_APP_OUT_OF_MEMORY,
};

static void run_message_loop(const struct folio_window *_Nonnull window)
{
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!folio_window_translate(window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

/* 状態と窓を作って走らせる。アダプタは状態より長く生きる。 */
static int run(struct persistence_adapter *_Nonnull persistence,
               struct appearance_adapter *_Nonnull appearance, struct regex_adapter *_Nonnull regex)
{
    struct persistence_port files = persistence_adapter_port(persistence);
    struct appearance_port looks = appearance_adapter_port(appearance);
    struct regex_port finder = regex_adapter_port(regex);
    struct folio_ports ports = {.persistence = &files, .appearance = &looks, .regex = &finder};
    struct folio_state *_Nullable state = nullptr;
    enum folio_state_outcome loaded = folio_state_create(&ports, &state);
    if (loaded != FOLIO_STATE_READY)
    {
        /* 状態を作れていないので、どの言語で見せるかを尋ねる相手がまだいない（ADR 0030 の補正 7）。
         */
        report(folio_state_failure_line(loaded, FOLIO_LANGUAGE_JA));
        return 1;
    }
    /* 起動は続けるが、設定を読めなかった理由は窓を作る前に 1 回だけ見せる（ADR 0025 の決定 6）。
     * main は 1 回しか走らないので「見せたか」の真偽はどこにも持たない。 */
    enum folio_state_outcome settings = folio_state_settings_notice(state);
    if (settings != FOLIO_STATE_READY)
    {
        report(folio_state_failure_line(settings, folio_state_language(state)));
    }
    struct folio_window *_Nullable window = nullptr;
    if (folio_window_create(state, &window) != FOLIO_WINDOW_CREATED)
    {
        report(ui_text_line(UI_TEXT_APP_NO_WINDOW, folio_state_language(state)));
        folio_state_destroy(state);
        return 1;
    }
    run_message_loop(window);
    folio_window_destroy(window);
    folio_state_destroy(state);
    return 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR arguments, int show);

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR arguments, int show)
{
    (void)instance;
    (void)previous;
    (void)arguments;
    (void)show;
    struct persistence_adapter *_Nullable adapter = nullptr;
    enum persistence_adapter_outcome created = persistence_adapter_create(&adapter);
    if (created != PERSISTENCE_ADAPTER_CREATED)
    {
        report(ui_text_line(adapter_lines[created], FOLIO_LANGUAGE_JA));
        return 1;
    }
    struct appearance_adapter *_Nullable appearance = nullptr;
    if (appearance_adapter_create(&appearance) != APPEARANCE_ADAPTER_CREATED)
    {
        report(ui_text_line(UI_TEXT_APP_OUT_OF_MEMORY, FOLIO_LANGUAGE_JA));
        persistence_adapter_destroy(adapter);
        return 1;
    }
    struct regex_adapter *_Nullable regex = nullptr;
    if (regex_adapter_create(&regex) != REGEX_ADAPTER_CREATED)
    {
        report(ui_text_line(UI_TEXT_APP_OUT_OF_MEMORY, FOLIO_LANGUAGE_JA));
        appearance_adapter_destroy(appearance);
        persistence_adapter_destroy(adapter);
        return 1;
    }
    int code = run(adapter, appearance, regex);
    regex_adapter_destroy(regex);
    appearance_adapter_destroy(appearance);
    persistence_adapter_destroy(adapter);
    return code;
}
