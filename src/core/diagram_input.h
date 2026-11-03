#ifndef NENEFOLIO_DIAGRAM_INPUT_H
#define NENEFOLIO_DIAGRAM_INPUT_H
#include "diagram_kind.h"
#include "diagram_text.h"
struct diagram_input
{
    struct diagram_text text;
    enum diagram_kind kind;
};
#endif
