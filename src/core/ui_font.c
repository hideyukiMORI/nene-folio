#include "ui_font.h"

/* 言語ごとの同梱の face の唯一の表（ADR 0036 の決定 4）。網羅は CNF-009 が守る
 * （eng/conformance-rules.json の lineTables の folio_language.h → ui_font.c）。
 * 資産は fonts/ にあり、manifest との一致は CNF-012 が守る。OS の退避は ui_font_fallback.c。 */
static const char *_Nonnull const faces[] = {
    [FOLIO_LANGUAGE_JA] = "Noto Sans JP",
    [FOLIO_LANGUAGE_EN] = "Arimo",
    [FOLIO_LANGUAGE_ZH_HANS] = "Noto Sans SC",
};

const char *_Nonnull ui_font_face(enum folio_language language)
{
    return faces[language];
}

const char *_Nonnull ui_font_pick(enum folio_language language, bool bundled_present,
                                  bool fallback_present)
{
    if (bundled_present)
    {
        return ui_font_face(language);
    }
    if (fallback_present)
    {
        return ui_font_fallback_face(language);
    }
    /* 日本語の退避は簡体字の一部だけがリンク先の字形になる（ADR 0032 の決定 4）。 */
    return ui_font_fallback_face(FOLIO_LANGUAGE_JA);
}
