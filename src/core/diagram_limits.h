#ifndef NENEFOLIO_DIAGRAM_LIMITS_H
#define NENEFOLIO_DIAGRAM_LIMITS_H
#include <stddef.h>
constexpr size_t diagram_source_limit = 256 * 1024;
constexpr size_t diagram_label_limit = 2048;
constexpr size_t diagram_node_limit = 256;
constexpr size_t diagram_link_limit = 1024;
constexpr size_t diagram_event_limit = 2048;
constexpr size_t diagram_group_limit = 64;
constexpr size_t diagram_text_limit = 4096;
constexpr size_t diagram_primitive_limit = 8192;
constexpr size_t diagram_depth_limit = 16;
constexpr int diagram_coordinate_limit = 32768;
constexpr size_t diagram_none = (size_t)-1;
#endif
