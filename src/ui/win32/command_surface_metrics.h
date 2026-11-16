#ifndef NENEFOLIO_COMMAND_SURFACE_METRICS_H
#define NENEFOLIO_COMMAND_SURFACE_METRICS_H

#include "folio_language.h"
#include <stddef.h>
#include <windows.h>

/* 「このアプリについて」の文字行の数（ADR0097）。表は folio_window.c が持つ。 */
constexpr size_t command_surface_about_lines = 9;

/* 実font/幅で測った固定footer。結果だけの更新で再計測しない。 */
struct command_surface_metrics
{
    int width;
    UINT dpi;
    HFONT font;
    enum folio_language language;
    int notice;
    int settings_footer;
    int motion_help;
    int themes_footer;
    int tags_footer;
    int tag_button_height;
    bool tag_hint_visible;
    bool about_ready; /* アプリの情報を開いたときだけ測る */
    int about_footer;
    int about_text[command_surface_about_lines];
    bool about_single_row; /* リンク4つが等幅1行に収まる */
};

#endif
