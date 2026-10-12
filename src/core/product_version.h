/* 製品版の唯一の参照。値はCMakeのproject VERSIONから渡す（FR-014 / ADR0045）。 */
#ifndef NENEFOLIO_PRODUCT_VERSION_H
#define NENEFOLIO_PRODUCT_VERSION_H

[[nodiscard]] const char *_Nonnull product_version_text(void);

#endif
