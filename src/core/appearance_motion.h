/* OSが観測できたclient-area animationの三値。未取得を無効という観測に置換しない。 */
#ifndef NENEFOLIO_APPEARANCE_MOTION_H
#define NENEFOLIO_APPEARANCE_MOTION_H

enum appearance_motion : unsigned char
{
    APPEARANCE_MOTION_UNKNOWN,
    APPEARANCE_MOTION_DISABLED,
    APPEARANCE_MOTION_ALLOWED
};

#endif
