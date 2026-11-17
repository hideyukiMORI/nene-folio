#include "test_window_file.h"
#include "window_placement.h"

static struct window_placement held;
static bool attached;

static void read_window(struct storage_adapter *_Nonnull adapter,
                        struct window_placement *_Nonnull out)
{
    (void)adapter;
    *out = held;
}

static enum storage_outcome store_window(struct storage_adapter *_Nonnull adapter,
                                         const struct window_placement *_Nonnull window)
{
    (void)adapter;
    held = *window;
    return STORAGE_READY;
}

void test_window_file_attach(struct storage_port *_Nonnull port)
{
    if (!attached)
    {
        held = window_placement_default();
        held.restore = false;
        attached = true;
    }
    port->read_window = read_window;
    port->store_window = store_window;
}
