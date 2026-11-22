#ifndef NENEFOLIO_REQUEST_PROMPT_CHOICE_H
#define NENEFOLIO_REQUEST_PROMPT_CHOICE_H
/* #1152 / ADR0119: 編集中に「このノートを開いて」の依頼が来たときの、利用者の答え。 */
enum request_prompt_choice : unsigned char
{
    REQUEST_PROMPT_SAVE,       /* いまのノートを保存してから開く */
    REQUEST_PROMPT_KEEP,       /* 編集中の本文を別の名前で残してから開く */
    REQUEST_PROMPT_DECLINE,    /* 開かない（Esc・×・主窓からの取消も） */
    REQUEST_PROMPT_UNAVAILABLE /* 面を作れなかった。何も選ばれていない */
};
#endif
