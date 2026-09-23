#include "font_bundle_verdict.h"

enum font_bundle_outcome font_bundle_verdict_of(size_t found, size_t registered)
{
    if (found == 0)
    {
        return FONT_BUNDLE_MISSING;
    }
    if (registered < found)
    {
        return FONT_BUNDLE_PARTIAL;
    }
    return FONT_BUNDLE_READY;
}
