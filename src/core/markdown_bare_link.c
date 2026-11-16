#include "markdown_bare_link.h"

static bool letter(char value)
{
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

static bool alphanumeric(char value)
{
    return letter(value) || (value >= '0' && value <= '9');
}

static bool ascii_lower_is(char value, char expected)
{
    return value == expected || (value >= 'A' && value <= 'Z' && value + ('a' - 'A') == expected);
}

/* Length of `prefix` at `start` (ASCII case folded when `fold`), or zero. */
static size_t prefix_at(struct markdown_view view, size_t start, const char *_Nonnull prefix,
                        bool fold)
{
    size_t index = 0;
    for (; prefix[index] != '\0'; ++index)
    {
        char value = markdown_view_at(view, start + index);
        bool same = fold ? ascii_lower_is(value, prefix[index]) : value == prefix[index];
        if (start + index >= view.length || !same)
            return 0;
    }
    return index;
}

static bool space(char value)
{
    return value == ' ' || value == '\t' || value == '\n' || value == '\r' || value == '\f' ||
           value == '\v';
}

/* GFM: line start, whitespace or `*_~(`. Text after a non-ASCII character too (ADR0099). */
static bool www_start(struct markdown_view view, size_t start)
{
    if (start == 0)
        return true;
    char before = markdown_view_at(view, start - 1);
    return space(before) || before == '*' || before == '_' || before == '~' || before == '(' ||
           (unsigned char)before >= 0x80;
}

/* cmark-gfm rewinds over ASCII letters before a scheme; anything else may precede it. */
static size_t scheme_length(struct markdown_view view, size_t start)
{
    if (start != 0 && letter(markdown_view_at(view, start - 1)))
        return 0;
    size_t length = prefix_at(view, start, "https://", true);
    return length != 0 ? length : prefix_at(view, start, "http://", true);
}

/* The bytes before the domain: `www.` belongs to the domain, so its prefix is zero there. */
static bool prefix_length(struct markdown_view view, size_t start, size_t *_Nonnull length)
{
    if (markdown_bare_link_www(view, start))
    {
        *length = 0;
        return www_start(view, start);
    }
    *length = scheme_length(view, start);
    return *length != 0;
}

static bool domain_character(char value)
{
    return alphanumeric(value) || value == '-' || value == '_' || value == '.';
}

/* The end of the domain characters after `start`, or `start` past the DNS length. */
static size_t domain_run(struct markdown_view view, size_t start, size_t limit)
{
    size_t end = start;
    while (end < limit && domain_character(markdown_view_at(view, end)))
    {
        if (end - start == markdown_bare_link_domain_limit)
            return start;
        ++end;
    }
    return end;
}

static size_t count_of(struct markdown_view view, size_t start, size_t end, char wanted)
{
    size_t count = 0;
    for (size_t index = start; index < end; ++index)
        count += markdown_view_at(view, index) == wanted ? 1 : 0;
    return count;
}

/* Where the domain's last two labels begin. */
static size_t last_labels(struct markdown_view view, size_t start, size_t end)
{
    size_t periods = 0;
    for (size_t index = end; index > start; --index)
    {
        if (markdown_view_at(view, index - 1) == '.' && ++periods == 2)
            return index;
    }
    return start;
}

/* No `_` in the last two labels (cmark-gfm check_domain); `www.` needs a period. */
static bool domain_valid(struct markdown_view view, size_t start, size_t end, bool dotted)
{
    if (end == start || count_of(view, last_labels(view, start, end), end, '_') != 0)
        return false;
    return !dotted || count_of(view, start, end, '.') != 0;
}

/* ASCII printable text up to whitespace, `<` or a non-ASCII byte (ADR0099). */
static size_t candidate_end(struct markdown_view view, size_t start, size_t limit)
{
    size_t end = start;
    while (end < limit)
    {
        unsigned char value = (unsigned char)markdown_view_at(view, end);
        if (value <= ' ' || value >= 0x7F || value == '<')
            break;
        ++end;
    }
    return end;
}

static bool trailing_punctuation(char value)
{
    static const char marks[] = "?!.,:*_~'\"";
    for (size_t index = 0; index < sizeof marks - 1; ++index)
        if (marks[index] == value)
            return true;
    return false;
}

/* `&name;` leaves together; a lone `;` leaves alone (cmark-gfm autolink_delim). */
static size_t entity_trimmed(struct markdown_view view, size_t start, size_t end)
{
    size_t index = end - 1;
    while (index > start && letter(markdown_view_at(view, index - 1)))
        --index;
    return index > start && markdown_view_at(view, index - 1) == '&' ? index - 1 : end - 1;
}

/* One trailing mark or entity that is not part of the link: the new end, or `end`. */
static size_t trim_mark(struct markdown_view view, size_t start, size_t end)
{
    char last = markdown_view_at(view, end - 1);
    if (trailing_punctuation(last))
        return end - 1;
    return last == ';' ? entity_trimmed(view, start, end) : end;
}

/* Marks, entities and unmatched closing parentheses leave the end, in any order. The counts
 * are taken once: a trimmed mark or entity never holds a parenthesis. */
static size_t trimmed_end(struct markdown_view view, size_t start, size_t end)
{
    size_t opening = count_of(view, start, end, '(');
    size_t closing = count_of(view, start, end, ')');
    while (end > start)
    {
        size_t next = trim_mark(view, start, end);
        bool unmatched = markdown_view_at(view, end - 1) == ')' && closing > opening;
        if (next == end && !unmatched)
            break;
        closing -= next == end ? 1 : 0;
        end = next == end ? end - 1 : next;
    }
    return end;
}

size_t markdown_bare_link_length(struct markdown_view view, size_t start, size_t limit)
{
    size_t prefix = 0;
    if (limit > view.length || start >= limit || !prefix_length(view, start, &prefix))
        return 0;
    bool www = prefix == 0;
    size_t host = start + prefix;
    size_t domain = domain_run(view, host, limit);
    if (!domain_valid(view, host, domain, www))
        return 0;
    size_t end = trimmed_end(view, start, candidate_end(view, domain, limit));
    /* Trimming may reach into the domain (`www.example.com.`); what is left must still be one,
     * and more than the `www.` or scheme alone. */
    size_t kept = end < domain ? end : domain;
    if (end <= start + (www ? 4 : prefix) || !domain_valid(view, host, kept, www))
        return 0;
    return end - start;
}

bool markdown_bare_link_www(struct markdown_view view, size_t start)
{
    return prefix_at(view, start, "www.", false) != 0;
}
