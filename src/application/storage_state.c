#include "storage_state.h"
#include "startup_settings.h"
#include "storage_path.h"
#include "storage_port.h"
#include "ui_text.h"
#include <stdlib.h>

struct storage_state
{
    struct storage_port port;
    struct storage_path *_Nonnull current;
    struct startup_settings *_Nullable next;
    enum storage_phase phase;
    enum storage_outcome notice;
};

static const struct storage_path *_Nonnull effective(
    const struct storage_port *_Nonnull port, const struct startup_settings *_Nullable settings)
{
    const struct storage_path *_Nullable custom =
        settings == nullptr ? nullptr : startup_settings_directory(settings);
    return custom == nullptr ? port->default_directory : custom;
}

static enum storage_phase absent_phase(enum storage_distribution distribution)
{
    switch (distribution)
    {
    case STORAGE_PORTABLE:
        return STORAGE_ACTIVE;
    case STORAGE_INSTALLED:
        return STORAGE_FIRST_USE;
    }
    return STORAGE_FIRST_USE;
}

static enum storage_outcome initial_check(struct storage_state *_Nonnull state)
{
    if (state->next == nullptr)
        return STORAGE_READY;
    enum storage_outcome checked =
        state->port.check(state->port.adapter, state->current, STORAGE_CHECK_EXISTING);
    if (checked == STORAGE_OUT_OF_MEMORY)
        return checked;
    if (checked != STORAGE_READY)
    {
        state->notice = checked;
        state->phase = STORAGE_RECOVERY;
    }
    return STORAGE_READY;
}

/* factoryの未公開localだけを同じモジュールで片付ける。 */
static void destroy_prepared(struct storage_path *_Nullable path,
                             struct startup_settings *_Nullable settings,
                             struct storage_state *_Nullable state)
{
    storage_path_destroy(path);
    startup_settings_destroy(settings);
    free(state);
}

enum storage_outcome storage_state_create(const struct storage_port *_Nonnull port,
                                          struct storage_state *_Nullable *_Nonnull out)
{
    struct startup_settings *_Nullable settings = nullptr;
    enum storage_outcome loaded = port->read(port->adapter, &settings);
    if (loaded != STORAGE_READY && loaded != STORAGE_ABSENT)
        return loaded;
    struct storage_path *_Nullable path = nullptr;
    enum storage_path_outcome copied = storage_path_copy(effective(port, settings), &path);
    struct storage_state *_Nullable state = malloc(sizeof *state);
    if (copied != STORAGE_PATH_READY || state == nullptr)
    {
        destroy_prepared(path, settings, state);
        return STORAGE_OUT_OF_MEMORY;
    }
    *state = (struct storage_state){
        .port = *port,
        .current = path,
        .next = settings,
        .phase = loaded == STORAGE_ABSENT ? absent_phase(port->distribution) : STORAGE_ACTIVE,
        .notice = STORAGE_READY};
    enum storage_outcome checked = initial_check(state);
    if (checked != STORAGE_READY)
    {
        storage_state_destroy(state);
        return checked;
    }
    *out = state;
    return STORAGE_READY;
}

static enum storage_outcome choose_prepared(struct storage_state *_Nonnull state,
                                            const struct storage_path *_Nonnull path,
                                            struct startup_settings *_Nonnull settings)
{
    struct storage_path *_Nullable replacement = nullptr;
    if (state->phase != STORAGE_ACTIVE &&
        storage_path_copy(path, &replacement) != STORAGE_PATH_READY)
        return STORAGE_OUT_OF_MEMORY;
    enum storage_outcome result = STORAGE_READY;
    if (state->phase != STORAGE_ACTIVE || !storage_path_equal(path, state->current))
        result = state->port.check(state->port.adapter, path, STORAGE_CHECK_CHOICE);
    if (result == STORAGE_READY)
        result = state->port.store(state->port.adapter, state->next, settings);
    if (result == STORAGE_READY && replacement != nullptr)
    {
        storage_path_destroy(state->current);
        state->current = replacement;
        replacement = nullptr;
    }
    storage_path_destroy(replacement);
    return result;
}

enum storage_outcome storage_state_choose(struct storage_state *_Nonnull state,
                                          const char *_Nonnull text, size_t length)
{
    struct storage_path *_Nullable path = nullptr;
    enum storage_path_outcome parsed = storage_path_create(text, length, &path);
    if (parsed != STORAGE_PATH_READY)
        return parsed == STORAGE_PATH_INVALID ? STORAGE_INVALID_PATH : STORAGE_OUT_OF_MEMORY;
    if (state->phase == STORAGE_ACTIVE &&
        storage_path_equal(path, storage_state_next_directory(state)))
    {
        storage_path_destroy(path);
        return STORAGE_READY;
    }
    struct startup_settings *_Nullable settings = nullptr;
    const struct storage_path *_Nullable custom =
        storage_path_equal(path, state->port.default_directory) ? nullptr : path;
    enum storage_outcome result = STORAGE_OUT_OF_MEMORY;
    if (startup_settings_create(custom, &settings) == STARTUP_SETTINGS_READY)
        result = choose_prepared(state, path, settings);
    storage_path_destroy(path);
    if (result == STORAGE_READY)
    {
        startup_settings_destroy(state->next);
        state->next = settings;
        state->phase = STORAGE_ACTIVE;
        state->notice = STORAGE_READY;
        return result;
    }
    startup_settings_destroy(settings);
    return result;
}

enum storage_phase storage_state_phase(const struct storage_state *_Nonnull state)
{
    return state->phase;
}

enum storage_outcome storage_state_notice(const struct storage_state *_Nonnull state)
{
    return state->notice;
}

const struct storage_path *_Nonnull storage_state_directory(
    const struct storage_state *_Nonnull state)
{
    return state->current;
}

const struct storage_path *_Nonnull storage_state_next_directory(
    const struct storage_state *_Nonnull state)
{
    return effective(&state->port, state->next);
}

const struct storage_path *_Nonnull storage_state_default_directory(
    const struct storage_state *_Nonnull state)
{
    return state->port.default_directory;
}

bool storage_state_pending(const struct storage_state *_Nonnull state)
{
    return !storage_path_equal(state->current, storage_state_next_directory(state));
}

/* closed enumを表へ移す分の網羅性はCNF-009で固定する。 */
static const enum ui_text failure_lines[] = {
    [STORAGE_READY] = UI_TEXT_EMPTY,
    [STORAGE_ABSENT] = UI_TEXT_EMPTY,
    [STORAGE_INVALID_PATH] = UI_TEXT_STORAGE_INVALID_PATH,
    [STORAGE_LOCATION_UNAVAILABLE] = UI_TEXT_STORAGE_LOCATION_UNAVAILABLE,
    [STORAGE_DIRECTORY_MISSING] = UI_TEXT_STORAGE_DIRECTORY_MISSING,
    [STORAGE_DIRECTORY_UNREADABLE] = UI_TEXT_STORAGE_DIRECTORY_UNREADABLE,
    [STORAGE_DIRECTORY_UNWRITABLE] = UI_TEXT_STORAGE_DIRECTORY_UNWRITABLE,
    [STORAGE_DIRECTORY_IN_USE] = UI_TEXT_STORAGE_DIRECTORY_IN_USE,
    [STORAGE_SETTINGS_UNREADABLE] = UI_TEXT_STORAGE_SETTINGS_UNREADABLE,
    [STORAGE_SETTINGS_MALFORMED] = UI_TEXT_STORAGE_SETTINGS_MALFORMED,
    [STORAGE_SETTINGS_UNSUPPORTED] = UI_TEXT_STORAGE_SETTINGS_UNSUPPORTED,
    [STORAGE_SETTINGS_STORE_FAILED] = UI_TEXT_STORAGE_SETTINGS_STORE_FAILED,
    [STORAGE_SETTINGS_CHANGED] = UI_TEXT_STORAGE_SETTINGS_CHANGED,
    [STORAGE_OUT_OF_MEMORY] = UI_TEXT_APP_OUT_OF_MEMORY,
};

const char *_Nonnull storage_state_failure_line(enum storage_outcome outcome,
                                                enum folio_language language)
{
    return ui_text_line(failure_lines[outcome], language);
}

void storage_state_destroy(struct storage_state *_Nullable state)
{
    if (state != nullptr)
    {
        storage_path_destroy(state->current);
        startup_settings_destroy(state->next);
    }
    free(state);
}
