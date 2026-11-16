#ifndef NENEFOLIO_CODE_COPY_TARGET_OUTCOME_H
#define NENEFOLIO_CODE_COPY_TARGET_OUTCOME_H
enum code_copy_target_outcome : unsigned char
{
    CODE_COPY_TARGET_SLOT, /* 内部の宛先。slot は 0 以上・上限未満の正規形だった */
    CODE_COPY_TARGET_OTHER /* それ以外の宛先。通常のリンクとして扱う */
};
#endif
