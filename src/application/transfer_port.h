#ifndef NENEFOLIO_TRANSFER_PORT_H
#define NENEFOLIO_TRANSFER_PORT_H
#include "persistence_outcome.h"
struct storage_path;
struct note_text;
struct note_archive;
struct transfer_port
{
    enum persistence_outcome (*_Nonnull read)(const struct storage_path *_Nonnull path,
                                              struct note_text *_Nullable *_Nonnull out);
    enum persistence_outcome (*_Nonnull write_note)(const struct storage_path *_Nonnull path,
                                                    const struct note_text *_Nonnull body);
    enum persistence_outcome (*_Nonnull write_archive)(const struct storage_path *_Nonnull path,
                                                       const struct note_archive *_Nonnull archive);
};
#endif
