#ifndef NENEFOLIO_FOOTNOTE_TARGET_OUTCOME_H
#define NENEFOLIO_FOOTNOTE_TARGET_OUTCOME_H
enum footnote_target_outcome : unsigned char
{
    FOOTNOTE_TARGET_NOTE,      /* 本文の番号の宛先。その番号の脚注へ移る */
    FOOTNOTE_TARGET_REFERENCE, /* 脚注の番号の宛先。その脚注を最初に参照した本文へ戻る */
    FOOTNOTE_TARGET_OTHER      /* それ以外の宛先。脚注の行き来ではない */
};
#endif
