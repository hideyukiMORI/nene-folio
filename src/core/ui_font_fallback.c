#include "ui_font.h"

/* 言語ごとの OS の退避の face の唯一の表（ADR 0036 の決定 4）。網羅は CNF-009 が守る
 * （eng/conformance-rules.json の lineTables の folio_language.h → ui_font_fallback.c）。
 * 実測（out/design/2026-09-23/language-probe/ の §2-1）:
 *   - Microsoft YaHei UI は日本語も簡体字も 1 字も欠かさない唯一の face である。
 *   - Yu Gothic UI は簡体字 18 字のうち 5 字を欠く（欠けた字だけが別の字形で描かれる）。
 *   - Segoe UI は CJK を 1 字も持たないので英語には使わない（ノート名が全部リンク先になる）。 */
static const char *_Nonnull const fallback_faces[] = {
    [FOLIO_LANGUAGE_JA] = "Yu Gothic UI",
    [FOLIO_LANGUAGE_EN] = "Yu Gothic UI",
    [FOLIO_LANGUAGE_ZH_HANS] = "Microsoft YaHei UI",
};

const char *_Nonnull ui_font_fallback_face(enum folio_language language)
{
    return fallback_faces[language];
}
