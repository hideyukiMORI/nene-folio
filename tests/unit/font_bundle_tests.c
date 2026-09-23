/* 同梱の書体の登録の判定（ADR 0036 の決定 3）。Win32 の列挙は adapters の font_bundle が行い、
 * 見つけた数と登録できた数だけを core の font_bundle_verdict が判定する。偽の列挙の 3 場面を踏む。
 */
#include "font_bundle_verdict.h"
#include "unit_tests.h"

static void verify_missing(void)
{
    require(font_bundle_verdict_of(0, 0) == FONT_BUNDLE_MISSING,
            "no fonts folder or no font file is MISSING");
}

static void verify_partial(void)
{
    require(font_bundle_verdict_of(6, 5) == FONT_BUNDLE_PARTIAL,
            "one refused font file out of six is PARTIAL");
    require(font_bundle_verdict_of(6, 0) == FONT_BUNDLE_PARTIAL,
            "font files found but none registered is PARTIAL, not MISSING");
}

static void verify_ready(void)
{
    require(font_bundle_verdict_of(6, 6) == FONT_BUNDLE_READY,
            "every found font file registered is READY");
    require(font_bundle_verdict_of(1, 1) == FONT_BUNDLE_READY, "a single registered file is READY");
}

void run_font_bundle_tests(void)
{
    verify_missing();
    verify_partial();
    verify_ready();
}
