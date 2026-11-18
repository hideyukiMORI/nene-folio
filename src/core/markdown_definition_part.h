#ifndef NENEFOLIO_MARKDOWN_DEFINITION_PART_H
#define NENEFOLIO_MARKDOWN_DEFINITION_PART_H
/* #971 / ADR0113: what an open paragraph is in a definition list. */
enum markdown_definition_part
{
    MARKDOWN_DEFINITION_NONE,
    MARKDOWN_DEFINITION_TERM,
    MARKDOWN_DEFINITION_DESCRIPTION
};
#endif
