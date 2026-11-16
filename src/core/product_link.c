#include "product_link.h"

#include <string.h>

const char *_Nonnull product_link_uri(enum product_link link)
{
    switch (link)
    {
    case PRODUCT_LINK_REPOSITORY:
        return "https://github.com/hideyukiMORI/nene-folio";
    case PRODUCT_LINK_RELEASES:
        return "https://github.com/hideyukiMORI/nene-folio/releases";
    case PRODUCT_LINK_STORE:
        return "https://apps.microsoft.com/detail/9PCHS225SWBX";
    case PRODUCT_LINK_ISSUES:
        return "https://github.com/hideyukiMORI/nene-folio/issues";
    }
    return "";
}

size_t product_link_uri_length(enum product_link link)
{
    return strlen(product_link_uri(link));
}
