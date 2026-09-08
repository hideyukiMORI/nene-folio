/* 合成ルート（ARC-006）。ポートに実装を結び、窓を起動し、メッセージループと終了コードを所有する。
 * 出してよいのは起動できなかった理由 1 行だけ（FR-015）。文言は application が作る。 */
#include "folio_state.h"
#include "folio_window.h"
#include "persistence_adapter.h"
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
    case PERSISTENCE_ADAPTER_OUT_OF_MEMORY:
        return L"記憶域が足りません。";
    }
    return L"実行ファイルの場所が取得できません。";
}

static void run_message_loop(void)
{
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

/* 状態と窓を作って走らせる。アダプタは状態より長く生きる。 */
static int run(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = persistence_adapter_port(adapter);
    struct folio_state *_Nullable state = nullptr;
    enum folio_state_outcome loaded = folio_state_create(&port, &state);
    if (loaded != FOLIO_STATE_READY)
    {
        report_utf8(folio_state_failure_line(loaded));
        return 1;
    }
    struct folio_window *_Nullable window = nullptr;
    if (folio_window_create(state, &window) != FOLIO_WINDOW_CREATED)
    {
        report(L"窓を作れませんでした。");
        folio_state_destroy(state);
        return 1;
    }
    run_message_loop();
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
    int code = run(adapter);
    persistence_adapter_destroy(adapter);
    return code;
}
