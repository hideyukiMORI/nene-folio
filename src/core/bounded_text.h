/* 決まった大きさの文字の入れ物（終端付きの UTF-8）へ、あふれさせずに足す純粋な関数。
 * 画面や読み上げに出す文を何本かつなぐ所のためのもの: 収まらない分は文字の境で切り捨てる。
 * strcat_s / strcpy_s は使わない: 収まらないとエラーを返さず、プロセスを終了させる（#1129 /
 * #1131）。 */
#ifndef NENEFOLIO_BOUNDED_TEXT_H
#define NENEFOLIO_BOUNDED_TEXT_H
#include <stddef.h>
/* out（capacity バイト。capacity は 1 以上で、いまの中身は終端までが capacity に収まっている）の
 * 終端の後ろへ text を足す。収まらなければ、収まる所までを UTF-8 の文字の境で切って足す。
 * 終端は必ず付く。足した後の長さ（終端を除く）を返す。 */
size_t bounded_text_append(char *_Nonnull out, size_t capacity, const char *_Nonnull text);
/* out（capacity バイト。1 以上）を text にする。収まらなければ、収まる所までを UTF-8 の文字の境で
 * 切って入れる。終端は必ず付く。入れた長さ（終端を除く）を返す: text の長さと同じなら全部入った。
 */
size_t bounded_text_copy(char *_Nonnull out, size_t capacity, const char *_Nonnull text);
#endif
