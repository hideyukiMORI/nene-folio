/* C23 が本当に有効かを、C23 にしか無い構文で確かめる（QLT-011）。 */
#include <stddef.h>

enum smoke_mode : unsigned char
{
    SMOKE_VIEW,
    SMOKE_EDIT
};

constexpr int smoke_limit = 3;

[[nodiscard]] static int smoke_select(enum smoke_mode mode);

static int smoke_select(enum smoke_mode mode)
{
    switch (mode)
    {
    case SMOKE_VIEW:
        return 1;
    case SMOKE_EDIT:
        return 2;
    }
    return 0;
}

int main(void)
{
    static_assert(smoke_limit == 3, "constexpr must be usable in constant expressions");
    int *absent = nullptr;
    typeof(smoke_limit) total = smoke_select(SMOKE_VIEW) + smoke_select(SMOKE_EDIT);
    bool ok = absent == nullptr && total == smoke_limit;
    return ok ? 0 : 1;
}
