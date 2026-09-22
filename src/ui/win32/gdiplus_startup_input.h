/* GdiplusStartup へ渡す束（SDK の GdiplusStartupInput と同じレイアウト・ADR 0033 の決定 2）。
 * SDK の gdiplus*.h は C++ 専用なので読めない。名前とメンバーの並びは SDK の gdiplusinit.h を
 * 写したもので、x64 のレイアウトが一致していることが署名の根拠になる（Win32 部品 probe で固定）。
 * DebugEventCallback は使わないが、void * にすると C-006 に触れるので関数ポインタで宣言して
 * 常に nullptr を入れる。GdiplusStartupOutput は背景スレッドを既定のままにするので渡さない
 * （nullptr）ため、この単位では型ごと置かない。 */
#ifndef NENEFOLIO_GDIPLUS_STARTUP_INPUT_H
#define NENEFOLIO_GDIPLUS_STARTUP_INPUT_H

#include <windows.h>

struct gdiplus_startup_input
{
    UINT32 version;                           /* SDK の GdiplusVersion。いまは 1 だけ */
    void(WINAPI *debug_event_callback)(void); /* SDK の DebugEventCallback。常に nullptr */
    BOOL suppress_background_thread;          /* SDK の SuppressBackgroundThread。FALSE */
    BOOL suppress_external_codecs;            /* SDK の SuppressExternalCodecs。FALSE */
};

#endif
