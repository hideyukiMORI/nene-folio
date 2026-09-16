/* 改名ポートの結果（C-005 / ARC-010 / ADR 0022 の決定 2）。
 * 記録を公開する前に断った拒否と、公開した後に途中で止まった PENDING と、完了を値で区別する。
 * 公開前の拒否では data/ に何も残らないので、application は意図を保持しない。 */
#ifndef NENEFOLIO_RENAME_OUTCOME_H
#define NENEFOLIO_RENAME_OUTCOME_H

enum rename_outcome : unsigned char
{
    RENAME_COMPLETED,       /* 履歴・md・index.json が移り、記録も消えた */
    RENAME_PENDING,         /* 記録は公開したが途中で止まった。同じ意図で再開できる */
    RENAME_NONE,            /* 復旧する記録が無い（recover_rename だけが返す） */
    RENAME_UNLOCKED,        /* data/ のロックが無いので書けない。記録は公開していない */
    RENAME_NAME_TAKEN,      /* 移動先の md か履歴が既にある（大小文字だけ違う名前を含む） */
    RENAME_UNSUPPORTED,     /* ローカル NTFS でない・照会できない・reparse point */
    RENAME_IDENTITY_FAILED, /* 元の md / 履歴の識別子を取れない */
    RENAME_JOURNAL_FAILED,  /* 記録を公開できない。data/ には何も残していない */
    RENAME_JOURNAL_BROKEN,  /* 記録が版 1 の形でない・名前が不正・台帳と合わない。消さない */
    RENAME_MISMATCHED,      /* 記録と data/ の実体が合わない（両方ある・両方無い・識別子違い） */
    RENAME_OUT_OF_MEMORY
};

#endif
