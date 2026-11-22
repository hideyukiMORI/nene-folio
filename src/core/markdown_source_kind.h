#ifndef NENEFOLIO_MARKDOWN_SOURCE_KIND_H
#define NENEFOLIO_MARKDOWN_SOURCE_KIND_H
/* #1008: what a block of the reading view is, for its entry in the source map. */
enum markdown_source_kind : unsigned char
{
    MARKDOWN_SOURCE_PARAGRAPH,   /* a paragraph or a list item's text */
    MARKDOWN_SOURCE_HEADING,     /* ATX or setext */
    MARKDOWN_SOURCE_CODE_HEADER, /* a fence's opening line (its language label and Copy) */
    MARKDOWN_SOURCE_CODE_LINE,   /* one line of a fenced or an indented code block */
    MARKDOWN_SOURCE_CODE_END,    /* the closing fence; it adds no paragraph */
    MARKDOWN_SOURCE_DIAGRAM,     /* a mermaid fence, drawn as one image */
    MARKDOWN_SOURCE_RULE,        /* a thematic break */
    MARKDOWN_SOURCE_TABLE_ROW,   /* a native table row (the header row includes the delimiter) */
    MARKDOWN_SOURCE_LITERAL_ROW, /* a table row shown as written */
    MARKDOWN_SOURCE_DEFINITION,  /* a definition list's term or description */
    MARKDOWN_SOURCE_SEPARATOR,   /* the rule before the footnotes (no source of its own) */
    MARKDOWN_SOURCE_FOOTNOTE     /* one footnote, from its definition */
};
#endif
