/* 改名ポートの結果（C-005 / ARC-010 / ADR 0022 の決定 2）。
 * 記録を公開する前に断った拒否と、公開した後の 2 値と、完了を値で区別する。
 * 公開前の拒否では data/ に何も残らないので、application は意図を保持しない。
 * 公開後は PENDING（再試行で進み得る）と HALTED（人が data/ を見ないと進まない）だけで、
 * application はどちらも保持して次の意図より先に再試行する。 */
#ifndef NENEFOLIO_RENAME_OUTCOME_H
#define NENEFOLIO_RENAME_OUTCOME_H

enum rename_outcome : unsigned char
{
    RENAME_COMPLETED, /* 履歴・md・index.json が移り、記録も消えた */
    /* ここから 2 つが「記録を公開した後」。application が意図を保持する。 */
    RENAME_PENDING, /* 段階の入出力・台帳・記録削除が失敗した。同じ意図の再試行で進み得る */
    RENAME_HALTED,  /* 識別子不一致・両方存在／両方不在・照会不能。data/ を直すまで進まない */
    RENAME_NONE,    /* 復旧する記録が無い（recover_rename だけが返す） */
    /* ここから下は「記録を公開する前」の拒否。data/ は何も変わっていない。 */
    RENAME_UNLOCKED,        /* data/ のロックが無いので書けない */
    RENAME_NAME_TAKEN,      /* 移動先の md か履歴が既にある（大小文字だけ違う名前を含む） */
    RENAME_UNSUPPORTED,     /* ローカル NTFS でない・照会できない・reparse point */
    RENAME_IDENTITY_FAILED, /* 元の md / 履歴の識別子を取れない */
    RENAME_JOURNAL_FAILED,  /* 記録を公開できない */
    RENAME_JOURNAL_BROKEN,  /* 記録が版 1 の形でない・名前が不正・台帳と合わない。消さない */
    RENAME_OUT_OF_MEMORY
};

#endif
