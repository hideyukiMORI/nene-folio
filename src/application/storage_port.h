#ifndef NENEFOLIO_STORAGE_PORT_H
#define NENEFOLIO_STORAGE_PORT_H
#include "storage_check.h"
#include "storage_distribution.h"
#include "storage_outcome.h"
struct storage_adapter;
struct storage_path;
struct startup_settings;
/* OS由来の不変な配置とtyped I/O。adapterとdefaultはstateより長く生きる。 */
struct storage_port
{
    struct storage_adapter *_Nonnull adapter;
    enum storage_distribution distribution;
    const struct storage_path *_Nonnull default_directory;
    enum storage_outcome (*_Nonnull read)(struct storage_adapter *_Nonnull adapter,
                                          struct startup_settings *_Nullable *_Nonnull out);
    enum storage_outcome (*_Nonnull check)(struct storage_adapter *_Nonnull adapter,
                                           const struct storage_path *_Nonnull path,
                                           enum storage_check kind);
    /* expectedの不在は、読込時に起動設定fileが無かったこと。競合時は上書きしない。 */
    enum storage_outcome (*_Nonnull store)(struct storage_adapter *_Nonnull adapter,
                                           const struct startup_settings *_Nullable expected,
                                           const struct startup_settings *_Nonnull replacement);
};
#endif
