#ifndef NENEFOLIO_WEB_LINK_PORT_H
#define NENEFOLIO_WEB_LINK_PORT_H
#include "web_link_outcome.h"
struct web_uri;
struct web_link_port
{
    enum web_link_outcome (*_Nonnull open)(const struct web_uri *_Nonnull uri);
};
#endif
