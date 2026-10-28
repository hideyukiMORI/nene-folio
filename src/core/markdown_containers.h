#ifndef NENEFOLIO_MARKDOWN_CONTAINERS_H
#define NENEFOLIO_MARKDOWN_CONTAINERS_H
#include "markdown_container_event.h"
#include "markdown_container_match.h"
#include "markdown_line.h"
#include "markdown_paragraph_state.h"
#include "markdown_rtf_outcome.h"
struct markdown_containers;
[[nodiscard]] enum markdown_rtf_outcome
markdown_containers_create(struct markdown_containers *_Nullable *_Nonnull out);
void markdown_containers_destroy(struct markdown_containers *_Nullable containers);
[[nodiscard]] enum markdown_rtf_outcome
markdown_containers_accept(struct markdown_containers *_Nonnull containers,
                           struct markdown_line raw, struct markdown_line next,
                           enum markdown_paragraph_state paragraph);
/* Pure query. On LEFT/INVALID_INPUT the supplied query remains unchanged. */
[[nodiscard]] enum markdown_container_match
markdown_containers_match(const struct markdown_containers *_Nonnull containers,
                          struct markdown_line *_Nonnull line);
[[nodiscard]] struct markdown_line
markdown_containers_line(const struct markdown_containers *_Nonnull containers);
[[nodiscard]] enum markdown_container_event
markdown_containers_event(const struct markdown_containers *_Nonnull containers);
[[nodiscard]] size_t
markdown_containers_indent(const struct markdown_containers *_Nonnull containers);
[[nodiscard]] size_t
markdown_containers_quote_depth(const struct markdown_containers *_Nonnull containers);
[[nodiscard]] bool
markdown_containers_in_list(const struct markdown_containers *_Nonnull containers);
[[nodiscard]] bool
markdown_containers_task_start(const struct markdown_containers *_Nonnull containers);
[[nodiscard]] size_t
markdown_containers_pending(const struct markdown_containers *_Nonnull containers);
[[nodiscard]] size_t
markdown_containers_marker_indent(const struct markdown_containers *_Nonnull containers);
[[nodiscard]] size_t
markdown_containers_marker_number(const struct markdown_containers *_Nonnull containers);
[[nodiscard]] char
markdown_containers_marker_delimiter(const struct markdown_containers *_Nonnull containers);
[[nodiscard]] bool
markdown_containers_marker_ordered(const struct markdown_containers *_Nonnull containers);
void markdown_containers_consume_marker(struct markdown_containers *_Nonnull containers);
#endif
