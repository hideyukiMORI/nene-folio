/* 正規表現ポート（ARC-003 / ADR 0028 の決定 1 / 2）。ICU を呼ぶ唯一の窓口で、実装は
 * adapters/win32 の regex_adapter だけ。application はその型を不完全型としてしか知らない
 * （void * を使わないため・C-006）。
 *
 * 操作は **1 回の呼び出しで閉じる粗い形**にしてある: `URegularExpression *` も本文のポインタも
 * application に持たせないので、「setText が本文を写さない」危険（probe の §11）が構造的に消える。
 */
#ifndef NENEFOLIO_REGEX_PORT_H
#define NENEFOLIO_REGEX_PORT_H

#include "regex_pattern_error.h"
#include "regex_scan_outcome.h"

struct regex_adapter;
struct regex_matches;
struct regex_request;

struct regex_port
{
    struct regex_adapter *_Nonnull adapter;
    /* request の本文とパターンを走査し、matches->capacity まで一致を書いて count に総数を書く。
     * 本文は長さ 0 でも実体のある番地であること（ICU は NULL を拒む）。
     * error は BAD_PATTERN のときだけ意味を持ち、それ以外では offset が 0 になる。 */
    enum regex_scan_outcome (*_Nonnull scan)(struct regex_adapter *_Nonnull adapter,
                                             const struct regex_request *_Nonnull request,
                                             struct regex_matches *_Nonnull matches,
                                             struct regex_pattern_error *_Nonnull error);
};

#endif
