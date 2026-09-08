#include "json_reader.h"
#include "json_writer.h"
#include "unit_tests.h"

#include <string.h>

static struct json_reader *_Nonnull open_reader(const char *_Nonnull text)
{
    struct json_reader *reader = nullptr;
    require(json_reader_create(text, strlen(text), &reader) == JSON_READER_CREATED,
            "reader create");
    return reader;
}

/* 文書全体が期待どおりの字句列になるか。tokens は JSON_TOKEN_END で終わる。 */
static void expect_tokens(const char *_Nonnull text, const enum json_token *_Nonnull tokens)
{
    struct json_reader *reader = open_reader(text);
    for (size_t index = 0;; ++index)
    {
        enum json_token token = json_reader_next(reader);
        if (token != tokens[index])
        {
            require(false, text);
        }
        if (token == JSON_TOKEN_END)
        {
            break;
        }
    }
    require(json_reader_next(reader) == JSON_TOKEN_END, "end is sticky");
    json_reader_destroy(reader);
}

static void expect_malformed(const char *_Nonnull text)
{
    struct json_reader *reader = open_reader(text);
    for (size_t step = 0; step < 32; ++step)
    {
        enum json_token token = json_reader_next(reader);
        if (token == JSON_TOKEN_MALFORMED)
        {
            require(json_reader_next(reader) == JSON_TOKEN_MALFORMED, "malformed is sticky");
            json_reader_destroy(reader);
            return;
        }
        if (token == JSON_TOKEN_END)
        {
            break;
        }
    }
    require(false, text);
}

static void verify_document_tokens(void)
{
    static const enum json_token document[] = {
        JSON_TOKEN_OBJECT_BEGIN, JSON_TOKEN_KEY,         JSON_TOKEN_UNSIGNED,
        JSON_TOKEN_KEY,          JSON_TOKEN_ARRAY_BEGIN, JSON_TOKEN_OBJECT_BEGIN,
        JSON_TOKEN_KEY,          JSON_TOKEN_STRING,      JSON_TOKEN_KEY,
        JSON_TOKEN_TRUE,         JSON_TOKEN_OBJECT_END,  JSON_TOKEN_ARRAY_END,
        JSON_TOKEN_OBJECT_END,   JSON_TOKEN_END};
    expect_tokens(" {\"version\" : 1 ,\r\n\"items\": [ {\"name\":\"a\", \"on\": true} ] } ",
                  document);
    static const enum json_token literals[] = {JSON_TOKEN_ARRAY_BEGIN, JSON_TOKEN_TRUE,
                                               JSON_TOKEN_FALSE,       JSON_TOKEN_NULL,
                                               JSON_TOKEN_ARRAY_END,   JSON_TOKEN_END};
    expect_tokens("[true,false,null]", literals);
    static const enum json_token empties[] = {
        JSON_TOKEN_OBJECT_BEGIN, JSON_TOKEN_KEY,       JSON_TOKEN_OBJECT_BEGIN,
        JSON_TOKEN_OBJECT_END,   JSON_TOKEN_KEY,       JSON_TOKEN_ARRAY_BEGIN,
        JSON_TOKEN_ARRAY_BEGIN,  JSON_TOKEN_ARRAY_END, JSON_TOKEN_ARRAY_END,
        JSON_TOKEN_OBJECT_END,   JSON_TOKEN_END};
    expect_tokens("{\"a\":{},\"b\":[[]]}", empties);
    static const enum json_token scalar[] = {JSON_TOKEN_STRING, JSON_TOKEN_END};
    expect_tokens("\"alone\"", scalar);
}

static void verify_strings(void)
{
    struct json_reader *reader =
        open_reader("\"a\\\"b\\\\c\\/d\\b\\f\\n\\r\\t\\u65e5\\ud83d\\ude00\\u0041\"");
    require(json_reader_next(reader) == JSON_TOKEN_STRING, "escaped string");
    require(same_text(json_reader_text(reader), "a\"b\\c/d\b\f\n\r\t\xE6\x97\xA5\xF0\x9F\x98\x80"
                                                "A"),
            "escapes expanded");
    require(json_reader_text_length(reader) == 20, "text length");
    json_reader_destroy(reader);
    char long_text[300];
    long_text[0] = '"';
    memset(long_text + 1, 'x', 200);
    long_text[201] = '"';
    long_text[202] = '\0';
    reader = open_reader(long_text);
    require(json_reader_next(reader) == JSON_TOKEN_STRING && json_reader_text_length(reader) == 200,
            "scratch grows");
    json_reader_destroy(reader);
    expect_malformed("\"unterminated");
    expect_malformed("\"control\x01\"");
    expect_malformed("\"bad\\x\"");
    expect_malformed("\"\\");
    expect_malformed("\"\\u12\"");
    expect_malformed("\"\\u12G4\"");
    expect_malformed("\"\\ud83d\"");
    expect_malformed("\"\\ud83d\\u0041\"");
    expect_malformed("\"\\ude00\"");
    expect_malformed("\"\\ud83dx\"");
    expect_malformed("\"\xC3\"");
}

static void verify_numbers(void)
{
    struct json_reader *reader = open_reader("[0, 12, -3, 1.5, 2e3, 4E+2, 4294967295, 4294967296]");
    require(json_reader_next(reader) == JSON_TOKEN_ARRAY_BEGIN, "numbers begin");
    require(json_reader_next(reader) == JSON_TOKEN_UNSIGNED && json_reader_unsigned(reader) == 0,
            "zero");
    require(json_reader_next(reader) == JSON_TOKEN_UNSIGNED && json_reader_unsigned(reader) == 12,
            "twelve");
    require(json_reader_next(reader) == JSON_TOKEN_NUMBER, "negative");
    require(json_reader_next(reader) == JSON_TOKEN_NUMBER, "fraction");
    require(json_reader_next(reader) == JSON_TOKEN_NUMBER, "exponent");
    require(json_reader_next(reader) == JSON_TOKEN_NUMBER, "signed exponent");
    require(json_reader_next(reader) == JSON_TOKEN_UNSIGNED &&
                json_reader_unsigned(reader) == 4294967295u,
            "max unsigned");
    require(json_reader_next(reader) == JSON_TOKEN_NUMBER, "overflow is a number");
    require(json_reader_next(reader) == JSON_TOKEN_ARRAY_END, "numbers end");
    json_reader_destroy(reader);
    expect_malformed("01");
    expect_malformed("-");
    expect_malformed("1.");
    expect_malformed("1e");
    expect_malformed("1e+");
    expect_malformed("+1");
}

static void verify_grammar(void)
{
    expect_malformed("");
    expect_malformed("   ");
    expect_malformed("[1,]");
    expect_malformed("{\"a\":1,}");
    expect_malformed("{\"a\" 1}");
    expect_malformed("{a:1}");
    expect_malformed("{\"a\"}");
    expect_malformed("{,}");
    expect_malformed("[,]");
    expect_malformed("]");
    expect_malformed("}");
    expect_malformed("[}");
    expect_malformed("{]");
    expect_malformed("[1 2]");
    expect_malformed("{} x");
    expect_malformed("1 2");
    expect_malformed("tru");
    expect_malformed("nul");
    expect_malformed("[[[[[[[[[1]]]]]]]]]");
    expect_malformed("[");
    expect_malformed("{\"a\":");
    expect_malformed("[1");
}

static struct json_writer *_Nonnull open_writer(void)
{
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "writer create");
    return writer;
}

static void verify_writer_document(void)
{
    struct json_writer *writer = open_writer();
    json_writer_object_begin(writer);
    json_writer_key(writer, "version");
    json_writer_unsigned(writer, 1);
    json_writer_key(writer, "max");
    json_writer_unsigned(writer, 4294967295u);
    json_writer_key(writer, "zero");
    json_writer_unsigned(writer, 0);
    json_writer_key(writer, "text");
    json_writer_string(writer, "q\"b\\s\n\xE6\x97\xA5");
    json_writer_key(writer, "flags");
    json_writer_array_begin(writer);
    json_writer_boolean(writer, true);
    json_writer_boolean(writer, false);
    json_writer_array_end(writer);
    json_writer_key(writer, "empty");
    json_writer_array_begin(writer);
    json_writer_array_end(writer);
    json_writer_key(writer, "inner");
    json_writer_object_begin(writer);
    json_writer_object_end(writer);
    json_writer_object_end(writer);
    require(json_writer_finish(writer) == JSON_WRITER_ACCEPTED, "writer finish");
    const char *expected = "{\n  \"version\": 1,\n  \"max\": 4294967295,\n  \"zero\": 0,\n"
                           "  \"text\": \"q\\\"b\\\\s\\u000A\xE6\x97\xA5\",\n"
                           "  \"flags\": [\n    true,\n    false\n  ],\n  \"empty\": [],\n"
                           "  \"inner\": {}\n}";
    require(same_text(json_writer_text(writer), expected), "writer text");
    require(json_writer_length(writer) == strlen(expected), "writer length");
    struct json_reader *reader = open_reader(json_writer_text(writer));
    for (size_t step = 0; step < 40; ++step)
    {
        enum json_token token = json_reader_next(reader);
        require(token != JSON_TOKEN_MALFORMED, "writer output is readable");
        if (token == JSON_TOKEN_END)
        {
            break;
        }
    }
    json_reader_destroy(reader);
    json_writer_destroy(writer);
    json_writer_destroy(nullptr);
}

static void verify_writer_growth(void)
{
    struct json_writer *writer = open_writer();
    json_writer_array_begin(writer);
    for (size_t index = 0; index < 200; ++index)
    {
        json_writer_string(writer, "0123456789");
    }
    json_writer_array_end(writer);
    require(json_writer_finish(writer) == JSON_WRITER_ACCEPTED, "large document");
    require(json_writer_length(writer) > 2000, "buffer grew");
    json_writer_destroy(writer);
}

static void expect_writer_malformed(void (*_Nonnull build)(struct json_writer *_Nonnull writer),
                                    const char *_Nonnull description)
{
    struct json_writer *writer = open_writer();
    build(writer);
    require(json_writer_finish(writer) == JSON_WRITER_MALFORMED, description);
    /* 失敗は記憶され、以後の書き込みでは覆らない。 */
    json_writer_object_begin(writer);
    json_writer_key(writer, "k");
    json_writer_string(writer, "v");
    json_writer_boolean(writer, true);
    json_writer_unsigned(writer, 1);
    json_writer_array_begin(writer);
    json_writer_array_end(writer);
    json_writer_object_end(writer);
    require(json_writer_finish(writer) == JSON_WRITER_MALFORMED, "writer failure is sticky");
    json_writer_destroy(writer);
}

static void build_value_without_key(struct json_writer *_Nonnull writer)
{
    json_writer_object_begin(writer);
    json_writer_unsigned(writer, 1);
    json_writer_object_end(writer);
}

static void build_key_in_array(struct json_writer *_Nonnull writer)
{
    json_writer_array_begin(writer);
    json_writer_key(writer, "k");
    json_writer_array_end(writer);
}

static void build_key_without_value(struct json_writer *_Nonnull writer)
{
    json_writer_object_begin(writer);
    json_writer_key(writer, "k");
    json_writer_object_end(writer);
}

static void build_double_key(struct json_writer *_Nonnull writer)
{
    json_writer_object_begin(writer);
    json_writer_key(writer, "k");
    json_writer_key(writer, "k");
}

static void build_mismatched_end(struct json_writer *_Nonnull writer)
{
    json_writer_array_begin(writer);
    json_writer_object_end(writer);
}

static void build_two_roots(struct json_writer *_Nonnull writer)
{
    json_writer_unsigned(writer, 1);
    json_writer_unsigned(writer, 2);
}

static void build_unbalanced(struct json_writer *_Nonnull writer)
{
    json_writer_array_begin(writer);
}

static void build_root_key(struct json_writer *_Nonnull writer)
{
    json_writer_key(writer, "k");
}

static void build_end_at_root(struct json_writer *_Nonnull writer)
{
    json_writer_array_end(writer);
}

static void build_too_deep(struct json_writer *_Nonnull writer)
{
    for (size_t depth = 0; depth < 9; ++depth)
    {
        json_writer_array_begin(writer);
    }
}

/* 何も書いていない書き手は未完成であり、失敗ではない。根を書けば完結する。 */
static void verify_writer_empty(void)
{
    struct json_writer *writer = open_writer();
    require(json_writer_finish(writer) == JSON_WRITER_MALFORMED, "nothing written");
    json_writer_unsigned(writer, 7);
    require(json_writer_finish(writer) == JSON_WRITER_ACCEPTED &&
                same_text(json_writer_text(writer), "7"),
            "root completes the document");
    json_writer_destroy(writer);
}

static void verify_writer_rejections(void)
{
    verify_writer_empty();
    expect_writer_malformed(build_value_without_key, "value without key");
    expect_writer_malformed(build_key_in_array, "key in array");
    expect_writer_malformed(build_key_without_value, "key without value");
    expect_writer_malformed(build_double_key, "double key");
    expect_writer_malformed(build_mismatched_end, "mismatched end");
    expect_writer_malformed(build_two_roots, "two roots");
    expect_writer_malformed(build_unbalanced, "unbalanced");
    expect_writer_malformed(build_root_key, "key at root");
    expect_writer_malformed(build_end_at_root, "end at root");
    expect_writer_malformed(build_too_deep, "too deep");
}

void run_json_tests(void)
{
    verify_document_tokens();
    verify_strings();
    verify_numbers();
    verify_grammar();
    verify_writer_document();
    verify_writer_growth();
    verify_writer_rejections();
    json_reader_destroy(nullptr);
}
