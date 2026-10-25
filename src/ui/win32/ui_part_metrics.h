#ifndef NENEFOLIO_UI_PART_METRICS_H
#define NENEFOLIO_UI_PART_METRICS_H

#include "folio_language.h"
#include <windows.h>

/* 固定操作ラベルだけの派生幅。font再生成では同じHFONTでも失効する。 */
struct ui_part_metrics
{
    HFONT font;
    UINT dpi;
    enum folio_language language;
    int tracking;
    int widths[4];
    bool valid;
};

#endif
