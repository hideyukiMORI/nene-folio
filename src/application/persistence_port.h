/* 永続化ポート（ARC-003 / ARC-007）。data/ の走査と台帳の読み書きを、型のある関数ポインタの束として
 * 中核へ渡す。実装は adapters/win32 の persistence_adapter だけで、application はその型を不完全型
 * としてしか知らない（void * を使わないため・C-006）。
 * 台帳は最初の境界（アダプタ）で core の codec により検証済みの型へ変換される（ARC-008 /
 * ARC-009）。 */
#ifndef NENEFOLIO_PERSISTENCE_PORT_H
#define NENEFOLIO_PERSISTENCE_PORT_H

#include "persistence_outcome.h"

struct category_ledger;
struct name_list;
struct note_ledger;
struct note_text;
struct persistence_adapter;

struct persistence_port
{
    struct persistence_adapter *_Nonnull adapter;
    /* data/ 直下のディレクトリ名。data/ が無ければ ABSENT。 */
    enum persistence_outcome (*_Nonnull scan_categories)(
        struct persistence_adapter *_Nonnull adapter, struct name_list *_Nullable *_Nonnull out);
    /* data/<category>/ にある拡張子 .md のファイル名（拡張子なし）。 */
    enum persistence_outcome (*_Nonnull scan_notes)(struct persistence_adapter *_Nonnull adapter,
                                                    const char *_Nonnull category,
                                                    struct name_list *_Nullable *_Nonnull out);
    /* data/categories.json。無ければ ABSENT。 */
    enum persistence_outcome (*_Nonnull read_category_ledger)(
        struct persistence_adapter *_Nonnull adapter,
        struct category_ledger *_Nullable *_Nonnull out);
    /* data/categories.json を置き換える。途中で落ちても壊れた台帳を残さない。 */
    enum persistence_outcome (*_Nonnull write_category_ledger)(
        struct persistence_adapter *_Nonnull adapter,
        const struct category_ledger *_Nonnull ledger);
    /* data/<category>/<note>.md の本文。無ければ ABSENT、UTF-8 でなければ MALFORMED。 */
    enum persistence_outcome (*_Nonnull read_note)(struct persistence_adapter *_Nonnull adapter,
                                                   const char *_Nonnull category,
                                                   const char *_Nonnull note,
                                                   struct note_text *_Nullable *_Nonnull out);
    /* いま data/<category>/<note>.md にある本文を data/.history/<category>/<note>/1.md へ写し、
     * 前の版を 1 つずつ後ろへずらす（FR-017 / ADR 0012 の決定 3）。写せたら STORED、
     * 元の md が無ければ ABSENT（写すものが無い）、履歴を書けなければ UNWRITABLE。 */
    enum persistence_outcome (*_Nonnull archive_note)(struct persistence_adapter *_Nonnull adapter,
                                                      const char *_Nonnull category,
                                                      const char *_Nonnull note);
    /* data/<category>/<note>.md を置き換える。途中で落ちても壊れた本文を残さない（FR-006）。 */
    enum persistence_outcome (*_Nonnull write_note)(struct persistence_adapter *_Nonnull adapter,
                                                    const char *_Nonnull category,
                                                    const char *_Nonnull note,
                                                    const struct note_text *_Nonnull body);
    /* data/<from_category>/<note>.md を data/<to_category>/ へ移す（ADR 0008 の決定 4）。
     * 移動先に同じ名前の md があれば置き換えずに UNWRITABLE。
     * 名前の文字列は呼び出しの間だけ有効で、実装は複製せずに使い切る（他の関数と同じ約束）。 */
    enum persistence_outcome (*_Nonnull move_note)(struct persistence_adapter *_Nonnull adapter,
                                                   const char *_Nonnull from_category,
                                                   const char *_Nonnull note,
                                                   const char *_Nonnull to_category);
    /* data/<category>/index.json。無ければ ABSENT。 */
    enum persistence_outcome (*_Nonnull read_note_ledger)(
        struct persistence_adapter *_Nonnull adapter, const char *_Nonnull category,
        struct note_ledger *_Nullable *_Nonnull out);
    /* data/<category>/index.json を置き換える。途中で落ちても壊れた台帳を残さない（FR-008）。 */
    enum persistence_outcome (*_Nonnull write_note_ledger)(
        struct persistence_adapter *_Nonnull adapter, const char *_Nonnull category,
        const struct note_ledger *_Nonnull ledger);
};

#endif
