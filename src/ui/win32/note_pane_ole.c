#include "note_pane_ole.h"

#include "folio_message.h"
#include "note_pane.h"
#include <shellapi.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>

struct note_pane_ole
{
    IRichEditOleCallback callback; /* 先頭に置く: インターフェースのポインタがそのまま本体を指す */
    LONG references;
    HWND _Nonnull pane;
    /* 直前の QueryAcceptData(RECO_DROP) が見たデータがファイルの並びだったか。GetDragDropEffect は
     * データを受け取らないので、ここで覚える（RichEdit は欄へ入るたびに QueryAcceptData を呼ぶ）。
     * 欄から出たことは知らされないので、真のまま残ることがある。次の投下は欄へ入るときに尋ね
     * 直されるが、尋ねられない道に古い値を読ませないよう、ほかの問い合わせ（投下でない
     * QueryAcceptData・欄からのドラッグの GetDragDropEffect）でも偽へ戻す。 */
    bool files_over;
};

/* richole.h の IID_IRichEditOleCallback と同じ値。uuid.lib に無いので、値をここに持つ。 */
static const IID callback_id = {
    0x00020D03, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

static struct note_pane_ole *_Nonnull owner(IRichEditOleCallback *_Nonnull self)
{
    return (struct note_pane_ole *)self;
}

/* QueryInterfaceのvoid**はCOM ABIだけの境界（C-006）。 */
static HRESULT STDMETHODCALLTYPE ole_query(IRichEditOleCallback *_Nonnull self, REFIID id,
                                           void *_Nullable *_Nullable out)
{
    if (out == nullptr)
        return E_POINTER;
    *out = nullptr;
    if (!IsEqualIID(id, &IID_IUnknown) && !IsEqualIID(id, &callback_id))
        return E_NOINTERFACE;
    *out = self;
    (void)InterlockedIncrement(&owner(self)->references);
    return S_OK;
}

static ULONG STDMETHODCALLTYPE ole_add(IRichEditOleCallback *_Nonnull self)
{
    return (ULONG)InterlockedIncrement(&owner(self)->references);
}

static ULONG STDMETHODCALLTYPE ole_release(IRichEditOleCallback *_Nonnull self)
{
    LONG count = InterlockedDecrement(&owner(self)->references);
    if (count == 0)
        free(owner(self));
    return (ULONG)count;
}

/* 埋め込みの物（OLE の物）の置き場は渡さない。外から来るデータは ole_accept が先に振り分ける
 * （投下はファイルか文字だけ、貼り付けは文字だけ）ので、ここは二重の守り。 */
static HRESULT STDMETHODCALLTYPE ole_storage(IRichEditOleCallback *_Nonnull self,
                                             LPSTORAGE *_Nullable storage)
{
    (void)self;
    if (storage != nullptr)
        *storage = nullptr;
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE ole_in_place(IRichEditOleCallback *_Nonnull self,
                                              LPOLEINPLACEFRAME *_Nullable frame,
                                              LPOLEINPLACEUIWINDOW *_Nullable document,
                                              LPOLEINPLACEFRAMEINFO _Nullable information)
{
    (void)self;
    if (frame != nullptr)
        *frame = nullptr;
    if (document != nullptr)
        *document = nullptr;
    if (information != nullptr)
    {
        information->fMDIApp = FALSE;
        information->hwndFrame = nullptr;
        information->haccel = nullptr;
        information->cAccelEntries = 0;
    }
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE ole_container(IRichEditOleCallback *_Nonnull self, BOOL show)
{
    (void)self;
    (void)show;
    return S_OK;
}

/* 挿入は断れない: 閲覧表示の画像（note_pane_images の InsertImage＝Folio 自身が入れる画像）も、
 * RichEdit はここへ尋ねる。断ると画像の表示が全部消える（2026-11-23 に試験で確かめた）。 */
static HRESULT STDMETHODCALLTYPE ole_insert(IRichEditOleCallback *_Nonnull self,
                                            LPCLSID _Nullable class, LPSTORAGE _Nullable storage,
                                            LONG position)
{
    (void)self;
    (void)class;
    (void)storage;
    (void)position;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ole_delete(IRichEditOleCallback *_Nonnull self,
                                            LPOLEOBJECT _Nullable object)
{
    (void)self;
    (void)object;
    return S_OK;
}

static FORMATETC files_shape(void)
{
    return (FORMATETC){
        .cfFormat = CF_HDROP, .dwAspect = DVASPECT_CONTENT, .lindex = -1, .tymed = TYMED_HGLOBAL};
}

static bool holds_files(LPDATAOBJECT _Nullable data)
{
    FORMATETC wanted = files_shape();
    return data != nullptr && data->lpVtbl->QueryGetData(data, &wanted) == S_OK;
}

/* 渡されたファイルの並び（DROPFILES）の写し。よそのプログラムが作ったものなので、頭の大きさと
 * 名前の始まりを確かめ、末尾に 0 を足して、終わりの無い並びを読ませない。呼んだ側が GlobalFree
 * する。足すのは、名前の部分（pFiles から後ろ）を偶数バイトへ切り上げる分と、0 の文字 2 つ分:
 * 名前の部分が奇数バイトで 0 で終わっていないと、最後の半端な 1 バイトが 1 文字の下位になるので、
 * その上位と、並びの終わりの 0 が 2 つ（UTF-16）要る。fWide が 0 の並び（1 バイトの 0 が 2 つ）は
 * これより少なくて足りる。 */
static HDROP _Nullable closed_list(HGLOBAL _Nonnull source)
{
    constexpr SIZE_T closing = 2 * sizeof(wchar_t);
    SIZE_T size = GlobalSize(source);
    const DROPFILES *_Nullable from = size < sizeof *from ? nullptr : GlobalLock(source);
    if (from == nullptr)
        return nullptr;
    bool sound = from->pFiles >= sizeof *from && from->pFiles <= size;
    SIZE_T odd = sound ? (size - from->pFiles) % sizeof(wchar_t) : 0;
    HGLOBAL copy =
        sound ? GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, size + odd + closing) : nullptr;
    void *_Nullable to = copy == nullptr ? nullptr : GlobalLock(copy);
    if (to != nullptr)
    {
        memcpy(to, from, size);
        GlobalUnlock(copy);
    }
    else if (copy != nullptr)
    {
        GlobalFree(copy);
        copy = nullptr;
    }
    GlobalUnlock(source);
    return (HDROP)copy;
}

/* 送り手のデータからファイルの並びの写しを取る。取れなければ（GetData の失敗・HGLOBAL でない・
 * 並びが壊れている・記憶が足りない）nullptr。データは OS の ReleaseStgMedium で放す。 */
static HDROP _Nullable taken_list(LPDATAOBJECT _Nonnull data)
{
    FORMATETC wanted = files_shape();
    STGMEDIUM medium = {0};
    if (FAILED(data->lpVtbl->GetData(data, &wanted, &medium)))
        return nullptr;
    HDROP list = medium.tymed == TYMED_HGLOBAL && medium.hGlobal != nullptr
                     ? closed_list(medium.hGlobal)
                     : nullptr;
    ReleaseStgMedium(&medium);
    return list;
}

/* ファイルの並びを、主窓の 1 つの取り込みの道（folio_message_files_dropped。欄の WM_DROPFILES と
 * 同じ行き先）へ渡す。ここではパスを読まず、取り込むかどうかも決めない（ARC-001）。
 * 並びを取れなかったときは、並び無し（lParam が 0）で同じ道へ知らせる: 主窓が「読めません」の
 * 1 行を出す（黙って「扱った」と答えない）。
 * 位置は選択の始まり: RichEdit は、投下を尋ねる前に、落とした点へ選択を畳んでいる（2026-11-23 の
 * 実験。落とした点の EM_CHARFROMPOS と同じ位置だった）。 */
static void hand_files(HWND _Nonnull pane, LPDATAOBJECT _Nonnull data)
{
    HDROP list = taken_list(data);
    CHARRANGE selected = {0};
    SendMessageW(pane, EM_EXGETSEL, 0, (LPARAM)&selected);
    SendMessageW(GetParent(pane), folio_message_files_dropped,
                 (WPARAM)(selected.cpMin < 0 ? 0 : selected.cpMin), (LPARAM)list);
    if (list != nullptr)
        GlobalFree(list);
}

/* ファイルの投下: 欄か主窓が入力を止めているあいだ（取り込みの途中など）は断る。それ以外の
 * 断り（面・変換の途中・閲覧表示・無題）は、渡した先の主窓と application が決めて 1 行で言う。
 * RichEdit には取り込ませない（S_FALSE＝こちらで調べた／扱った）。 */
static HRESULT accept_files(struct note_pane_ole *_Nonnull ole, LPDATAOBJECT _Nonnull data,
                            bool really)
{
    if (!IsWindowEnabled(ole->pane) || !IsWindowEnabled(GetAncestor(ole->pane, GA_ROOT)))
        return E_FAIL;
    ole->files_over = !really;
    if (really)
        hand_files(ole->pane, data);
    return S_FALSE;
}

/* 外から来るデータの振り分け。投下（RECO_DROP）のファイルの並びは accept_files。それ以外の投下と
 * 貼り付け（RECO_PASTE）は、文字としてだけ受けさせる（書式つきの文字も、物も入れさせない）。
 * 文字の投下は、欄の貼り付けと同じ 1 つの判定（note_pane_text_open: 準備中・変換の途中・着色の錠・
 * 読み取り専用・入力を止めているあいだ）で断る。貼り付けは、欄がその判定を済ませてから
 * EM_PASTESPECIAL で頼むので、ここでは形式だけを文字に固定する。欄からの写し・切り取り・ドラッグ
 * （RECO_COPY・RECO_CUT・RECO_DRAG）は触らない。 */
// Waiver: WVR-0002
// NOLINTNEXTLINE(readability-function-size)
static HRESULT STDMETHODCALLTYPE ole_accept(IRichEditOleCallback *_Nonnull self,
                                            LPDATAOBJECT _Nullable data,
                                            CLIPFORMAT *_Nullable format, DWORD operation,
                                            BOOL really, HGLOBAL _Nullable picture)
{
    (void)picture;
    struct note_pane_ole *_Nonnull ole = owner(self);
    ole->files_over = false;
    if (operation != RECO_DROP && operation != RECO_PASTE)
        return S_OK;
    if (operation == RECO_DROP && data != nullptr && holds_files(data))
        return accept_files(ole, data, really != FALSE);
    if (operation == RECO_DROP && !note_pane_text_open(ole->pane))
        return E_FAIL;
    if (format != nullptr)
        *format = CF_UNICODETEXT;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE ole_help(IRichEditOleCallback *_Nonnull self, BOOL enter)
{
    (void)self;
    (void)enter;
    return E_NOTIMPL;
}

/* 欄からの写し・切り取り・ドラッグは、RichEdit の既定のデータのまま（E_NOTIMPL）。 */
static HRESULT STDMETHODCALLTYPE ole_clipboard(IRichEditOleCallback *_Nonnull self,
                                               CHARRANGE *_Nullable range, DWORD operation,
                                               LPDATAOBJECT *_Nullable data)
{
    (void)self;
    (void)range;
    (void)operation;
    if (data != nullptr)
        *data = nullptr;
    return E_NOTIMPL;
}

/* ファイルが上にあるあいだは「コピー」の印（送り手がコピーを許していなければ、落とせない印）。
 * 文字の投下は RichEdit の既定に任せる（E_NOTIMPL で、コールバックが無いときと同じ印になる）。
 * 欄からのドラッグ（drag）は、渡された既定の効果を変えずに通す。 */
static HRESULT STDMETHODCALLTYPE ole_effect(IRichEditOleCallback *_Nonnull self, BOOL drag,
                                            DWORD keys, DWORD *_Nullable effect)
{
    (void)keys;
    if (drag)
    {
        owner(self)->files_over = false;
        return S_OK;
    }
    if (effect == nullptr || !owner(self)->files_over)
        return E_NOTIMPL;
    *effect &= DROPEFFECT_COPY;
    return S_OK;
}

/* メニューは出さない。右クリックのメニューは、欄のサブクラスが WM_CONTEXTMENU で出す（右ボタンを
 * 離した知らせは、欄が RichEdit へ渡さないので、ふつうはここへ来ない＝note_pane の pane_forward）。
 */
// Waiver: WVR-0002
// NOLINTNEXTLINE(readability-function-size)
static HRESULT STDMETHODCALLTYPE ole_menu(IRichEditOleCallback *_Nonnull self, WORD selection,
                                          LPOLEOBJECT _Nullable object, CHARRANGE *_Nullable range,
                                          HMENU *_Nullable menu)
{
    (void)self;
    (void)selection;
    (void)object;
    (void)range;
    if (menu != nullptr)
        *menu = nullptr;
    return E_NOTIMPL;
}

static IRichEditOleCallbackVtbl callback_table = {
    ole_query,  ole_add,    ole_release, ole_storage,   ole_in_place, ole_container, ole_insert,
    ole_delete, ole_accept, ole_help,    ole_clipboard, ole_effect,   ole_menu};

bool note_pane_ole_create(HWND _Nonnull pane, IRichEditOleCallback *_Nullable *_Nonnull out)
{
    struct note_pane_ole *_Nullable ole = calloc(1, sizeof *ole);
    if (ole == nullptr)
        return false;
    ole->callback.lpVtbl = &callback_table;
    ole->references = 1;
    ole->pane = pane;
    *out = &ole->callback;
    return true;
}
