#include "ui_font.h"

/* 等幅の書体の face（#1217・ADR 0036 の 2026-11-23 の補正）。言語で変わらないので表は持たない。
 * 同梱は fonts/ の IBM Plex Mono、退避は Windows に入っている Consolas。 */
const char *_Nonnull ui_font_mono_face(void)
{
    return "IBM Plex Mono";
}

const char *_Nonnull ui_font_mono_fallback_face(void)
{
    return "Consolas";
}

const char *_Nonnull ui_font_mono_pick(bool bundled_present)
{
    return bundled_present ? ui_font_mono_face() : ui_font_mono_fallback_face();
}

/* IBM Plex Mono は、同じ大きさの Consolas より字の送りが広く、行も高い。1 小さくすると、
 * 96 / 120 / 144 / 192 DPI のどれでも字の送りが Consolas と同じになり、行の高さも ±1 画素に
 * 収まる（実測。ADR 0036 の 2026-11-23 の補正）。寸法は Consolas の大きさで決めてあるので、
 * 同梱の書体のときだけ 1 引く。 */
int ui_font_mono_size(int size, bool bundled_present)
{
    return bundled_present ? size - 1 : size;
}
