#ifndef NENEFOLIO_IMAGE_WORK_H
#define NENEFOLIO_IMAGE_WORK_H
#include "image_phase.h"
#include <stdint.h>
struct image_adapter;
struct image_work
{
    struct image_adapter *_Nonnull adapter;
    uint64_t ticket;
    bool (*_Nonnull cancelled)(const struct image_work *_Nonnull work);
    void (*_Nonnull progress)(const struct image_work *_Nonnull work, enum image_phase phase);
};
#endif
