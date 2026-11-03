#ifndef NENEFOLIO_DIAGRAM_EVENT_H
#define NENEFOLIO_DIAGRAM_EVENT_H
#include "diagram_event_kind.h"
#include "diagram_frame_kind.h"
#include "diagram_link.h"
#include "diagram_note_side.h"
struct diagram_event
{
    enum diagram_event_kind kind;
    enum diagram_frame_kind frame;
    enum diagram_note_side side;
    struct diagram_link link;
    size_t number;
};
#endif
