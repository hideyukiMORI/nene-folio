/* One synchronous transition may replace an untitled document (ADR0050). */
#ifndef NENEFOLIO_FOLIO_UNTITLED_POLICY_H
#define NENEFOLIO_FOLIO_UNTITLED_POLICY_H
enum folio_untitled_policy : unsigned char
{
    FOLIO_UNTITLED_PROTECT,
    FOLIO_UNTITLED_DISCARD
};
#endif
