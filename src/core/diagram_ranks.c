/* DFS cycle orientation and longest-path ranking adapted from Pele's layered.ts:
 * https://github.com/obsidianmd/pele/blob/3a2f075ee0c3d914f0fad3545fc0f193693975ac/src/layout/layered.ts
 * Copyright (c) 2026 kepano, MIT. See THIRD_PARTY_NOTICES.txt.
 * Packed adjacency and an explicit stack keep each pass O(nodes + edges), without recursion. */
#include "diagram_ranks.h"
#include "diagram_visit.h"
#include <stdlib.h>
#include <string.h>
struct diagram_ranks
{
    size_t values[diagram_node_limit];
    bool reversed[diagram_link_limit];
    size_t starts[diagram_node_limit + 1];
    size_t edges[diagram_link_limit];
    size_t cursor[diagram_node_limit];
    size_t incoming[diagram_node_limit];
    size_t stack[diagram_node_limit];
    enum diagram_visit visits[diagram_node_limit];
    size_t maximum;
};
static struct diagram_link oriented(const struct diagram_ranks *_Nonnull ranks,
                                    const struct diagram_model *_Nonnull model, size_t index)
{
    struct diagram_link link = diagram_model_link_at(model, index);
    if (ranks->reversed[index])
    {
        size_t from = link.from;
        link.from = link.to;
        link.to = from;
    }
    return link;
}
static void adjacency(struct diagram_ranks *_Nonnull ranks,
                      const struct diagram_model *_Nonnull model)
{
    memset(ranks->starts, 0, sizeof ranks->starts);
    memset(ranks->incoming, 0, sizeof ranks->incoming);
    size_t count = diagram_model_node_count(model);
    for (size_t index = 0; index < diagram_model_link_count(model); ++index)
    {
        struct diagram_link link = oriented(ranks, model, index);
        if (link.from == link.to)
            continue;
        ++ranks->starts[link.from + 1];
        ++ranks->incoming[link.to];
    }
    for (size_t index = 0; index < count; ++index)
        ranks->starts[index + 1] += ranks->starts[index];
    memcpy(ranks->cursor, ranks->starts, count * sizeof ranks->cursor[0]);
    for (size_t index = 0; index < diagram_model_link_count(model); ++index)
    {
        struct diagram_link link = oriented(ranks, model, index);
        if (link.from != link.to)
            ranks->edges[ranks->cursor[link.from]++] = index;
    }
}
static void visit(struct diagram_ranks *_Nonnull ranks, const struct diagram_model *_Nonnull model,
                  size_t *_Nonnull depth)
{
    size_t node = ranks->stack[*depth - 1];
    if (ranks->cursor[node] == ranks->starts[node + 1])
    {
        ranks->visits[node] = DIAGRAM_VISITED;
        --*depth;
        return;
    }
    size_t edge = ranks->edges[ranks->cursor[node]++];
    size_t target = diagram_model_link_at(model, edge).to;
    switch (ranks->visits[target])
    {
    case DIAGRAM_UNSEEN:
        ranks->visits[target] = DIAGRAM_VISITING;
        ranks->stack[(*depth)++] = target;
        break;
    case DIAGRAM_VISITING:
        ranks->reversed[edge] = true;
        break;
    case DIAGRAM_VISITED:
        break;
    }
}
static void orient_cycles(struct diagram_ranks *_Nonnull ranks,
                          const struct diagram_model *_Nonnull model)
{
    size_t count = diagram_model_node_count(model);
    memcpy(ranks->cursor, ranks->starts, count * sizeof ranks->cursor[0]);
    for (size_t root = 0; root < count; ++root)
    {
        if (ranks->visits[root] != DIAGRAM_UNSEEN)
            continue;
        size_t depth = 1;
        ranks->stack[0] = root;
        ranks->visits[root] = DIAGRAM_VISITING;
        while (depth != 0)
            visit(ranks, model, &depth);
    }
}
static void advance_rank(struct diagram_ranks *_Nonnull ranks,
                         const struct diagram_model *_Nonnull model, size_t node,
                         size_t *_Nonnull queued)
{
    for (size_t at = ranks->starts[node]; at < ranks->starts[node + 1]; ++at)
    {
        struct diagram_link link = oriented(ranks, model, ranks->edges[at]);
        size_t candidate = ranks->values[node] + link.length;
        if (candidate > ranks->values[link.to])
            ranks->values[link.to] = candidate;
        if (--ranks->incoming[link.to] == 0)
            ranks->stack[(*queued)++] = link.to;
    }
}
static bool assign_ranks(struct diagram_ranks *_Nonnull ranks,
                         const struct diagram_model *_Nonnull model)
{
    size_t count = diagram_model_node_count(model);
    size_t queued = 0;
    for (size_t index = 0; index < count; ++index)
        if (ranks->incoming[index] == 0)
            ranks->stack[queued++] = index;
    for (size_t index = 0; index < queued; ++index)
        advance_rank(ranks, model, ranks->stack[index], &queued);
    for (size_t index = 0; index < count; ++index)
        if (ranks->values[index] > ranks->maximum)
            ranks->maximum = ranks->values[index];
    return queued == count;
}
enum diagram_outcome diagram_ranks_create(const struct diagram_model *_Nonnull model,
                                          struct diagram_ranks *_Nullable *_Nonnull out)
{
    if (diagram_model_kind(model) != DIAGRAM_FLOWCHART || diagram_model_node_count(model) == 0)
        return DIAGRAM_INVALID;
    struct diagram_ranks *_Nullable ranks = calloc(1, sizeof *ranks);
    if (ranks == nullptr)
        return DIAGRAM_OUT_OF_MEMORY;
    adjacency(ranks, model);
    orient_cycles(ranks, model);
    adjacency(ranks, model);
    if (!assign_ranks(ranks, model))
    {
        diagram_ranks_destroy(ranks);
        return DIAGRAM_INVALID;
    }
    *out = ranks;
    return DIAGRAM_ACCEPTED;
}
size_t diagram_ranks_at(const struct diagram_ranks *_Nonnull ranks, size_t node)
{
    return ranks->values[node];
}
size_t diagram_ranks_maximum(const struct diagram_ranks *_Nonnull ranks)
{
    return ranks->maximum;
}
bool diagram_ranks_reversed(const struct diagram_ranks *_Nonnull ranks, size_t edge)
{
    return ranks->reversed[edge];
}
void diagram_ranks_destroy(struct diagram_ranks *_Nullable ranks)
{
    free(ranks);
}
