#ifndef NENEFOLIO_OPEN_REQUEST_OUTCOME_H
#define NENEFOLIO_OPEN_REQUEST_OUTCOME_H
/* #1071 / ADR0118: 外から来た「開く」依頼の文字列を検証した結果。 */
enum open_request_outcome : unsigned char
{
    OPEN_REQUEST_ACCEPTED,
    OPEN_REQUEST_MALFORMED, /* 形が違う: スキーム・動作・query・%・UTF-8・行の範囲・重複したキー */
    OPEN_REQUEST_NAME_INVALID, /* カテゴリ名かノート名が名前の規則に合わない */
    OPEN_REQUEST_TOO_LONG,     /* 全体か、探す文が上限を超える */
    OPEN_REQUEST_OUT_OF_MEMORY
};
#endif
