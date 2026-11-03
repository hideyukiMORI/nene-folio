#ifndef NENEFOLIO_MERMAID_SHAPE_H
#define NENEFOLIO_MERMAID_SHAPE_H
#include "diagram_shape.h"
struct mermaid_shape
{
    const char *_Nonnull opening;
    const char *_Nonnull closing;
    enum diagram_shape shape;
};
#endif
