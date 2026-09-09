/* 右ペインの表示モード。閉じた集合（C-002）。所有者は folio_state（ARC-004）で、
 * 編集中の本文そのものは UI（RichEdit）が持つ（ADR 0006）。 */
#ifndef NENEFOLIO_PANE_MODE_H
#define NENEFOLIO_PANE_MODE_H

enum pane_mode : unsigned char
{
    PANE_MODE_VIEW,
    PANE_MODE_EDIT
};

#endif
