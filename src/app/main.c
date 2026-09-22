/* 合成ルート（ARC-006）。ポートに実装を結び、窓を起動し、メッセージループと終了コードを所有する。
 * 出してよいのは「起動できなかった理由」と「起動は続けるが設定を読めなかった知らせ」の 1 行だけ
 * （FR-015 / ADR 0025 の決定 6）。どちらも文言は application が作る。 */
#include "appearance_adapter.h"
#include "folio_state.h"
#include "folio_window.h"
#include "persistence_adapter.h"
#include "regex_adapter.h"
#include "utf16_text.h"

#include <string.h>
#include <windows.h>

static const wchar_t title[] = L"NeNe Folio";

static void report(const wchar_t *_Nonnull line)
{
    MessageBoxW(nullptr, line, title, MB_OK | MB_ICONERROR);
}

static void report_utf8(const char *_Nonnull line)
{
    struct utf16_text *_Nullable text = nullptr;
    if (utf16_text_create(line, strlen(line), &text) != UTF16_TEXT_CONVERTED)
    {
        report(L"記憶域が足りません。");
        return;
    }
    report(utf16_text_units(text));
    utf16_text_destroy(text);
}

static const wchar_t *_Nonnull adapter_failure(enum persistence_adapter_outcome outcome)
{
    switch (outcome)
    {
    case PERSISTENCE_ADAPTER_CREATED:
        return L"";
    case PERSISTENCE_ADAPTER_NO_MODULE_PATH:
        return L"実行ファイルの場所が長すぎるか、取得できません。";
    case PERSISTENCE_ADAPTER_DATA_IN_USE:
        return L"同じ data/ を別の NeNe Folio が使っています。先に閉じてください。";
    case PERSISTENCE_ADAPTER_RECOVERY_LOCKED:
        return L"名前変更の復旧記録（data/.rename.json）がありますが、data/ "
               L"を占有できないので復旧できません。";
    case PERSISTENCE_ADAPTER_OUT_OF_MEMORY:
        return L"記憶域が足りません。";
    }
    return L"実行ファイルの場所が取得できません。";
}

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
        report_utf8(folio_state_failure_line(loaded));
        return 1;
    }
    /* 起動は続けるが、設定を読めなかった理由は窓を作る前に 1 回だけ見せる（ADR 0025 の決定 6）。
     * main は 1 回しか走らないので「見せたか」の真偽はどこにも持たない。 */
    enum folio_state_outcome settings = folio_state_settings_notice(state);
    if (settings != FOLIO_STATE_READY)
    {
        report_utf8(folio_state_failure_line(settings));
    }
    struct folio_window *_Nullable window = nullptr;
    if (folio_window_create(state, &window) != FOLIO_WINDOW_CREATED)
    {
        report(L"窓を作れませんでした。");
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
        report(adapter_failure(created));
        return 1;
    }
    struct appearance_adapter *_Nullable appearance = nullptr;
    if (appearance_adapter_create(&appearance) != APPEARANCE_ADAPTER_CREATED)
    {
        report(L"記憶域が足りません。");
        persistence_adapter_destroy(adapter);
        return 1;
    }
    struct regex_adapter *_Nullable regex = nullptr;
    if (regex_adapter_create(&regex) != REGEX_ADAPTER_CREATED)
    {
        report(L"記憶域が足りません。");
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
