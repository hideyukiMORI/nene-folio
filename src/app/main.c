/* 合成ルート（ARC-006）。ポートに実装を結び、窓を起動し、メッセージループと終了コードを所有する。
 * 出してよいのは「起動できなかった理由」と「起動は続けるが設定を読めなかった知らせ」の 1 行だけ
 * （FR-015 / ADR 0025 の決定 6）。どちらも文言は application が作る。 */
#include "appearance_adapter.h"
#include "folio_palette.h"
#include "folio_state.h"
#include "folio_window.h"
#include "font_bundle.h"
#include "persistence_adapter.h"
#include "regex_adapter.h"
#include "storage_adapter.h"
#include "storage_prompt.h"
#include "storage_state.h"
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
    [PERSISTENCE_ADAPTER_INVALID_PATH] = UI_TEXT_STORAGE_INVALID_PATH,
    [PERSISTENCE_ADAPTER_DATA_IN_USE] = UI_TEXT_APP_DATA_IN_USE,
    [PERSISTENCE_ADAPTER_RECOVERY_LOCKED] = UI_TEXT_APP_RECOVERY_LOCKED,
    [PERSISTENCE_ADAPTER_OUT_OF_MEMORY] = UI_TEXT_APP_OUT_OF_MEMORY,
};

static void report_initial_notices(const struct folio_state *_Nonnull state)
{
    const enum folio_state_outcome notices[] = {folio_state_settings_notice(state),
                                                folio_state_appearance_notice(state)};
    for (size_t index = 0; index < sizeof notices / sizeof notices[0]; ++index)
    {
        if (notices[index] != FOLIO_STATE_READY)
        {
            report(folio_state_failure_line(notices[index], folio_state_language(state)));
        }
    }
}

static void dispatch_message(const struct folio_window *_Nonnull window, MSG *_Nonnull message)
{
    if (!folio_window_translate(window, message))
    {
        TranslateMessage(message);
        DispatchMessageW(message);
    }
}

/* 全範囲queueを空にした直後だけ一件準備し、抑止中は新規messageを待つ（ADR0068）。 */
static void run_message_loop(struct folio_window *_Nonnull window)
{
    MSG message;
    for (;;)
    {
        if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
                break;
            dispatch_message(window, &message);
            continue;
        }
        if (folio_window_idle_work_ready(window))
            folio_window_continue_index_filter(window);
        else if (!WaitMessage())
            break;
    }
}

/* 状態と窓を作って走らせる。アダプタは状態より長く生きる。 */
static int run(const struct folio_ports *_Nonnull ports, struct storage_state *_Nonnull storage,
               enum font_bundle_outcome fonts)
{
    struct folio_state *_Nullable state = nullptr;
    enum folio_state_outcome loaded = folio_state_create(ports, &state);
    if (loaded != FOLIO_STATE_READY)
    {
        /* 状態を作れていないので、どの言語で見せるかを尋ねる相手がまだいない（ADR 0030 の補正 7）。
         */
        report(folio_state_failure_line(loaded, FOLIO_LANGUAGE_JA));
        return 1;
    }
    /* 起動は続けるが、設定を読めなかった理由は窓を作る前に 1 回だけ見せる（ADR 0025 の決定 6）。
     * main は 1 回しか走らないので「見せたか」の真偽はどこにも持たない。 */
    report_initial_notices(state);
    struct folio_window *_Nullable window = nullptr;
    if (folio_window_create(state, storage, fonts, &window) != FOLIO_WINDOW_CREATED)
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

/* 初回/復旧だけが確認を要求する。色は既存appearance portとcoreの同じ解決経路。 */
static enum storage_prompt_outcome confirm_storage(struct storage_state *_Nonnull storage,
                                                   struct appearance_adapter *_Nonnull appearance)
{
    if (storage_state_phase(storage) == STORAGE_ACTIVE)
        return STORAGE_PROMPT_ACCEPTED;
    struct theme_palette colors = *folio_theme_palette(FOLIO_THEME_LIGHT);
    struct appearance_port looks = appearance_adapter_port(appearance);
    struct appearance_snapshot snapshot;
    if (looks.read(looks.adapter, &snapshot) == APPEARANCE_READY)
        colors = appearance_snapshot_palette(&snapshot, FOLIO_THEME_CHOICE_SYSTEM);
    struct folio_palette palette = folio_palette_from(&colors);
    struct storage_prompt_request request = {.storage = storage, .language = FOLIO_LANGUAGE_JA};
    return storage_prompt_show(nullptr, &request, &palette);
}

static int run_from_storage(struct storage_state *_Nonnull storage,
                            struct appearance_adapter *_Nonnull appearance,
                            struct regex_adapter *_Nonnull regex, enum font_bundle_outcome fonts)
{
    switch (confirm_storage(storage, appearance))
    {
    case STORAGE_PROMPT_ACCEPTED:
        break;
    case STORAGE_PROMPT_CANCELLED:
        return 0;
    case STORAGE_PROMPT_UNAVAILABLE:
        report(ui_text_line(UI_TEXT_APP_NO_WINDOW, FOLIO_LANGUAGE_JA));
        return 1;
    }
    struct persistence_adapter *_Nullable persistence = nullptr;
    enum persistence_adapter_outcome created =
        persistence_adapter_create(storage_state_directory(storage), &persistence);
    if (created != PERSISTENCE_ADAPTER_CREATED)
    {
        report(ui_text_line(adapter_lines[created], FOLIO_LANGUAGE_JA));
        return 1;
    }
    struct persistence_port files = persistence_adapter_port(persistence);
    struct appearance_port looks = appearance_adapter_port(appearance);
    struct regex_port finder = regex_adapter_port(regex);
    struct folio_ports ports = {.persistence = &files, .appearance = &looks, .regex = &finder};
    int code = run(&ports, storage, fonts);
    persistence_adapter_destroy(persistence);
    return code;
}

static int run_with_storage(struct storage_adapter *_Nonnull adapter,
                            struct appearance_adapter *_Nonnull appearance,
                            struct regex_adapter *_Nonnull regex, enum font_bundle_outcome fonts)
{
    struct storage_port port = storage_adapter_port(adapter);
    struct storage_state *_Nullable storage = nullptr;
    enum storage_outcome loaded = storage_state_create(&port, &storage);
    if (loaded != STORAGE_READY)
    {
        report(storage_state_failure_line(loaded, FOLIO_LANGUAGE_JA));
        return 1;
    }
    int code = run_from_storage(storage, appearance, regex, fonts);
    storage_state_destroy(storage);
    return code;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR arguments, int show);

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR arguments, int show)
{
    (void)instance;
    (void)previous;
    (void)arguments;
    (void)show;
    struct storage_adapter *_Nullable adapter = nullptr;
    enum storage_outcome created = storage_adapter_create(&adapter);
    if (created != STORAGE_READY)
    {
        report(storage_state_failure_line(created, FOLIO_LANGUAGE_JA));
        return 1;
    }
    struct appearance_adapter *_Nullable appearance = nullptr;
    if (appearance_adapter_create(&appearance) != APPEARANCE_ADAPTER_CREATED)
    {
        report(ui_text_line(UI_TEXT_APP_OUT_OF_MEMORY, FOLIO_LANGUAGE_JA));
        storage_adapter_destroy(adapter);
        return 1;
    }
    struct regex_adapter *_Nullable regex = nullptr;
    if (regex_adapter_create(&regex) != REGEX_ADAPTER_CREATED)
    {
        report(ui_text_line(UI_TEXT_APP_OUT_OF_MEMORY, FOLIO_LANGUAGE_JA));
        appearance_adapter_destroy(appearance);
        storage_adapter_destroy(adapter);
        return 1;
    }
    /* 同梱の書体は窓を作る前に登録し、メッセージループのあとに解除する（ADR 0036 の決定 3・
     * ARC-004 の所有者はこの入口）。読めなくても起動は止めず、窓が最初の描画のあとに 1 回知らせる
     * （決定 5）。 */
    struct font_bundle *_Nullable fonts = nullptr;
    enum font_bundle_outcome bundled = font_bundle_register(&fonts);
    int code = run_with_storage(adapter, appearance, regex, bundled);
    font_bundle_destroy(fonts);
    regex_adapter_destroy(regex);
    appearance_adapter_destroy(appearance);
    storage_adapter_destroy(adapter);
    return code;
}
