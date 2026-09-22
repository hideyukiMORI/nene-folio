#include "ui_face.h"

#include "ui_font.h"

/* face 名は印字できる ASCII だけ（core の単体が固定する）ので、1 バイト 1 単位で広げられる。
 * 収まらなければ切る（LF_FACESIZE は終端を含む）。 */
static void widen(const char *_Nonnull ascii, wchar_t *_Nonnull out)
{
    size_t at = 0;
    while (ascii[at] != '\0' && at + 1 < LF_FACESIZE)
    {
        out[at] = (wchar_t)(unsigned char)ascii[at];
        at += 1;
    }
    out[at] = L'\0';
}

/* 1 つでも来たらその face は在る。列挙は最初の 1 件で止める。 */
static int CALLBACK seen(const LOGFONTW *_Nonnull font, const TEXTMETRICW *_Nonnull metric,
                         DWORD type, LPARAM found)
{
    (void)font;
    (void)metric;
    (void)type;
    *(int *)found = 1;
    return 0;
}

static bool installed(const wchar_t *_Nonnull face)
{
    LOGFONTW request = {.lfCharSet = DEFAULT_CHARSET};
    size_t at = 0;
    while (face[at] != L'\0' && at + 1 < LF_FACESIZE)
    {
        request.lfFaceName[at] = face[at];
        at += 1;
    }
    request.lfFaceName[at] = L'\0';
    HDC screen = GetDC(nullptr);
    if (screen == nullptr)
    {
        return true; /* 確かめられないときは要求どおりにする（GDI の代替に任せる） */
    }
    int found = 0;
    EnumFontFamiliesExW(screen, &request, seen, (LPARAM)&found, 0);
    ReleaseDC(nullptr, screen);
    return found != 0;
}

void ui_face_for(enum folio_language language, wchar_t *_Nonnull out)
{
    widen(ui_font_face(language), out);
    if (installed(out))
    {
        return;
    }
    /* 無ければ日本語の face に落とす（簡体字の一部だけがリンク先の字形になる・決定 4）。 */
    widen(ui_font_face(FOLIO_LANGUAGE_JA), out);
}
