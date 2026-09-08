/* JSON を人が読める整形（2 空白の字下げ・改行）で組み立てる書き手（stdio 無し・ARC-003）。
 * 途中の失敗は記憶され、json_writer_finish がまとめて返す。文字列は " \ と制御文字だけを
 * エスケープし、それ以外の UTF-8 はそのまま書く。 */
#ifndef NENEFOLIO_JSON_WRITER_H
#define NENEFOLIO_JSON_WRITER_H

#include "json_writer_outcome.h"

#include <stddef.h>
#include <stdint.h>

struct json_writer;

[[nodiscard]] enum json_writer_outcome
json_writer_create(struct json_writer *_Nullable *_Nonnull out);
void json_writer_object_begin(struct json_writer *_Nonnull writer);
void json_writer_object_end(struct json_writer *_Nonnull writer);
void json_writer_array_begin(struct json_writer *_Nonnull writer);
void json_writer_array_end(struct json_writer *_Nonnull writer);
void json_writer_key(struct json_writer *_Nonnull writer, const char *_Nonnull text);
void json_writer_string(struct json_writer *_Nonnull writer, const char *_Nonnull text);
void json_writer_unsigned(struct json_writer *_Nonnull writer, uint32_t value);
void json_writer_boolean(struct json_writer *_Nonnull writer, bool value);
/* 文書が完結していれば ACCEPTED。text / length はそのときだけ意味を持つ。 */
[[nodiscard]] enum json_writer_outcome
json_writer_finish(const struct json_writer *_Nonnull writer);
/* 終端付き。writer が生きている間だけ有効。 */
[[nodiscard]] const char *_Nonnull json_writer_text(const struct json_writer *_Nonnull writer);
[[nodiscard]] size_t json_writer_length(const struct json_writer *_Nonnull writer);
void json_writer_destroy(struct json_writer *_Nullable writer);

#endif
