#ifndef NENEFOLIO_TAG_SURFACE_FOCUS_H
#define NENEFOLIO_TAG_SURFACE_FOCUS_H
/* The custom layer focus; the native text field remains owned by GetFocus(). */
enum tag_surface_focus : unsigned char
{
    TAG_SURFACE_ROWS,
    TAG_SURFACE_ADD,
    TAG_SURFACE_APPLY,
    TAG_SURFACE_CANCEL,
    TAG_SURFACE_ALL,
    TAG_SURFACE_ANY,
    TAG_SURFACE_CLEAR,
    TAG_SURFACE_CLOSE,
    TAG_SURFACE_PRIMARY,
    TAG_SURFACE_SECONDARY
};
#endif
