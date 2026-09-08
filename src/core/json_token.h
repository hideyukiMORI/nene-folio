/* json_reader が返す字句。
 * JSON_TOKEN_END / MALFORMED / OUT_OF_MEMORY は終端で、以後も同じ値を返す。 */
#ifndef NENEFOLIO_JSON_TOKEN_H
#define NENEFOLIO_JSON_TOKEN_H

enum json_token : unsigned char
{
    JSON_TOKEN_OBJECT_BEGIN,
    JSON_TOKEN_OBJECT_END,
    JSON_TOKEN_ARRAY_BEGIN,
    JSON_TOKEN_ARRAY_END,
    JSON_TOKEN_KEY,
    JSON_TOKEN_STRING,
    JSON_TOKEN_UNSIGNED, /* 32 bit に収まる非負整数。json_reader_unsigned で読む */
    JSON_TOKEN_NUMBER,   /* それ以外の JSON 数値。この製品では値を使わない */
    JSON_TOKEN_TRUE,
    JSON_TOKEN_FALSE,
    JSON_TOKEN_NULL,
    JSON_TOKEN_END,
    JSON_TOKEN_MALFORMED,
    JSON_TOKEN_OUT_OF_MEMORY
};

#endif
