#include "allocation_probe.h"
#include "startup_settings.h"
#include "storage_path.h"
#include "storage_port.h"
#include "storage_state.h"
#include "unit_tests.h"
#include <string.h>

struct storage_adapter
{
    const struct storage_path *_Nullable saved;
    enum storage_outcome read_result;
    enum storage_outcome check_result;
    enum storage_outcome store_result;
    size_t checks;
    size_t stores;
    enum storage_check checked_kind;
    bool stored_default;
};

static enum storage_outcome read_settings(struct storage_adapter *_Nonnull fake,
                                          struct startup_settings *_Nullable *_Nonnull out)
{
    if (fake->read_result != STORAGE_READY)
        return fake->read_result;
    return startup_settings_create(fake->saved, out) == STARTUP_SETTINGS_READY
               ? STORAGE_READY
               : STORAGE_OUT_OF_MEMORY;
}

static enum storage_outcome check_directory(struct storage_adapter *_Nonnull fake,
                                            const struct storage_path *_Nonnull path,
                                            enum storage_check kind)
{
    (void)path;
    fake->checks += 1;
    fake->checked_kind = kind;
    return fake->check_result;
}

static enum storage_outcome store_settings(struct storage_adapter *_Nonnull fake,
                                           const struct startup_settings *_Nullable expected,
                                           const struct startup_settings *_Nonnull replacement)
{
    (void)expected;
    fake->stores += 1;
    fake->stored_default = startup_settings_directory(replacement) == nullptr;
    return fake->store_result;
}

static struct storage_port make_port(struct storage_adapter *_Nonnull fake,
                                     const struct storage_path *_Nonnull path,
                                     enum storage_distribution distribution)
{
    return (struct storage_port){.adapter = fake,
                                 .default_directory = path,
                                 .distribution = distribution,
                                 .read = read_settings,
                                 .check = check_directory,
                                 .store = store_settings};
}

static struct storage_path *_Nonnull path_for(const char *_Nonnull text)
{
    struct storage_path *path = nullptr;
    require(storage_path_create(text, strlen(text), &path) == STORAGE_PATH_READY, "test root");
    return path;
}

static void verify_initial(void)
{
    struct storage_path *root = path_for("D:/app/data");
    struct storage_path *saved = path_for("D:/notes");
    struct storage_adapter fake = {.read_result = STORAGE_ABSENT};
    struct storage_port port = make_port(&fake, root, STORAGE_PORTABLE);
    struct storage_state *state = nullptr;
    require(storage_state_create(&port, &state) == STORAGE_READY, "legacy portable starts");
    require(storage_state_phase(state) == STORAGE_ACTIVE && fake.checks == 0 && fake.stores == 0 &&
                storage_path_equal(root, storage_state_directory(state)),
            "legacy portable keeps root without requiring new write permission");
    storage_state_destroy(state);
    state = nullptr;
    port.distribution = STORAGE_INSTALLED;
    require(storage_state_create(&port, &state) == STORAGE_READY &&
                storage_state_phase(state) == STORAGE_FIRST_USE && fake.stores == 0,
            "installed first use waits without persisting anything");
    storage_state_destroy(state);
    state = nullptr;
    fake.read_result = STORAGE_READY;
    fake.saved = saved;
    require(storage_state_create(&port, &state) == STORAGE_READY &&
                storage_state_phase(state) == STORAGE_ACTIVE && fake.checks == 1 &&
                fake.checked_kind == STORAGE_CHECK_EXISTING &&
                storage_path_equal(saved, storage_state_directory(state)),
            "explicit saved root must be checked before use");
    storage_state_destroy(state);
    state = nullptr;
    fake.check_result = STORAGE_DIRECTORY_MISSING;
    require(storage_state_create(&port, &state) == STORAGE_READY &&
                storage_state_phase(state) == STORAGE_RECOVERY &&
                storage_state_notice(state) == STORAGE_DIRECTORY_MISSING &&
                storage_path_equal(saved, storage_state_directory(state)),
            "missing root enters recovery with original path, never fallback");
    storage_state_destroy(state);
    state = nullptr;
    fake.check_result = STORAGE_OUT_OF_MEMORY;
    require(storage_state_create(&port, &state) == STORAGE_OUT_OF_MEMORY && state == nullptr,
            "failed initial check does not expose partial state");
    fake.read_result = STORAGE_SETTINGS_UNSUPPORTED;
    require(storage_state_create(&port, &state) == STORAGE_SETTINGS_UNSUPPORTED && state == nullptr,
            "unsupported startup settings stop before any defaults");
    storage_path_destroy(saved);
    storage_path_destroy(root);
}

static void verify_runtime(void)
{
    struct storage_path *root = path_for("D:/app/data");
    struct storage_adapter fake = {.read_result = STORAGE_ABSENT};
    struct storage_port port = make_port(&fake, root, STORAGE_PORTABLE);
    struct storage_state *state = nullptr;
    require(storage_state_create(&port, &state) == STORAGE_READY, "runtime state");
    require(storage_state_choose(state, "relative", 8) == STORAGE_INVALID_PATH && fake.stores == 0,
            "invalid user text has no I/O");
    fake.check_result = STORAGE_DIRECTORY_IN_USE;
    require(storage_state_choose(state, "D:/notes", 8) == STORAGE_DIRECTORY_IN_USE &&
                fake.stores == 0 && !storage_state_pending(state),
            "locked target leaves state alone");
    fake.check_result = STORAGE_READY;
    fake.store_result = STORAGE_SETTINGS_STORE_FAILED;
    require(storage_state_choose(state, "D:/notes", 8) == STORAGE_SETTINGS_STORE_FAILED &&
                !storage_state_pending(state),
            "failed config save preserves next root");
    fake.store_result = STORAGE_SETTINGS_CHANGED;
    require(storage_state_choose(state, "D:/notes", 8) == STORAGE_SETTINGS_CHANGED &&
                !storage_state_pending(state),
            "conflicting config is not adopted");
    fake.store_result = STORAGE_READY;
    require(storage_state_choose(state, "D:/notes", 8) == STORAGE_READY &&
                storage_state_pending(state) && !fake.stored_default &&
                storage_path_equal(root, storage_state_directory(state)) &&
                same_text(storage_path_text(storage_state_next_directory(state)), "D:\\notes"),
            "successful change only changes next root");
    size_t stores = fake.stores;
    size_t checks = fake.checks;
    require(storage_state_choose(state, "D:/notes/", 9) == STORAGE_READY && fake.stores == stores &&
                fake.checks == checks,
            "same normalized choice is read-only");
    require(storage_state_choose(state, "D:/app/data", 11) == STORAGE_READY &&
                !storage_state_pending(state) && fake.stored_default && fake.checks == checks,
            "return to currently held root needs no competing lock and stores relocatable default");
    require(storage_path_equal(root, storage_state_default_directory(state)),
            "default never changes");
    storage_state_destroy(state);
    storage_path_destroy(root);
}

static void verify_first_choice(void)
{
    struct storage_path *root = path_for("D:/documents/NeNeFolio");
    struct storage_adapter fake = {.read_result = STORAGE_ABSENT,
                                   .store_result = STORAGE_SETTINGS_STORE_FAILED};
    struct storage_port port = make_port(&fake, root, STORAGE_INSTALLED);
    struct storage_state *state = nullptr;
    require(storage_state_create(&port, &state) == STORAGE_READY, "first use state");
    require(storage_state_choose(state, "D:/notes", 8) == STORAGE_SETTINGS_STORE_FAILED &&
                storage_state_phase(state) == STORAGE_FIRST_USE &&
                storage_path_equal(root, storage_state_directory(state)),
            "failed first confirmation preserves initial phase and root");
    fake.store_result = STORAGE_READY;
    require(storage_state_choose(state, "D:/notes", 8) == STORAGE_READY &&
                storage_state_phase(state) == STORAGE_ACTIVE &&
                fake.checked_kind == STORAGE_CHECK_CHOICE && !storage_state_pending(state) &&
                same_text(storage_path_text(storage_state_directory(state)), "D:\\notes"),
            "first confirmation adopts checked root for this launch");
    storage_state_destroy(state);
    storage_path_destroy(root);
}

static void verify_choose_allocation(enum storage_distribution distribution)
{
    struct storage_path *root = path_for("D:/app/data");
    struct storage_adapter fake = {.read_result = STORAGE_ABSENT};
    struct storage_port port = make_port(&fake, root, distribution);
    struct storage_state *state = nullptr;
    require(storage_state_create(&port, &state) == STORAGE_READY, "choice allocation baseline");
    allocation_probe_fail_at(0);
    require(storage_state_choose(state, "D:/notes", 8) == STORAGE_READY, "baseline choice");
    size_t calls = allocation_probe_calls();
    storage_state_destroy(state);
    for (size_t nth = 1; nth <= calls; ++nth)
    {
        allocation_probe_fail_at(0);
        state = nullptr;
        fake.stores = 0;
        require(storage_state_create(&port, &state) == STORAGE_READY, "fresh choice state");
        const struct storage_path *before = storage_state_next_directory(state);
        enum storage_phase phase = storage_state_phase(state);
        allocation_probe_fail_at(nth);
        enum storage_outcome result = storage_state_choose(state, "D:/notes", 8);
        allocation_probe_fail_at(0);
        require(result == STORAGE_OUT_OF_MEMORY && fake.stores == 0 &&
                    storage_state_next_directory(state) == before &&
                    storage_state_phase(state) == phase,
                "every choice allocation failure precedes persistence and preserves state");
        storage_state_destroy(state);
    }
    storage_path_destroy(root);
}

static void verify_create_allocation(void)
{
    struct storage_path *root = path_for("D:/app/data");
    struct storage_adapter fake = {.read_result = STORAGE_READY, .saved = root};
    struct storage_port port = make_port(&fake, root, STORAGE_PORTABLE);
    struct storage_state *state = nullptr;
    allocation_probe_fail_at(0);
    require(storage_state_create(&port, &state) == STORAGE_READY, "create allocation baseline");
    size_t calls = allocation_probe_calls();
    storage_state_destroy(state);
    for (size_t nth = 1; nth <= calls; ++nth)
    {
        state = nullptr;
        allocation_probe_fail_at(nth);
        enum storage_outcome result = storage_state_create(&port, &state);
        allocation_probe_fail_at(0);
        require(result == STORAGE_OUT_OF_MEMORY && state == nullptr,
                "every state allocation failure leaves no partially constructed owner");
    }
    storage_path_destroy(root);
}

void run_storage_state_tests(void)
{
    verify_initial();
    verify_runtime();
    verify_first_choice();
    verify_choose_allocation(STORAGE_PORTABLE);
    verify_choose_allocation(STORAGE_INSTALLED);
    verify_create_allocation();
    for (size_t language = 0; language < folio_language_count; ++language)
        require(strlen(storage_state_failure_line(STORAGE_SETTINGS_CHANGED,
                                                  (enum folio_language)language)) > 0,
                "storage failure is localized");
}
