/* persistence_adapter の生成結果（C-005）。 */
#ifndef NENEFOLIO_PERSISTENCE_ADAPTER_OUTCOME_H
#define NENEFOLIO_PERSISTENCE_ADAPTER_OUTCOME_H

enum persistence_adapter_outcome : unsigned char
{
    PERSISTENCE_ADAPTER_CREATED,
    PERSISTENCE_ADAPTER_NO_MODULE_PATH, /* 実行ファイルの場所が取れない、または長すぎる */
    /* 同じ data/ を別のプロセスが開いている。復旧と編集の所有者は 1 つ（ADR 0022 の決定 3） */
    PERSISTENCE_ADAPTER_DATA_IN_USE,
    /* 改名の復旧記録があるのに錠を取れない。復旧できないので起動しない（決定 3 の補正） */
    PERSISTENCE_ADAPTER_RECOVERY_LOCKED,
    PERSISTENCE_ADAPTER_OUT_OF_MEMORY
};

#endif
