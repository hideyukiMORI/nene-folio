#include "mermaid_diagram.h"
#include "diagram_statement.h"
#include "mermaid_flow.h"
#include "mermaid_sequence.h"
#include "note_text.h"
enum diagram_outcome mermaid_diagram_create(const struct note_text *_Nonnull source,
                                            struct diagram_model *_Nullable *_Nonnull out)
{
    if (note_text_length(source) > diagram_source_limit)
        return DIAGRAM_TOO_LARGE;
    struct diagram_text text = {note_text_bytes(source), note_text_length(source)};
    struct diagram_statement first;
    enum diagram_outcome outcome = diagram_statement_read(
        (struct diagram_input){.text = text, .kind = DIAGRAM_SEQUENCE}, 0, &first);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    struct diagram_text header = diagram_text_word(&first.text);
    if (diagram_text_equal(header, "flowchart") || diagram_text_equal(header, "graph"))
        return mermaid_flow_create(text, out);
    if (diagram_text_equal(header, "sequenceDiagram"))
        return mermaid_sequence_create(text, out);
    return DIAGRAM_UNSUPPORTED;
}
