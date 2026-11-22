/* #1195 / ADR0120: 本文の欄（RichEdit）へ渡す IRichEditOleCallback。編集できる RichEdit は OLE の
 * 投下先を自分で登録し、エクスプローラからのファイルを「埋め込みの物」として取り込む。ここで
 * それを断り、ファイルの並びは主窓の取り込みの道（folio_message_files_dropped。欄の WM_DROPFILES と
 * 同じ行き先）へ回す。文字の投下と貼り付けは文字としてだけ受けさせ、埋め込みの物の置き場は
 * 渡さない。挿入の問い合わせ（QueryInsertObject）は通す: Folio 自身が閲覧表示の画像を同じ入口から
 * 入れるので、断れない。 */
#ifndef NENEFOLIO_NOTE_PANE_OLE_H
#define NENEFOLIO_NOTE_PANE_OLE_H
#include <windows.h>

#include <richedit.h> /* windows.h の後で読む（並べ替えさせないために、段を分けている） */

#include <richole.h>
/* pane は本文の欄の窓。参照を 1 つ持つコールバックを out に返す（呼んだ側が EM_SETOLECALLBACK で
 * 欄へ渡し、自分の参照を Release する）。記憶が足りなければ偽で、out は変えない。 */
[[nodiscard]] bool note_pane_ole_create(HWND _Nonnull pane,
                                        IRichEditOleCallback *_Nullable *_Nonnull out);
#endif
