/* #1131: text is added to a fixed buffer without ever running past it. */
#include "bounded_text.h"
#include "unit_tests.h"
#include <string.h>

static void verify_fits(void)
{
    char out[8] = "";
    require(bounded_text_append(out, sizeof out, "abc") == 3 && strcmp(out, "abc") == 0,
            "text that fits is added whole");
    require(bounded_text_append(out, sizeof out, "defg") == 7 && strcmp(out, "abcdefg") == 0,
            "up to the last byte before the terminator");
    require(bounded_text_append(out, sizeof out, "h") == 7 && strcmp(out, "abcdefg") == 0,
            "a full buffer takes nothing more");
    require(bounded_text_append(out, sizeof out, "") == 7, "and nothing is nothing");
}

static void verify_cut(void)
{
    char guarded[10];
    memset(guarded, '#', sizeof guarded);
    guarded[0] = '\0';
    require(bounded_text_append(guarded, 6, "abcdefgh") == 5 && strcmp(guarded, "abcde") == 0 &&
                guarded[6] == '#',
            "what does not fit is cut, the terminator is written, and nothing past the buffer");
    char out[6] = "a";
    /* U+3042 is three bytes: one fits after "a", the second does not. */
    require(bounded_text_append(out, sizeof out, "\xE3\x81\x82\xE3\x81\x82") == 4 &&
                strcmp(out, "a\xE3\x81\x82") == 0,
            "the cut falls between characters, not inside one");
    char narrow[3] = "";
    require(bounded_text_append(narrow, sizeof narrow, "\xE3\x81\x82") == 0 && narrow[0] == '\0',
            "a character that does not fit at all is left out");
    char least[1] = {'x'};
    require(bounded_text_append(least, sizeof least, "abc") == 0 && least[0] == '\0',
            "a buffer of one byte holds the terminator only");
}

static void verify_copy(void)
{
    char out[8] = "earlier";
    require(bounded_text_copy(out, sizeof out, "new") == 3 && strcmp(out, "new") == 0,
            "a copy replaces what was there");
    require(bounded_text_copy(out, sizeof out, "exactly7") == 7 && strcmp(out, "exactly") == 0,
            "a text that does not fit is cut, and the length says it was");
    require(bounded_text_copy(out, sizeof out, "") == 0 && out[0] == '\0', "an empty text empties");
    char narrow[4] = "x";
    require(bounded_text_copy(narrow, sizeof narrow, "\xE3\x81\x82\xE3\x81\x82") == 3 &&
                strcmp(narrow, "\xE3\x81\x82") == 0,
            "the cut falls between characters");
}

void run_bounded_text_tests(void)
{
    verify_copy();
    verify_fits();
    verify_cut();
}
