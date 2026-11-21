/* ADR0119 の決定 6: data/.requests/open-*.json（外から置かれる「開く」依頼のファイル）を、
 * 古い順に 1 つずつ取り出して消す。中身は解釈しない（検証は core の open_request）。
 * いまの時刻を読むので adapters に置く（ARC-007）。 */
#ifndef NENEFOLIO_REQUEST_FILE_H
#define NENEFOLIO_REQUEST_FILE_H
#include "request_outcome.h"
#include <stddef.h>
/* folder（data の下の .requests。終端付き）の、更新のいちばん古い open-*.json を 1 つ取り出す。
 * - 中身を buffer へ写せて、ファイルを消せたときだけ TAKEN（*length に長さ）。消せないファイルは
 *   取り出さずに次を見る: 同じ依頼を何度も渡さない。読み取り専用の印は外して消す。
 * - 並べた時点で capacity より大きいもの・更新から 5 分より古いものは、読まずに消す（消せなければ
 *   そのまま）。取り出す候補には入れない。
 * - いま読めないファイル（書いている途中など）は、消さずに次を見る。
 * フォルダが無い・該当が無いなら NONE。候補にするのは古いほうから 32 個まで。道が長すぎて
 * 組み立てられないファイルは見ない。 */
[[nodiscard]] enum request_outcome request_file_take(const wchar_t *_Nonnull folder,
                                                     char *_Nonnull buffer, size_t capacity,
                                                     size_t *_Nonnull length);
#endif
