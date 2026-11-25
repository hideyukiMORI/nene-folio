#ifndef NENEFOLIO_SEARCH_SCOPE_H
#define NENEFOLIO_SEARCH_SCOPE_H
/* 一覧の検索語を、どのノートに掛けるか（ADR 0122 の決定 6・#1292）。
 * ALL は、お気に入りとタグの条件を掛けずに、すべてのノートから探す（既定）。
 * FILTERED は、条件の中から探す（検索語と条件の AND）。 */
enum search_scope
{
    SEARCH_SCOPE_ALL,
    SEARCH_SCOPE_FILTERED,
};
#endif
