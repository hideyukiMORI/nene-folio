/* ADR0119 の決定 1: data フォルダの下の変更の知らせ。スレッドは持たない。合成ルートの入口の
 * 待ちが data_watch_wait を呼び、メッセージか、この知らせのどちらかで戻る。何が変わったかは
 * 言わない（読み直しの側が、同じなら何もしない）。 */
#ifndef NENEFOLIO_DATA_WATCH_H
#define NENEFOLIO_DATA_WATCH_H
#include "data_watch_outcome.h"
#include "data_watch_wake.h"
struct data_watch;
struct storage_path;
/* root の下（サブフォルダも）の、ファイル名・フォルダ名・最終更新の変化を見張る。 */
[[nodiscard]] enum data_watch_outcome data_watch_open(const struct storage_path *_Nonnull root,
                                                      struct data_watch *_Nullable *_Nonnull out);
/* メッセージが来るか、見張りが変化を知らせるまで待つ。変化で戻るときは、次の知らせを張り直して
 * ある。watch が無い（張れなかった）ときと、張り直せなくなった後は、メッセージだけを待つ。 */
[[nodiscard]] enum data_watch_wake data_watch_wait(struct data_watch *_Nullable watch);
void data_watch_destroy(struct data_watch *_Nullable watch);
#endif
