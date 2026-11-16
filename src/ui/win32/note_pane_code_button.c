#include "note_pane_code_button.h"
#include "icon_paint.h"
#include "note_pane_links.h"
#include "theme_palette.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windowsx.h>

constexpr size_t no_action = SIZE_MAX;
constexpr UINT done_milliseconds = 1500;
constexpr size_t label_capacity = 24;
/* 設計の寸法（96 DPI・100%での px）。倍率と DPI を掛けて使う。 */
constexpr int icon_size = 14;
constexpr int rest_inset = 6;
constexpr int label_inset = 9;
constexpr int icon_gap = 6;

struct note_pane_code_button
{
    HWND _Nonnull window;
    const struct note_pane_links *_Nullable links; /* 借用。次の attach まで有効。 */
    COLORREF surface;                              /* rtf.code_background */
    COLORREF muted;                                /* rtf.code_comment: 静止の絵と枠 */
    COLORREF strong;                               /* rtf.code_text: 乗ったときの絵と字 */
    COLORREF success;                              /* rtf.code_string: 済みの絵と字と枠 */
    bool enabled;                                  /* 高コントラストでは飾らない */
    bool tracking;
    bool held;                /* 札の上で押した。離すか取り消すまでの間だけ。 */
    size_t held_action;       /* 押した札 */
    CHARRANGE held_selection; /* 押す前の選択 */
    POINT held_scroll;        /* 押す前のスクロール位置 */
    int zoom_percent;
    wchar_t face[LF_FACESIZE];
    int twips; /* 札の字の大きさ */
    wchar_t label[label_capacity];
    wchar_t done_text[label_capacity];
    size_t hover; /* 以下は札の文書順の番号。無ければ no_action。 */
    size_t pressed;
    size_t done;
    size_t caret;
    HFONT _Nullable font;
    int font_height;
};

static COLORREF to_color(struct rgb_color color)
{
    return RGB(color.red, color.green, color.blue);
}

enum note_pane_outcome
note_pane_code_button_create(HWND _Nonnull window,
                             struct note_pane_code_button *_Nullable *_Nonnull out)
{
    struct note_pane_code_button *_Nullable button = calloc(1, sizeof *button);
    if (button == nullptr)
        return NOTE_PANE_OUT_OF_MEMORY;
    button->window = window;
    button->zoom_percent = 100;
    button->hover = button->pressed = button->done = button->caret = no_action;
    *out = button;
    return NOTE_PANE_CREATED;
}

void note_pane_code_button_destroy(struct note_pane_code_button *_Nullable button)
{
    if (button == nullptr)
        return;
    KillTimer(button->window, note_pane_code_button_timer_id);
    if (button->font != nullptr)
        DeleteObject(button->font);
    free(button);
}

void note_pane_code_button_palette(struct note_pane_code_button *_Nonnull button,
                                   const struct theme_palette *_Nonnull theme)
{
    button->surface = to_color(theme->rtf.code_background);
    button->muted = to_color(theme->rtf.code_comment);
    button->strong = to_color(theme->rtf.code_text);
    button->success = to_color(theme->rtf.code_string);
    switch (theme->contrast)
    {
    case FOLIO_CONTRAST_STANDARD:
        button->enabled = true;
        return;
    case FOLIO_CONTRAST_HIGH:
        button->enabled = false;
        return;
    }
}

void note_pane_code_button_zoom(struct note_pane_code_button *_Nonnull button, int zoom_percent)
{
    button->zoom_percent = zoom_percent;
}

static size_t action_count(const struct note_pane_code_button *_Nonnull button)
{
    return button->enabled ? note_pane_links_action_count(button->links) : 0;
}

static int pixels(const struct note_pane_code_button *_Nonnull button, int units)
{
    return MulDiv(MulDiv(units, (int)GetDpiForWindow(button->window), 96), button->zoom_percent,
                  100);
}

/* 札の区間が今の画面で占める矩形。行の高さは次の行の上端までで、段落の前後の間隔を含む。 */
static bool action_bounds(const struct note_pane_code_button *_Nonnull button, size_t index,
                          RECT *_Nonnull out)
{
    CHARRANGE run = note_pane_links_action(button->links, index);
    POINTL start = {0};
    POINTL end = {0};
    POINTL below = {0};
    SendMessageW(button->window, EM_POSFROMCHAR, (WPARAM)&start, run.cpMin);
    SendMessageW(button->window, EM_POSFROMCHAR, (WPARAM)&end, run.cpMax);
    SendMessageW(button->window, EM_POSFROMCHAR, (WPARAM)&below, run.cpMax + 1);
    *out = (RECT){start.x, start.y, end.x, below.y};
    RECT client;
    RECT shared;
    GetClientRect(button->window, &client);
    return end.y == start.y && out->right > out->left && out->bottom > out->top &&
           IntersectRect(&shared, out, &client);
}

/* from 以降で画面に掛かる最初の札。無ければ札の数を返す。画面外の札には位置を尋ねない。 */
static size_t next_visible(const struct note_pane_code_button *_Nonnull button, size_t from,
                           RECT *_Nonnull bounds)
{
    RECT client;
    GetClientRect(button->window, &client);
    POINTL top = {0, 0};
    POINTL bottom = {client.right, client.bottom};
    LONG first = (LONG)SendMessageW(button->window, EM_CHARFROMPOS, 0, (LPARAM)&top);
    LONG last = (LONG)SendMessageW(button->window, EM_CHARFROMPOS, 0, (LPARAM)&bottom);
    size_t count = action_count(button);
    for (size_t index = from; index < count; ++index)
    {
        CHARRANGE run = note_pane_links_action(button->links, index);
        if (run.cpMin > last)
            return count;
        if (run.cpMax >= first && action_bounds(button, index, bounds))
            return index;
    }
    return count;
}

static size_t caret_action(const struct note_pane_code_button *_Nonnull button)
{
    CHARRANGE selected = {0};
    SendMessageW(button->window, EM_EXGETSEL, 0, (LPARAM)&selected);
    if (GetFocus() != button->window || selected.cpMin != selected.cpMax)
        return no_action;
    size_t count = action_count(button);
    for (size_t index = 0; index < count; ++index)
    {
        CHARRANGE run = note_pane_links_action(button->links, index);
        if (selected.cpMin >= run.cpMin && selected.cpMin < run.cpMax)
            return index;
    }
    return no_action;
}

static HFONT _Nullable label_font(struct note_pane_code_button *_Nonnull button)
{
    int height = -MulDiv(MulDiv(button->twips, (int)GetDpiForWindow(button->window), 1440),
                         button->zoom_percent, 100);
    if (button->font != nullptr && button->font_height == height)
        return button->font;
    if (button->font != nullptr)
        DeleteObject(button->font);
    button->font = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH, button->face);
    button->font_height = height;
    return button->font;
}

static void fill(HDC _Nonnull device, RECT bounds, COLORREF color)
{
    SetDCBrushColor(device, color);
    FillRect(device, &bounds, (HBRUSH)GetStockObject(DC_BRUSH));
}

/* 文言を右の内側の余白へ寄せて描き、その左端を返す。 */
static LONG draw_label(struct note_pane_code_button *_Nonnull button, HDC _Nonnull device,
                       RECT bounds, const wchar_t *_Nonnull text)
{
    HFONT _Nullable font = label_font(button);
    int length = (int)wcslen(text);
    LONG right = bounds.right - pixels(button, label_inset);
    if (font == nullptr || length == 0)
        return right;
    SelectObject(device, font);
    SIZE size = {0};
    GetTextExtentPoint32W(device, text, length, &size);
    LONG left = right - size.cx;
    SetBkMode(device, TRANSPARENT);
    TextOutW(device, left, (bounds.top + bounds.bottom - size.cy) / 2, text, length);
    return left - pixels(button, icon_gap);
}

static bool action_active(const struct note_pane_code_button *_Nonnull button, size_t index)
{
    return index == button->done || index == button->pressed || index == button->hover ||
           index == button->caret;
}

/* 絵と字の色。済み・押している・乗っている（キャレットを含む）・静止の順に決める。 */
static COLORREF action_ink(const struct note_pane_code_button *_Nonnull button, size_t index)
{
    if (index == button->done)
        return button->success;
    if (index == button->pressed)
        return button->surface;
    return action_active(button, index) ? button->strong : button->muted;
}

/* 枠と文言を描き、絵の右端になる位置を返す。 */
static LONG paint_active(struct note_pane_code_button *_Nonnull button, HDC _Nonnull device,
                         RECT bounds, size_t index)
{
    bool done = index == button->done;
    SetDCBrushColor(device, done ? button->success : button->muted);
    FrameRect(device, &bounds, (HBRUSH)GetStockObject(DC_BRUSH));
    SetTextColor(device, action_ink(button, index));
    return draw_label(button, device, bounds, done ? button->done_text : button->label);
}

/* 静止は絵だけ。乗る・キャレット・押す・済みは枠と文言を足す（設計の案C）。 */
static void paint_action(struct note_pane_code_button *_Nonnull button, HDC _Nonnull device,
                         size_t index, RECT bounds)
{
    bool done = index == button->done;
    bool pressed = !done && index == button->pressed;
    int saved = SaveDC(device);
    IntersectClipRect(device, bounds.left, bounds.top, bounds.right, bounds.bottom);
    fill(device, bounds, pressed ? button->muted : button->surface);
    LONG edge = bounds.right - pixels(button, rest_inset);
    if (action_active(button, index))
        edge = paint_active(button, device, bounds, index);
    int size = pixels(button, icon_size);
    RECT icon = {edge - size, (bounds.top + bounds.bottom - size) / 2, edge, 0};
    icon.bottom = icon.top + size;
    if (icon.left > bounds.left)
        icon_paint_fill(device, icon, done ? ICON_PAINT_CHECK : ICON_PAINT_COPY,
                        action_ink(button, index));
    RestoreDC(device, saved);
}

void note_pane_code_button_paint(struct note_pane_code_button *_Nonnull button,
                                 HDC _Nullable device)
{
    size_t count = action_count(button);
    if (count == 0)
        return;
    HDC _Nullable target = device != nullptr ? device : GetDC(button->window);
    if (target == nullptr)
        return;
    bool hidden = device == nullptr && HideCaret(button->window);
    button->caret = caret_action(button);
    RECT bounds;
    for (size_t index = next_visible(button, 0, &bounds); index < count;
         index = next_visible(button, index + 1, &bounds))
        paint_action(button, target, index, bounds);
    if (hidden)
        ShowCaret(button->window);
    if (device == nullptr)
        ReleaseDC(button->window, target);
}

void note_pane_code_button_exclude(const struct note_pane_code_button *_Nonnull button)
{
    size_t count = action_count(button);
    RECT bounds;
    for (size_t index = next_visible(button, 0, &bounds); index < count;
         index = next_visible(button, index + 1, &bounds))
        ValidateRect(button->window, &bounds);
}

static size_t action_at(const struct note_pane_code_button *_Nonnull button, LPARAM position)
{
    POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};
    size_t count = action_count(button);
    RECT bounds;
    for (size_t index = next_visible(button, 0, &bounds); index < count;
         index = next_visible(button, index + 1, &bounds))
        if (PtInRect(&bounds, point))
            return index;
    return no_action;
}

void note_pane_code_button_press(struct note_pane_code_button *_Nonnull button, LPARAM position)
{
    button->held = false;
    if (action_count(button) == 0)
        return;
    button->held_action = action_at(button, position);
    if (button->held_action == no_action)
        return;
    button->held = true;
    SendMessageW(button->window, EM_EXGETSEL, 0, (LPARAM)&button->held_selection);
    SendMessageW(button->window, EM_GETSCROLLPOS, 0, (LPARAM)&button->held_scroll);
}

/* 預かった選択とスクロール位置を戻す。選択の復元がキャレットまでスクロールしても元へ戻る。 */
static void restore_held(struct note_pane_code_button *_Nonnull button)
{
    button->held = false;
    SendMessageW(button->window, EM_EXSETSEL, 0, (LPARAM)&button->held_selection);
    SendMessageW(button->window, EM_SETSCROLLPOS, 0, (LPARAM)&button->held_scroll);
}

/* 離した後。押した札の上で離したクリックだけ、選択を戻す。外へ引いた選択はそのまま。 */
static void released(struct note_pane_code_button *_Nonnull button, LPARAM position)
{
    button->pressed = no_action;
    if (!button->held)
        return;
    if (action_at(button, position) == button->held_action)
        restore_held(button);
    button->held = false;
}

static void pointer_moved(struct note_pane_code_button *_Nonnull button, WPARAM keys,
                          LPARAM position)
{
    if (!button->tracking)
    {
        TRACKMOUSEEVENT request = {
            .cbSize = sizeof request, .dwFlags = TME_LEAVE, .hwndTrack = button->window};
        button->tracking = TrackMouseEvent(&request) != 0;
    }
    size_t over = action_at(button, position);
    /* 選択のドラッグ中は RichEdit が行を描き直し得るので、状態が同じでも描き直す。 */
    if (over == button->hover && (keys & MK_LBUTTON) == 0)
        return;
    button->hover = over;
    note_pane_code_button_paint(button, nullptr);
}

void note_pane_code_button_input(struct note_pane_code_button *_Nonnull button, UINT message,
                                 WPARAM wparam, LPARAM lparam)
{
    if (action_count(button) == 0)
        return;
    switch (message)
    {
    case WM_MOUSEMOVE:
        pointer_moved(button, wparam, lparam);
        return;
    case WM_MOUSELEAVE:
        button->tracking = false;
        button->hover = no_action;
        break;
    case WM_LBUTTONDOWN:
        button->pressed = action_at(button, lparam);
        break;
    case WM_LBUTTONUP:
        released(button, lparam);
        break;
    case WM_CAPTURECHANGED:
        button->pressed = no_action;
        break;
    case WM_CANCELMODE:
        button->pressed = no_action;
        if (button->held)
            restore_held(button);
        break;
    /* 選択・キャレット・スクロールが動く入力。RichEdit が WM_PAINT を経ずに描いても戻す。 */
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_MOUSEWHEEL:
    case WM_VSCROLL:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case EM_SETSEL:
    case EM_EXSETSEL:
    case EM_SETSCROLLPOS:
        break;
    default:
        return; /* Open Win32 message set. */
    }
    note_pane_code_button_paint(button, nullptr);
}

void note_pane_code_button_done(struct note_pane_code_button *_Nonnull button, size_t slot,
                                const char16_t *_Nonnull text)
{
    size_t count = action_count(button);
    size_t found = no_action;
    for (size_t index = 0; index < count && found == no_action; ++index)
        if (note_pane_links_action_slot(button->links, index) == slot)
            found = index;
    if (found == no_action)
        return;
    size_t length = 0;
    while (text[length] != u'\0' && length + 1 < label_capacity)
    {
        button->done_text[length] = (wchar_t)text[length];
        ++length;
    }
    button->done_text[length] = L'\0';
    button->done = found;
    SetTimer(button->window, note_pane_code_button_timer_id, done_milliseconds, nullptr);
    note_pane_code_button_paint(button, nullptr);
}

bool note_pane_code_button_timer(struct note_pane_code_button *_Nonnull button, WPARAM identifier)
{
    if (identifier != note_pane_code_button_timer_id)
        return false;
    KillTimer(button->window, note_pane_code_button_timer_id);
    button->done = no_action;
    note_pane_code_button_paint(button, nullptr);
    return true;
}

static void read_label(struct note_pane_code_button *_Nonnull button, ITextRange *_Nonnull range)
{
    BSTR _Nullable text = nullptr;
    if (FAILED(range->lpVtbl->GetText(range, &text)) || text == nullptr)
        return;
    size_t written = 0;
    for (size_t index = 0; text[index] != L'\0' && written + 1 < label_capacity; ++index)
        if (text[index] != L'\xA0')
            button->label[written++] = text[index];
    button->label[written] = L'\0';
    SysFreeString(text);
}

static void read_font(struct note_pane_code_button *_Nonnull button, ITextRange *_Nonnull range)
{
    ITextFont *_Nullable font = nullptr;
    if (FAILED(range->lpVtbl->GetFont(range, &font)) || font == nullptr)
        return;
    BSTR _Nullable name = nullptr;
    float points = 0;
    if (SUCCEEDED(font->lpVtbl->GetName(font, &name)) && name != nullptr)
        wcsncpy_s(button->face, LF_FACESIZE, name, _TRUNCATE);
    if (SUCCEEDED(font->lpVtbl->GetSize(font, &points)) && points > 0)
        button->twips = (int)(points * 20);
    SysFreeString(name);
    font->lpVtbl->Release(font);
}

/* 札の字の書体・大きさ・文言を文書から借りる。取れなければ既定のまま（絵だけになる）。 */
static void read_style(struct note_pane_code_button *_Nonnull button,
                       ITextDocument *_Nonnull document)
{
    CHARRANGE run = note_pane_links_action(button->links, 0);
    ITextRange *_Nullable range = nullptr;
    if (FAILED(document->lpVtbl->Range(document, run.cpMin, run.cpMax, &range)) || range == nullptr)
        return;
    read_label(button, range);
    read_font(button, range);
    range->lpVtbl->Release(range);
}

void note_pane_code_button_attach(struct note_pane_code_button *_Nonnull button,
                                  const struct note_pane_links *_Nullable links,
                                  ITextDocument *_Nullable document)
{
    KillTimer(button->window, note_pane_code_button_timer_id);
    button->links = links;
    button->hover = button->pressed = button->done = button->caret = no_action;
    button->label[0] = L'\0';
    button->twips = 180;
    wcscpy_s(button->face, LF_FACESIZE, L"Segoe UI");
    if (document != nullptr && note_pane_links_action_count(links) != 0)
        read_style(button, document);
}
