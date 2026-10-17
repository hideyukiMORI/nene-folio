/* OSの表示方針。保存するtheme_choiceとは独立する（ADR0057）。 */
#ifndef NENEFOLIO_FOLIO_CONTRAST_H
#define NENEFOLIO_FOLIO_CONTRAST_H

enum folio_contrast : unsigned char
{
    FOLIO_CONTRAST_STANDARD,
    FOLIO_CONTRAST_HIGH
};

#endif
