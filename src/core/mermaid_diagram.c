#include "mermaid_diagram.h"
#include "diagram_statement.h"
#include "mermaid_class.h"
#include "mermaid_er.h"
#include "mermaid_flow.h"
#include "mermaid_gantt.h"
#include "mermaid_pie.h"
#include "mermaid_sequence.h"
#include "mermaid_state.h"
#include "note_text.h"
/* The first word of a diagram names its kind; each kind has its own reader. */
static const struct
{
    const char *_Nonnull word;
    enum diagram_outcome (*_Nonnull create)(struct diagram_text source,
                                            struct diagram_model *_Nullable *_Nonnull out);
} kinds[] = {{"flowchart", mermaid_flow_create},
             {"graph", mermaid_flow_create},
             {"sequenceDiagram", mermaid_sequence_create},
             {"stateDiagram", mermaid_state_create},
             {"stateDiagram-v2", mermaid_state_create},
             {"erDiagram", mermaid_er_create},
             {"classDiagram", mermaid_class_create},
             {"classDiagram-v2", mermaid_class_create},
             {"pie", mermaid_pie_create},
             {"gantt", mermaid_gantt_create}};
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
    for (size_t index = 0; index < sizeof kinds / sizeof kinds[0]; ++index)
        if (diagram_text_equal(header, kinds[index].word))
            return kinds[index].create(text, out);
    return DIAGRAM_UNSUPPORTED;
}
