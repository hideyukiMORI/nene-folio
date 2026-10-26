#ifndef NENEFOLIO_NOTE_PANE_TABLE_H
#define NENEFOLIO_NOTE_PANE_TABLE_H
#include "note_pane_load_outcome.h"
#include "note_pane_table_scan.h"
#include <stddef.h>
#include <uchar.h>
#include <windows.h>
[[nodiscard]] enum note_pane_table_scan
note_pane_table_scan(HWND _Nonnull handle, const char16_t *_Nonnull units, size_t length);
/* Native row/cell structures are transient; raw positions remain authoritative. */
[[nodiscard]] enum note_pane_load_outcome note_pane_table_layout(HWND _Nonnull handle,
                                                                 const char16_t *_Nonnull units,
                                                                 size_t length, LONG width);
#endif
