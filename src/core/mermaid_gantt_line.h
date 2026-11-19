#ifndef NENEFOLIO_MERMAID_GANTT_LINE_H
#define NENEFOLIO_MERMAID_GANTT_LINE_H
#include "diagram_outcome.h"
#include "diagram_text.h"
#include "mermaid_gantt_task.h"
#include <stdint.h>
/* #992: one task line, `name : [tags,] [id,] [start,] end`, into a task (unresolved). */
[[nodiscard]] enum diagram_outcome mermaid_gantt_line_read(struct diagram_text text,
                                                           struct mermaid_gantt_task *_Nonnull out);
/* `YYYY-MM-DD` as a day number. INVALID for a date that does not exist or another shape. */
[[nodiscard]] enum diagram_outcome mermaid_gantt_line_date(struct diagram_text text,
                                                           int32_t *_Nonnull out);
#endif
