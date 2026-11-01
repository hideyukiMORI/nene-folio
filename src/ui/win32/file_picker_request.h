#ifndef NENEFOLIO_FILE_PICKER_REQUEST_H
#define NENEFOLIO_FILE_PICKER_REQUEST_H
#include "folio_language.h"
#include "ui_text.h"
struct file_picker_request
{
    enum folio_language language;
    enum ui_text title;
    const char *_Nonnull name;
    const char *_Nonnull extension;
};
#endif
