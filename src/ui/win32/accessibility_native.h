#ifndef NENEFOLIO_ACCESSIBILITY_NATIVE_H
#define NENEFOLIO_ACCESSIBILITY_NATIVE_H

#include <objbase.h>
#include <uchar.h>
#include <windows.h>

/* UIのCOM apartmentで呼ぶ。native providerのValue/Text/Selectionは置き換えない。 */
[[nodiscard]] HRESULT accessibility_native_name(HWND _Nonnull window,
                                                const char16_t *_Nonnull name);

#endif
