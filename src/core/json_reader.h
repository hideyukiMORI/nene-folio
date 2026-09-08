/* 文法を検証しながら JSON を字句の列として読む純関数の読み手（stdio 無し・ARC-003）。
 * 木（DOM）は作らない。値の解釈は各台帳の codec が行う（C-006）。
 * 文字列は \" \ \/ \b \f \n \r \t \uXXXX（サロゲート対を含む）を展開し、UTF-8 として検証する。
 * 入れ子は json_reader_max_depth まで。text は読み手より長く生きていなければならない。 */
#ifndef NENEFOLIO_JSON_READER_H
#define NENEFOLIO_JSON_READER_H

#include "json_reader_outcome.h"
#include "json_token.h"

#include <stddef.h>
#include <stdint.h>

struct json_reader;

constexpr size_t json_reader_max_depth = 8;

[[nodiscard]] enum json_reader_outcome
json_reader_create(const char *_Nonnull text, size_t length,
                   struct json_reader *_Nullable *_Nonnull out);
[[nodiscard]] enum json_token json_reader_next(struct json_reader *_Nonnull reader);
/* 直前が JSON_TOKEN_KEY / STRING のときの展開済み文字列（終端付き）。次の next まで有効。 */
[[nodiscard]] const char *_Nonnull json_reader_text(const struct json_reader *_Nonnull reader);
[[nodiscard]] size_t json_reader_text_length(const struct json_reader *_Nonnull reader);
/* 直前が JSON_TOKEN_UNSIGNED のときの値。 */
[[nodiscard]] uint32_t json_reader_unsigned(const struct json_reader *_Nonnull reader);
void json_reader_destroy(struct json_reader *_Nullable reader);

#endif
