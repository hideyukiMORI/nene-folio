/* A theme identifier either names one selection or is unknown (ADR0049). */
#ifndef NENEFOLIO_FOLIO_THEME_LOOKUP_OUTCOME_H
#define NENEFOLIO_FOLIO_THEME_LOOKUP_OUTCOME_H

enum folio_theme_lookup_outcome : unsigned char
{
    FOLIO_THEME_LOOKUP_FOUND,
    FOLIO_THEME_LOOKUP_UNKNOWN
};

#endif
