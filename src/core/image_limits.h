#ifndef NENEFOLIO_IMAGE_LIMITS_H
#define NENEFOLIO_IMAGE_LIMITS_H
#include <stddef.h>
#include <stdint.h>
constexpr size_t image_input_byte_limit = 16 * 1024 * 1024;
constexpr uint64_t image_input_pixel_limit = UINT64_C(64) * 1024 * 1024;
#endif
