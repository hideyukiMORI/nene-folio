/* 合成ルート（ARC-006）。ポートに実装を結び、窓を起動し、メッセージループと終了コードを所有する。
 * 出してよいのは起動できなかった理由 1 行だけ（FR-015）。 */
#include "folio_state.h"
#include "folio_window.h"
#include "persistence_adapter.h"

#include <windows.h>

static const wchar_t title[] = L"NeNe Folio";

static void report(const wchar_t *_Nonnull line)
{
    MessageBoxW(nullptr, line, title, MB_OK | MB_ICONERROR);
}

static const wchar_t *_Nonnull state_failure(enum folio_state_outcome outcome)
{
    switch (outcome)
    {
    case FOLIO_STATE_READY:
        return L"";
    case FOLIO_STATE_DATA_UNREADABLE:
        return L"data/ を読めませんでした。";
    case FOLIO_STATE_LEDGER_MALFORMED:
        return L"data/ の台帳（categories.json / index.json）が版 1 の形ではありません。";
    case FOLIO_STATE_OUT_OF_MEMORY:
        return L"記憶域が足りません。";
    }
    return L"data/ を読めませんでした。";
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

/* 起動時の読み込みだけにアダプタを使う。読み込みが終わればポートは要らない。 */
static enum folio_state_outcome load_state(struct folio_state *_Nullable *_Nonnull out)
{
    struct persistence_adapter *_Nullable adapter = nullptr;
    enum persistence_adapter_outcome created = persistence_adapter_create(&adapter);
    if (created != PERSISTENCE_ADAPTER_CREATED)
    {
        report(adapter_failure(created));
        return FOLIO_STATE_DATA_UNREADABLE;
    }
    struct persistence_port port = persistence_adapter_port(adapter);
    enum folio_state_outcome outcome = folio_state_create(&port, out);
    persistence_adapter_destroy(adapter);
    if (outcome != FOLIO_STATE_READY)
    {
        report(state_failure(outcome));
    }
    return outcome;
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

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR arguments, int show);

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE previous, PWSTR arguments, int show)
{
    (void)instance;
    (void)previous;
    (void)arguments;
    (void)show;
    struct folio_state *_Nullable state = nullptr;
    if (load_state(&state) != FOLIO_STATE_READY)
    {
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
