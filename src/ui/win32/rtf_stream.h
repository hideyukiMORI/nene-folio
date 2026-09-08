/* EM_STREAMIN へ渡す読み取り位置。全メンバーが独立に妥当な完全型（C-003）。 */
#ifndef NENEFOLIO_RTF_STREAM_H
#define NENEFOLIO_RTF_STREAM_H

#include <stddef.h>

struct rtf_stream
{
    const char *_Nonnull bytes;
    size_t remaining;
};

#endif
