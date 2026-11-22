#ifndef NENEFOLIO_REQUEST_PROMPT_REQUEST_H
#define NENEFOLIO_REQUEST_PROMPT_REQUEST_H
struct folio_state;
/* #1152 / ADR0119: 面に出す名前（UTF-8。呼び出しの間だけ借りる）。言語と配色は state から引く。 */
struct request_prompt_request
{
    const struct folio_state *_Nonnull state;
    const char *_Nonnull requested; /* 開いてほしいと言われたノートの名前 */
    const char *_Nonnull current;   /* いま編集しているノートの名前 */
    const char *_Nonnull kept;      /* 別の名前で残すときに付く名前 */
};
#endif
