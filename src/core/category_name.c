#include "category_name.h"
#include "ascii_fold.h"
#include "name_list.h"
#include <stdlib.h>
#include <string.h>

struct category_name
{
    struct name_list *_Nullable value;
};

/* data/ の直下でアプリが使うファイルの名前。小文字で書き、比べる側だけを畳む。 */
static const char *_Nonnull const reserved_names[] = {"categories.json", "settings.json"};

static bool folded_equal(const char *_Nonnull text, size_t length, const char *_Nonnull word)
{
    if (strlen(word) != length)
    {
        return false;
    }
    for (size_t index = 0; index < length; ++index)
    {
        if (ascii_fold_byte(text[index]) != word[index])
        {
            return false;
        }
    }
    return true;
}

static bool reserved(const char *_Nonnull text, size_t length)
{
    for (size_t index = 0; index < sizeof reserved_names / sizeof reserved_names[0]; ++index)
    {
        if (folded_equal(text, length, reserved_names[index]))
        {
            return true;
        }
    }
    return false;
}

enum category_name_outcome category_name_create(const char *_Nonnull text, size_t length,
                                                struct category_name *_Nullable *_Nonnull out)
{
    if (length == 0 || text[0] == '.' || reserved(text, length))
    {
        return CATEGORY_NAME_INVALID;
    }
    struct category_name *_Nullable name = calloc(1, sizeof *name);
    if (name == nullptr)
    {
        return CATEGORY_NAME_OUT_OF_MEMORY;
    }
    /* 残りの規則（UTF-8・禁止文字・末尾・予約名・長さ）は name_list が検証する。 */
    enum name_list_outcome accepted = name_list_create(&name->value);
    if (accepted == NAME_LIST_ACCEPTED)
    {
        accepted = name_list_append(name->value, text, length);
    }
    if (accepted != NAME_LIST_ACCEPTED)
    {
        category_name_destroy(name);
        return accepted == NAME_LIST_OUT_OF_MEMORY ? CATEGORY_NAME_OUT_OF_MEMORY
                                                   : CATEGORY_NAME_INVALID;
    }
    *out = name;
    return CATEGORY_NAME_ACCEPTED;
}

const char *_Nonnull category_name_text(const struct category_name *_Nonnull name)
{
    return name_list_at(name->value, 0);
}

void category_name_destroy(struct category_name *_Nullable name)
{
    if (name == nullptr)
    {
        return;
    }
    name_list_destroy(name->value);
    free(name);
}
