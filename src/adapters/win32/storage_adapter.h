#ifndef NENEFOLIO_STORAGE_ADAPTER_H
#define NENEFOLIO_STORAGE_ADAPTER_H
#include "storage_port.h"
struct storage_adapter;
[[nodiscard]] enum storage_outcome
storage_adapter_create(struct storage_adapter *_Nullable *_Nonnull out);
[[nodiscard]] struct storage_port storage_adapter_port(struct storage_adapter *_Nonnull adapter);
void storage_adapter_destroy(struct storage_adapter *_Nullable adapter);
#endif
