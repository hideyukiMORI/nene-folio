/* ADR0119 の決定 1: data フォルダの下の変更の知らせ。スレッドは持たない。合成ルートの入口の
 * 待ちが data_watch_wait を呼び、メッセージか、この知らせのどちらかで戻る。何が変わったかは
 * 言わない（読み直しの側が、同じなら何もしない）。 */
#ifndef NENEFOLIO_DATA_WATCH_H
#define NENEFOLIO_DATA_WATCH_H
#include "data_watch_outcome.h"
#include "data_watch_wake.h"
struct data_watch;
struct storage_path;
/* root の下（サブフォルダも）の、ファイル名・フォルダ名・大きさ・最終更新の変化を見張る。
 * 開いた時点で、最初の知らせを待っている。 */
[[nodiscard]] enum data_watch_outcome data_watch_open(const struct storage_path *_Nonnull root,
                                                      struct data_watch *_Nullable *_Nonnull out);
/* メッセージが来るか、見張りが変化を知らせるまで待つ。変化で戻った後は、data_watch_listen を
 * 呼ぶまで次の知らせを待たない（メッセージだけを待つ）: 知らせをすぐ出し直す場所や、書き込みが
 * 続くあいだも、待ちが空回りしない。watch が無い（張れなかった）ときと、知らせを頼めなくなった
 * 後も、メッセージだけを待つ。 */
[[nodiscard]] enum data_watch_wake data_watch_wait(struct data_watch *_Nullable watch);
/* 次の知らせを頼む。変化で戻った後に、呼ぶ側の都合のよい時（まとめの時間の後）に呼ぶ。そのあいだ
 * に起きた変化は OS が覚えていて、頼んだ直後の待ちがそれで戻る。待っている最中・watch が無い
 * ときは何もしない。 */
void data_watch_listen(struct data_watch *_Nullable watch);
void data_watch_destroy(struct data_watch *_Nullable watch);
#endif
