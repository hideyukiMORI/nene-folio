#include "markdown_inline_tokens.h"
#include "markdown_autolink.h"
#include "markdown_character_reference.h"
#include "markdown_delimiter_kind.h"
#include "markdown_inline_kind.h"
#include "rtf_text.h"
#include "text_buffer.h"
#include "utf8_text.h"
#include <stdint.h>
#include <stdlib.h>

struct markdown_inline_tokens
{
    struct markdown_view view;
    struct markdown_inline_token
    {
        size_t start;
        size_t length;
        size_t mate;
        size_t previous;
        size_t after;
        size_t scope;
        enum markdown_inline_kind kind;
        bool opens;
        bool closes;
    } *_Nonnull tokens;
    struct text_buffer *_Nonnull out;
    size_t count;
};

static bool whitespace(char character)
{
    return character == ' ' || character == '\t' || character == '\n' || character == '\r' ||
           character == '\0';
}

static bool punctuation(char character)
{
    return (character > ' ' && character < '0') || (character > '9' && character < 'A') ||
           (character > 'Z' && character < 'a') || (character > 'z' && character < 0x7F);
}

static bool word(char character)
{
    return !whitespace(character) && !punctuation(character);
}

static char at(const struct markdown_inline_tokens *_Nonnull parser, size_t index)
{
    return markdown_view_at(parser->view, index);
}

static size_t run(struct markdown_view view, size_t index)
{
    size_t end = index + 1;
    while (end < view.length && markdown_view_at(view, end) == markdown_view_at(view, index))
        ++end;
    return end - index;
}

static bool delimiter_character(char character)
{
    return character == '*' || character == '_' || character == '~';
}

static bool run_character(char character)
{
    return delimiter_character(character) || character == '`' || character == ' ';
}

static bool literal_boundary(char character)
{
    return delimiter_character(character) || character == '`' || character == '\\' ||
           character == '[' || character == ']' || character == '(' || character == ')' ||
           character == '\n' || character == '<' || character == '>';
}

/* Both counting and emission consume exactly this borrowed lexical extent. */
static size_t extent(struct markdown_view view, size_t start)
{
    char character = markdown_view_at(view, start);
    if (character == '\\' && punctuation(markdown_view_at(view, start + 1)))
        return markdown_view_at(view, start + 1) == '`' ? 1 : 2;
    if (run_character(character))
        return run(view, start);
    if (literal_boundary(character))
        return 1;
    size_t end = start + 1;
    while (end < view.length && !literal_boundary(markdown_view_at(view, end)))
        ++end;
    if (markdown_view_at(view, end) != '\n')
        return end - start;
    /* This extent starts with a non-SP literal, so rewinding stays positive. */
    while (markdown_view_at(view, end - 1) == ' ')
        --end;
    return end - start;
}

static size_t token_count(struct markdown_view view, size_t *_Nonnull longest_backtick)
{
    size_t count = 0;
    *longest_backtick = 0;
    for (size_t start = 0; start < view.length;)
    {
        size_t length = extent(view, start);
        char character = markdown_view_at(view, start);
        count += delimiter_character(character) ? length / 2 + length % 2 : 1;
        if (character == '`' && length > *longest_backtick)
            *longest_backtick = length;
        start += length;
    }
    return count;
}

static void add_token(struct markdown_inline_tokens *_Nonnull parser, size_t start, size_t length)
{
    struct markdown_inline_token *token = (&parser->tokens[parser->count++]);
    *token = (struct markdown_inline_token){.start = start,
                                            .length = length,
                                            .mate = SIZE_MAX,
                                            .previous = SIZE_MAX,
                                            .after = SIZE_MAX,
                                            .scope = SIZE_MAX};
}

static bool left_flanking(char before, char after)
{
    return !whitespace(after) && (!punctuation(after) || whitespace(before) || punctuation(before));
}

static enum markdown_inline_kind delimiter_kind(char character, size_t width)
{
    if (character == '~')
        return width == 2 ? MARKDOWN_INLINE_STRIKE : MARKDOWN_INLINE_LITERAL;
    return width == 2 ? MARKDOWN_INLINE_BOLD : MARKDOWN_INLINE_ITALIC;
}

static void add_delimiter_run(struct markdown_inline_tokens *_Nonnull parser, size_t start,
                              size_t length)
{
    char character = at(parser, start);
    char before = start == 0 ? '\0' : at(parser, start - 1);
    char after = at(parser, start + length);
    bool opens = left_flanking(before, after);
    bool closes = left_flanking(after, before);
    if (character == '_' && word(before) && word(after))
        opens = closes = false;
    size_t used = 0;
    while (used < length)
    {
        size_t width = length - used >= 2 ? 2 : 1;
        if (closes && used == 0 && length % 2 != 0)
            width = 1;
        add_token(parser, start + used, width);
        struct markdown_inline_token *token = (&parser->tokens[parser->count - 1]);
        token->opens = opens;
        token->closes = closes;
        token->kind = delimiter_kind(character, width);
        used += width;
    }
}

static bool escaped_at(struct markdown_inline_tokens *_Nonnull parser, size_t index)
{
    size_t count = 0;
    while (index != 0 && at(parser, --index) == '\\')
        ++count;
    return count % 2 != 0;
}

static size_t tokenize_at(struct markdown_inline_tokens *_Nonnull parser, size_t start)
{
    char character = at(parser, start);
    size_t length = extent(parser->view, start);
    if (character == '\\' && punctuation(at(parser, start + 1)))
    {
        add_token(parser, start, length);
        ((&parser->tokens[parser->count - 1]))->kind = MARKDOWN_INLINE_ESCAPE;
        return length;
    }
    if (delimiter_character(character))
    {
        add_delimiter_run(parser, start, length);
        return length;
    }
    add_token(parser, start, length);
    ((&parser->tokens[parser->count - 1]))->opens = !escaped_at(parser, start);
    return length;
}

static void tokenize(struct markdown_inline_tokens *_Nonnull parser)
{
    for (size_t start = 0; start < parser->view.length;)
        start += tokenize_at(parser, start);
}

static size_t autolink_after(struct markdown_inline_tokens *_Nonnull parser, size_t index)
{
    struct markdown_inline_token *token = &parser->tokens[index];
    if (token->kind == MARKDOWN_INLINE_ESCAPE)
        return index;
    size_t length = markdown_autolink_length(parser->view, token->start);
    if (length == 0)
        return index;
    size_t after = index + 1;
    while (after < parser->count && parser->tokens[after].start < token->start + length)
        ++after;
    token->kind = MARKDOWN_INLINE_AUTOLINK;
    token->length = length;
    token->after = after;
    return after;
}

static void next_codes(struct markdown_inline_tokens *_Nonnull parser, size_t *_Nonnull next)
{
    for (size_t index = parser->count; index != 0;)
    {
        struct markdown_inline_token *token = (&parser->tokens[--index]);
        if (at(parser, token->start) != '`' || token->kind == MARKDOWN_INLINE_ESCAPE)
            continue;
        token->mate = next[token->length] == 0 ? SIZE_MAX : next[token->length] - 1;
        next[token->length] = index + 1;
    }
}

static void pair_verbatim(struct markdown_inline_tokens *_Nonnull parser, size_t *_Nonnull next)
{
    next_codes(parser, next);
    for (size_t index = 0; index < parser->count; ++index)
    {
        size_t after = autolink_after(parser, index);
        if (after != index)
        {
            index = after - 1;
            continue;
        }
        struct markdown_inline_token *token = (&parser->tokens[index]);
        if (at(parser, token->start) != '`' || token->mate == SIZE_MAX || !token->opens)
            continue;
        size_t close = token->mate;
        token->kind = MARKDOWN_INLINE_CODE;
        token->opens = true;
        ((&parser->tokens[close]))->kind = MARKDOWN_INLINE_CODE;
        ((&parser->tokens[close]))->mate = index;
        ((&parser->tokens[close]))->opens = false;
        index = close;
    }
}

static bool skip_literal_range(const struct markdown_inline_token *_Nonnull token,
                               size_t *_Nonnull index)
{
    if (token->kind == MARKDOWN_INLINE_AUTOLINK)
    {
        *index = token->after - 1;
        return true;
    }
    if (token->kind == MARKDOWN_INLINE_CODE && token->opens)
    {
        *index = token->mate;
        return true;
    }
    return false;
}

static void pair_bracket(struct markdown_inline_tokens *_Nonnull parser, size_t index,
                         size_t *_Nonnull head)
{
    struct markdown_inline_token *token = (&parser->tokens[index]);
    char character = at(parser, token->start);
    if (character == '[' || character == '(')
    {
        token->previous = *head;
        *head = index;
    }
    if ((character == ']' || character == ')') && *head != SIZE_MAX)
    {
        token->mate = *head;
        ((&parser->tokens[*head]))->mate = index;
        *head = ((&parser->tokens[*head]))->previous;
    }
}

static void pair_brackets(struct markdown_inline_tokens *_Nonnull parser)
{
    size_t brackets = SIZE_MAX;
    size_t parentheses = SIZE_MAX;
    for (size_t index = 0; index < parser->count; ++index)
    {
        struct markdown_inline_token *token = (&parser->tokens[index]);
        if (skip_literal_range(token, &index))
            continue;
        if (token->kind == MARKDOWN_INLINE_ESCAPE)
            continue;
        char character = at(parser, token->start);
        size_t *head = character == '[' || character == ']' ? &brackets : &parentheses;
        pair_bracket(parser, index, head);
    }
}

/* 既存mateから、limitの内側で閉じる[...](...)のendを返す。 */
static size_t closed_link_end(struct markdown_inline_tokens *_Nonnull parser, size_t index,
                              size_t limit)
{
    if (limit > parser->count || index >= limit || at(parser, parser->tokens[index].start) != '[')
    {
        return SIZE_MAX;
    }
    size_t close = parser->tokens[index].mate;
    if (close == SIZE_MAX || close <= index || close >= limit)
        return SIZE_MAX;
    size_t destination = close + 1;
    if (destination >= limit || at(parser, parser->tokens[destination].start) != '(')
        return SIZE_MAX;
    size_t end = parser->tokens[destination].mate;
    return end > destination && end < limit ? end : SIZE_MAX;
}

/* 直前の!がescapeされていないことを画像印の条件にする。 */
static bool image_marker(struct markdown_inline_tokens *_Nonnull parser, size_t index)
{
    size_t start = parser->tokens[index].start;
    return start != 0 && at(parser, start - 1) == '!' && !escaped_at(parser, start - 1);
}

static void mark_image_literal(struct markdown_inline_tokens *_Nonnull parser, size_t start,
                               size_t end)
{
    struct markdown_inline_token *open = &parser->tokens[start];
    open->kind = MARKDOWN_INLINE_IMAGE_LITERAL;
    open->opens = true;
    open->after = end + 1;
}

static void pair_label_images(struct markdown_inline_tokens *_Nonnull parser, size_t start,
                              size_t close)
{
    for (size_t index = start + 1; index < close; ++index)
    {
        struct markdown_inline_token *token = &parser->tokens[index];
        if (skip_literal_range(token, &index))
            continue;
        size_t end = closed_link_end(parser, index, close);
        if (end == SIZE_MAX || !image_marker(parser, index))
            continue;
        mark_image_literal(parser, index, end);
        index = end;
    }
}

static void pair_links(struct markdown_inline_tokens *_Nonnull parser)
{
    pair_brackets(parser);
    for (size_t index = 0; index < parser->count; ++index)
    {
        struct markdown_inline_token *token = &parser->tokens[index];
        if (skip_literal_range(token, &index))
            continue;
        size_t end = closed_link_end(parser, index, parser->count);
        if (end == SIZE_MAX)
            continue;
        size_t close = token->mate;
        if (image_marker(parser, index))
        {
            mark_image_literal(parser, index, end);
            index = end;
            continue;
        }
        token->kind = MARKDOWN_INLINE_LINK;
        token->opens = true;
        parser->tokens[close].kind = MARKDOWN_INLINE_LINK;
        parser->tokens[close].opens = false;
        parser->tokens[close].after = end + 1;
        pair_label_images(parser, index, close);
        index = end;
    }
}

static enum markdown_delimiter_kind delimiter_slot(struct markdown_inline_tokens *_Nonnull parser,
                                                   size_t index)
{
    const struct markdown_inline_token *token = (&parser->tokens[index]);
    char character = at(parser, token->start);
    switch (token->kind)
    {
    case MARKDOWN_INLINE_BOLD:
        return character == '*' ? MARKDOWN_DELIMITER_STAR_BOLD : MARKDOWN_DELIMITER_UNDERSCORE_BOLD;
    case MARKDOWN_INLINE_ITALIC:
        return character == '*' ? MARKDOWN_DELIMITER_STAR_ITALIC
                                : MARKDOWN_DELIMITER_UNDERSCORE_ITALIC;
    case MARKDOWN_INLINE_STRIKE:
        return MARKDOWN_DELIMITER_STRIKE;
    case MARKDOWN_INLINE_LITERAL:
    case MARKDOWN_INLINE_ESCAPE:
    case MARKDOWN_INLINE_CODE:
    case MARKDOWN_INLINE_LINK:
    case MARKDOWN_INLINE_IMAGE_LITERAL:
    case MARKDOWN_INLINE_AUTOLINK:
        return MARKDOWN_DELIMITER_COUNT;
    }
    return MARKDOWN_DELIMITER_COUNT;
}

static bool emphasis_token(enum markdown_inline_kind kind)
{
    return kind == MARKDOWN_INLINE_BOLD || kind == MARKDOWN_INLINE_ITALIC ||
           kind == MARKDOWN_INLINE_STRIKE;
}

static void pair_delimiter(struct markdown_inline_tokens *_Nonnull parser, size_t index,
                           size_t *_Nonnull head)
{
    struct markdown_inline_token *token = (&parser->tokens[index]);
    if (token->closes && *head != SIZE_MAX && ((&parser->tokens[*head]))->scope == token->scope)
    {
        token->mate = *head;
        ((&parser->tokens[token->mate]))->mate = index;
        *head = ((&parser->tokens[token->mate]))->previous;
        token->opens = false;
        return;
    }
    if (token->opens)
    {
        token->previous = *head;
        *head = index;
    }
}

static void leave_link(struct markdown_inline_tokens *_Nonnull parser, size_t *_Nonnull heads,
                       size_t scope)
{
    for (size_t slot = 0; slot < MARKDOWN_DELIMITER_COUNT; ++slot)
        while (heads[slot] != SIZE_MAX && ((&parser->tokens[heads[slot]]))->scope == scope)
            heads[slot] = ((&parser->tokens[heads[slot]]))->previous;
}

static size_t delimiter_scope(struct markdown_inline_tokens *_Nonnull parser, size_t index,
                              size_t scope, size_t *_Nonnull heads)
{
    struct markdown_inline_token *token = (&parser->tokens[index]);
    if (token->kind != MARKDOWN_INLINE_LINK)
        return scope;
    if (token->opens)
    {
        token->scope = scope;
        return index;
    }
    leave_link(parser, heads, scope);
    return ((&parser->tokens[scope]))->scope;
}

static bool skip_pair_range(const struct markdown_inline_token *token, size_t *_Nonnull index)
{
    if (token->kind == MARKDOWN_INLINE_LINK && !token->opens)
    {
        *index = token->after - 1;
        return true;
    }
    if (token->kind == MARKDOWN_INLINE_IMAGE_LITERAL)
    {
        *index = token->after - 1;
        return true;
    }
    return skip_literal_range(token, index);
}

static void pair_delimiters(struct markdown_inline_tokens *_Nonnull parser)
{
    size_t heads[MARKDOWN_DELIMITER_COUNT];
    for (size_t slot = 0; slot < MARKDOWN_DELIMITER_COUNT; ++slot)
        heads[slot] = SIZE_MAX;
    size_t scope = SIZE_MAX;
    for (size_t index = 0; index < parser->count; ++index)
    {
        struct markdown_inline_token *token = (&parser->tokens[index]);
        scope = delimiter_scope(parser, index, scope, heads);
        if (skip_pair_range(token, &index))
            continue;
        if (!emphasis_token(token->kind))
            continue;
        token->scope = scope;
        enum markdown_delimiter_kind slot = delimiter_slot(parser, index);
        pair_delimiter(parser, index, &heads[slot]);
    }
}

static size_t coherent_close(struct markdown_inline_tokens *_Nonnull parser, size_t index,
                             size_t *_Nonnull stack, size_t count)
{
    struct markdown_inline_token *token = &parser->tokens[index];
    while (count != 0 && parser->tokens[stack[count - 1]].mate == SIZE_MAX)
        --count;
    if (count != 0 && stack[count - 1] == token->mate)
        return count - 1;
    parser->tokens[token->mate].mate = SIZE_MAX;
    token->mate = SIZE_MAX;
    return count;
}

static void coherent_pairs(struct markdown_inline_tokens *_Nonnull parser, size_t *_Nonnull stack)
{
    size_t count = 0;
    for (size_t index = 0; index < parser->count; ++index)
    {
        struct markdown_inline_token *token = &parser->tokens[index];
        if (skip_pair_range(token, &index))
            continue;
        if (!emphasis_token(token->kind) || token->mate == SIZE_MAX)
            continue;
        if (token->opens)
        {
            stack[count++] = index;
            continue;
        }
        count = coherent_close(parser, index, stack, count);
    }
}

static size_t append_reference(struct markdown_inline_tokens *_Nonnull parser, size_t start,
                               size_t end)
{
    uint32_t value = 0;
    size_t length = markdown_character_reference_read(parser->view, start, &value);
    if (length == 0 || length > end - start)
        return 0;
    /* Inline line-ending references remain whitespace, never new block syntax. */
    if (value == '\r' || value == '\n')
        value = ' ';
    char encoded[utf8_text_max_encoded];
    size_t count = utf8_text_encode(value, encoded);
    rtf_text_append(parser->out, encoded, count);
    return length;
}

static size_t append_character(struct markdown_inline_tokens *_Nonnull parser, size_t index,
                               char character)
{
    size_t length = 1;
    if (parser->view.table_cell && character == '\\' && at(parser, index + 1) == '|')
    {
        character = '|';
        length = 2;
    }
    if (character == '\n')
        character = ' ';
    rtf_text_append(parser->out, &character, 1);
    return length;
}

static void append_range(struct markdown_inline_tokens *_Nonnull parser, size_t start, size_t end,
                         bool references)
{
    for (size_t index = start; index < end;)
    {
        char character = at(parser, index);
        size_t reference =
            references && character == '&' ? append_reference(parser, index, end) : 0;
        index += reference != 0 ? reference : append_character(parser, index, character);
    }
}

static void emit_code(struct markdown_inline_tokens *_Nonnull parser, size_t index)
{
    const struct markdown_inline_token *token = (&parser->tokens[index]);
    size_t start = token->start + token->length;
    size_t end = ((&parser->tokens[token->mate]))->start;
    bool nonspace = false;
    for (size_t offset = start; offset < end; ++offset)
        nonspace = nonspace || (at(parser, offset) != ' ' && at(parser, offset) != '\n');
    if (nonspace && (at(parser, start) == ' ' || at(parser, start) == '\n') &&
        (at(parser, end - 1) == ' ' || at(parser, end - 1) == '\n'))
    {
        ++start;
        --end;
    }
    text_buffer_append_text(parser->out, "{\\f1\\fs20\\cf4 ");
    append_range(parser, start, end, false);
    text_buffer_append_text(parser->out, "}");
}

static void emit_break(struct markdown_inline_tokens *_Nonnull parser, size_t index)
{
    size_t backslashes = 0;
    while (index > backslashes && at(parser, index - backslashes - 1) == '\\')
        ++backslashes;
    bool spaces = index >= 2 && at(parser, index - 1) == ' ' && at(parser, index - 2) == ' ';
    text_buffer_append_text(parser->out, spaces || backslashes % 2 != 0 ? "\\line " : " ");
}

static bool trailing_marker(struct markdown_inline_tokens *_Nonnull parser, size_t index)
{
    const struct markdown_inline_token *token = (&parser->tokens[index]);
    char character = at(parser, token->start);
    if (character != ' ' && character != '\\')
        return false;
    return at(parser, token->start + token->length) == '\n';
}

static void emit_style(struct markdown_inline_tokens *_Nonnull parser, size_t index)
{
    const struct markdown_inline_token *token = (&parser->tokens[index]);
    switch (token->kind)
    {
    case MARKDOWN_INLINE_BOLD:
        text_buffer_append_text(parser->out, token->opens ? "{\\b " : "}");
        return;
    case MARKDOWN_INLINE_ITALIC:
        text_buffer_append_text(parser->out, token->opens ? "{\\i " : "}");
        return;
    case MARKDOWN_INLINE_STRIKE:
        text_buffer_append_text(parser->out, token->opens ? "{\\strike " : "}");
        return;
    case MARKDOWN_INLINE_LINK:
        text_buffer_append_text(parser->out, token->opens ? "{\\cf4\\ul " : "\\ul0}");
        return;
    case MARKDOWN_INLINE_LITERAL:
    case MARKDOWN_INLINE_ESCAPE:
    case MARKDOWN_INLINE_CODE:
    case MARKDOWN_INLINE_IMAGE_LITERAL:
    case MARKDOWN_INLINE_AUTOLINK:
        return;
    }
}

static size_t emit_range_token(struct markdown_inline_tokens *_Nonnull parser, size_t index)
{
    const struct markdown_inline_token *token = (&parser->tokens[index]);
    if (token->kind == MARKDOWN_INLINE_AUTOLINK)
    {
        text_buffer_append_text(parser->out, "{\\cf4\\ul ");
        append_range(parser, token->start + 1, token->start + token->length - 1, false);
        text_buffer_append_text(parser->out, "\\ul0}");
        return token->after;
    }
    if (token->kind == MARKDOWN_INLINE_IMAGE_LITERAL)
    {
        size_t end = token->after - 1;
        const struct markdown_inline_token *last = &parser->tokens[end];
        append_range(parser, token->start, last->start + last->length, false);
        return token->after;
    }
    if (token->kind == MARKDOWN_INLINE_CODE && token->opens)
    {
        emit_code(parser, index);
        return token->mate + 1;
    }
    return index;
}

static size_t emit_token(struct markdown_inline_tokens *_Nonnull parser, size_t index)
{
    size_t after = emit_range_token(parser, index);
    if (after != index)
        return after;
    const struct markdown_inline_token *token = &parser->tokens[index];
    if (token->kind == MARKDOWN_INLINE_ESCAPE)
    {
        append_range(parser, token->start + 1, token->start + token->length, false);
        return index + 1;
    }
    if (token->mate != SIZE_MAX && token->kind != MARKDOWN_INLINE_LITERAL)
    {
        emit_style(parser, index);
        return token->after == SIZE_MAX ? index + 1 : token->after;
    }
    char character = at(parser, token->start);
    if (character == '\n')
        emit_break(parser, token->start);
    else if (!trailing_marker(parser, index))
        append_range(parser, token->start, token->start + token->length, true);
    return index + 1;
}

static void markdown_inline_tokens_destroy(struct markdown_inline_token *_Nullable tokens,
                                           size_t *_Nullable next)
{
    free(tokens);
    free(next);
}

enum markdown_rtf_outcome markdown_inline_tokens_render(struct text_buffer *_Nonnull out,
                                                        struct markdown_view view)
{
    if (!markdown_view_valid(view))
        return MARKDOWN_RTF_INVALID_INPUT;
    if (view.length == 0)
        return MARKDOWN_RTF_CONVERTED;
    if (view.length == SIZE_MAX)
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    if (view.length > SIZE_MAX / sizeof(struct markdown_inline_token) - 1)
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    size_t longest_backtick = 0;
    size_t count = token_count(view, &longest_backtick);
    size_t scratch = longest_backtick + 1;
    if (scratch < count)
        scratch = count;
    /* Each extent emits at most its byte length: count <= N, scratch <= N + 1.
     * The unchanged pre-scan length guard bounds both allocations. */
    struct markdown_inline_token *_Nullable tokens = calloc(count, sizeof *tokens);
    size_t *_Nullable next = calloc(scratch, sizeof *next);
    if (tokens == nullptr || next == nullptr)
    {
        markdown_inline_tokens_destroy(tokens, next);
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    }
    struct markdown_inline_tokens parser = {.view = view, .tokens = tokens, .out = out};
    tokenize(&parser);
    pair_verbatim(&parser, next);
    pair_links(&parser);
    pair_delimiters(&parser);
    coherent_pairs(&parser, next);
    for (size_t index = 0; index < parser.count;)
        index = emit_token(&parser, index);
    markdown_inline_tokens_destroy(tokens, next);
    return MARKDOWN_RTF_CONVERTED;
}
