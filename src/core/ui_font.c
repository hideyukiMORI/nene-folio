#include "ui_font.h"

/* 言語ごとの face の唯一の表（ADR 0032 の決定 4）。網羅は CNF-009 が守る
 * （eng/conformance-rules.json の lineTables の folio_language.h → ui_font.c）。
 * 実測（out/design/2026-09-23/language-probe/ の §2-1）:
 *   - Microsoft YaHei UI は日本語も簡体字も 1 字も欠かさない唯一の face である。
 *   - Yu Gothic UI は簡体字 18 字のうち 5 字を欠く（欠けた字だけが別の字形で描かれる）。
 *   - Segoe UI は CJK を 1 字も持たないので英語には使わない（ノート名が全部リンク先になる）。
 * ui は起動時と切り替え時に face の実在を確かめ、無ければ日本語の face に落とす。 */
static const char *_Nonnull const faces[] = {
    [FOLIO_LANGUAGE_JA] = "Yu Gothic UI",
    [FOLIO_LANGUAGE_EN] = "Yu Gothic UI",
    [FOLIO_LANGUAGE_ZH_HANS] = "Microsoft YaHei UI",
};

const char *_Nonnull ui_font_face(enum folio_language language)
{
    return faces[language];
}
