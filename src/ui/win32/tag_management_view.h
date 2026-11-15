#ifndef NENEFOLIO_TAG_MANAGEMENT_VIEW_H
#define NENEFOLIO_TAG_MANAGEMENT_VIEW_H
/* Native input steps precede the application-owned immutable preview and apply phases. */
enum tag_management_view : unsigned char
{
    TAG_MANAGEMENT_CHOOSE,
    TAG_MANAGEMENT_NAME,
    TAG_MANAGEMENT_PLAN
};
#endif
