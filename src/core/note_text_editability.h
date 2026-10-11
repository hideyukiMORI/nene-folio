/* note_text を編集の RichEdit へ渡してよいか（C-005・Issue #242）。 */
#ifndef NENEFOLIO_NOTE_TEXT_EDITABILITY_H
#define NENEFOLIO_NOTE_TEXT_EDITABILITY_H

enum note_text_editability : unsigned char
{
    NOTE_TEXT_EDITABLE,
    /* RichEdit が流し込みで別の文字へ変える文字を含む（U+0007 → U+0020・U+2028 → U+000B・
     * U+2029 → U+000D）。平文でも RTF でも同じ。2026-10-12 実測 */
    NOTE_TEXT_EDITOR_ALTERS
};

#endif
