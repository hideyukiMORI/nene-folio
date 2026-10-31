#ifndef NENEFOLIO_FOLDER_PICKER_H
#define NENEFOLIO_FOLDER_PICKER_H
#include "folder_picker_outcome.h"
#include "folio_language.h"
#include <windows.h>
struct utf8_text;
/* COM apartmentは呼出し元が所有する。OS面はpathの選択だけを行い、設定を書かない。 */
[[nodiscard]] enum folder_picker_outcome
folder_picker_show(HWND _Nonnull owner, const char *_Nonnull initial, enum folio_language language,
                   struct utf8_text *_Nullable *_Nonnull out);
#endif
