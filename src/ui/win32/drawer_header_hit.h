/* 固定帯の当たり。タグごとの札は、釦の種類に番号を足して 1 つを指す（#1247）。
 * slot は、button が DRAWER_HEADER_TAG のときだけ読む（ほかは 0 を入れる）。 */
#ifndef NENEFOLIO_DRAWER_HEADER_HIT_H
#define NENEFOLIO_DRAWER_HEADER_HIT_H

#include "drawer_header_button.h"

#include <stddef.h>

struct drawer_header_hit
{
    enum drawer_header_button button;
    size_t slot; /* DRAWER_HEADER_TAG のときの札の番号。ほかは 0 */
};

#endif
