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
    /* data/<category>/index.json。無ければ ABSENT。 */
    enum persistence_outcome (*_Nonnull read_note_ledger)(
        struct persistence_adapter *_Nonnull adapter, const char *_Nonnull category,
        struct note_ledger *_Nullable *_Nonnull out);
};

#endif
