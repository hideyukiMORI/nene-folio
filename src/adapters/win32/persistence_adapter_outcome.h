/* persistence_adapter の生成結果（C-005）。 */
#ifndef NENEFOLIO_PERSISTENCE_ADAPTER_OUTCOME_H
#define NENEFOLIO_PERSISTENCE_ADAPTER_OUTCOME_H

enum persistence_adapter_outcome : unsigned char
{
    PERSISTENCE_ADAPTER_CREATED,
    PERSISTENCE_ADAPTER_NO_MODULE_PATH, /* 実行ファイルの場所が取れない、または長すぎる */
    PERSISTENCE_ADAPTER_OUT_OF_MEMORY
};

#endif
