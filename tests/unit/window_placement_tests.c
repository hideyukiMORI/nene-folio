/* #891 / ADR0108: the remembered window place — its file format and the check that decides
 * whether a recorded place may be used. */
#include "json_writer.h"
#include "unit_tests.h"
#include "window_fit.h"
#include "window_placement.h"
#include <string.h>

static const char recorded[] =
    "{\"version\": 1, \"restore\": true, \"placed\": true, \"x\": 999900, "
    "\"y\": 1000050, \"width\": 1200, \"height\": 800, \"dpi\": 144, "
    "\"maximized\": true}";

/* Writes the value and reads the document back. */
static struct window_placement written(struct window_placement value)
{
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "writer");
    window_placement_write(&value, writer);
    require(json_writer_finish(writer) == JSON_WRITER_ACCEPTED, "a complete document");
    struct window_placement read = {0};
    require(window_placement_parse(json_writer_text(writer), json_writer_length(writer), &read) ==
                WINDOW_PLACEMENT_READY,
            "what is written can be read");
    json_writer_destroy(writer);
    return read;
}

static void verify_format(void)
{
    struct window_placement none = window_placement_default();
    require(none.restore && !none.placed && none.dpi == 96,
            "a missing file carries the place over with nothing recorded");
    struct window_placement read = {0};
    require(window_placement_parse(recorded, strlen(recorded), &read) == WINDOW_PLACEMENT_READY,
            "the recorded place is read");
    require(read.restore && read.placed && read.x == -100 && read.y == 50 && read.width == 1200 &&
                read.height == 800 && read.dpi == 144 && read.maximized,
            "every value, with coordinates left of the primary screen");
    struct window_placement again = written(read);
    require(again.restore && again.placed && again.x == -100 && again.y == 50 &&
                again.width == 1200 && again.height == 800 && again.dpi == 144 && again.maximized,
            "writing and reading give the same value back");
}

static void expect_unrecorded(struct window_placement value)
{
    struct window_placement again = written(value);
    require(!again.restore && !again.placed && again.width == 0 && again.height == 0 &&
                again.dpi == 96,
            "a value the reader would refuse is written as nothing recorded, keeping the choice");
}

static void verify_unwritable(void)
{
    struct window_placement value = {.restore = false, .placed = true, .dpi = 96};
    value.x = window_placement_bias + 1;
    expect_unrecorded(value);
    value.x = 0;
    value.y = -window_placement_bias - 1;
    expect_unrecorded(value);
    value.y = 0;
    value.width = -1;
    expect_unrecorded(value);
    value.width = 0;
    value.height = 2000000;
    expect_unrecorded(value);
    value.height = 0;
    value.dpi = 0;
    expect_unrecorded(value);
    value.dpi = 96 * 16 + 1;
    expect_unrecorded(value);
}

static void refuse(const char *_Nonnull text, enum window_placement_outcome expected)
{
    struct window_placement read = {.width = 7};
    require(window_placement_parse(text, strlen(text), &read) == expected && read.width == 7,
            "a refused document leaves the value untouched");
}

static void verify_refusals(void)
{
    refuse("", WINDOW_PLACEMENT_MALFORMED);
    refuse("[]", WINDOW_PLACEMENT_MALFORMED);
    refuse("{\"version\":\"1\"}", WINDOW_PLACEMENT_MALFORMED);
    refuse("{\"version\":2,\"restore\":true}", WINDOW_PLACEMENT_UNSUPPORTED_VERSION);
    refuse("{\"version\":1}", WINDOW_PLACEMENT_MALFORMED);
    refuse("{\"version\":1,\"restore\":1}", WINDOW_PLACEMENT_MALFORMED);
    refuse("{\"version\":1,\"placed\":true,\"restore\":true}", WINDOW_PLACEMENT_MALFORMED);
    refuse("{\"version\":1,\"restore\":true,\"placed\":true,\"x\":\"0\"}",
           WINDOW_PLACEMENT_MALFORMED);
    refuse("{\"version\":1,\"restore\":true,\"placed\":true,\"x\":2000001,\"y\":0,\"width\":1,"
           "\"height\":1,\"dpi\":96,\"maximized\":false}",
           WINDOW_PLACEMENT_MALFORMED);
    refuse("{\"version\":1,\"restore\":true,\"placed\":true,\"x\":0,\"y\":0,\"width\":1000001,"
           "\"height\":1,\"dpi\":96,\"maximized\":false}",
           WINDOW_PLACEMENT_MALFORMED);
    refuse("{\"version\":1,\"restore\":true,\"placed\":true,\"x\":0,\"y\":0,\"width\":1,"
           "\"height\":1,\"dpi\":0,\"maximized\":false}",
           WINDOW_PLACEMENT_MALFORMED);
    refuse("{\"version\":1,\"restore\":true,\"placed\":true,\"x\":0,\"y\":0,\"width\":1,"
           "\"height\":1,\"dpi\":96}",
           WINDOW_PLACEMENT_MALFORMED);
    refuse("{\"version\":1,\"restore\":true,\"placed\":true,\"x\":0,\"y\":0,\"width\":1,"
           "\"height\":1,\"dpi\":96,\"maximized\":false,\"extra\":1}",
           WINDOW_PLACEMENT_MALFORMED);
    refuse("{\"version\":1,\"restore\":true,\"placed\":true,\"x\":0,\"y\":0,\"width\":1,"
           "\"height\":1,\"dpi\":96,\"maximized\":false} x",
           WINDOW_PLACEMENT_MALFORMED);
}

/* One 1920x1040 work area at 96 DPI, and one at 144 DPI to its left (negative coordinates). */
static const struct window_area screens[] = {{0, 0, 1920, 1040, 96}, {-2560, 0, 2560, 1400, 144}};

static enum window_fit_outcome fit(struct window_placement saved, struct window_area *_Nonnull out)
{
    struct window_fit_request request = {.saved = &saved,
                                         .areas = screens,
                                         .count = 2,
                                         .minimum_width = 560,
                                         .minimum_height = 360,
                                         .header_height = 48,
                                         .grip_width = 160};
    return window_fit_resolve(&request, out);
}

static struct window_placement placed(int x, int y, int width, int height)
{
    return (struct window_placement){.restore = true,
                                     .placed = true,
                                     .x = x,
                                     .y = y,
                                     .width = width,
                                     .height = height,
                                     .dpi = 96};
}

static void expect_default(struct window_placement saved, const char *_Nonnull why)
{
    struct window_area out = {.width = -1};
    require(fit(saved, &out) == WINDOW_FIT_DEFAULT && out.width == -1, why);
}

static void verify_fit(void)
{
    struct window_area out = {0};
    require(fit(placed(100, 80, 960, 640), &out) == WINDOW_FIT_RESTORE && out.x == 100 &&
                out.y == 80 && out.width == 960 && out.height == 640 && out.dpi == 96,
            "a place inside a screen is restored as recorded");
    require(fit(placed(1500, 600, 960, 640), &out) == WINDOW_FIT_RESTORE && out.x == 1500,
            "hanging off the right and the bottom is kept while the header can be grabbed");
    require(fit(placed(-700, 80, 960, 640), &out) == WINDOW_FIT_RESTORE && out.dpi == 96,
            "across two screens, the first screen that shows enough of the header decides");
    require(fit(placed(-2000, 100, 960, 640), &out) == WINDOW_FIT_RESTORE && out.x == -2000 &&
                out.width == 1440 && out.height == 960 && out.dpi == 144,
            "on a screen with another DPI the size follows the DPI");
    /* #904: the header overhangs a small screen the window does not fit; the large screen next
     * to it holds both the header and the size. */
    static const struct window_area pair[] = {{-800, 0, 800, 500, 96}, {0, 0, 1920, 1040, 96}};
    struct window_placement wide = placed(-200, 0, 1800, 900);
    struct window_fit_request spanning = {.saved = &wide,
                                          .areas = pair,
                                          .count = 2,
                                          .minimum_width = 560,
                                          .minimum_height = 360,
                                          .header_height = 48,
                                          .grip_width = 160};
    require(window_fit_resolve(&spanning, &out) == WINDOW_FIT_RESTORE && out.x == -200 &&
                out.width == 1800,
            "a screen the size does not fit is skipped when another screen holds the window");
    struct window_placement off = placed(100, 80, 960, 640);
    off.restore = false;
    expect_default(off, "the user chose the initial place");
    off = placed(100, 80, 960, 640);
    off.placed = false;
    expect_default(off, "nothing was recorded");
    expect_default(placed(5000, 80, 960, 640), "no screen is there any more");
    expect_default(placed(100, -20, 960, 640), "the header is above the screen");
    expect_default(placed(100, 1010, 960, 640), "the header hangs below the work area");
    expect_default(placed(1800, 80, 960, 640), "less than the grip width of the header shows");
    expect_default(placed(100, 80, 400, 640), "narrower than the minimum");
    expect_default(placed(100, 80, 960, 200), "shorter than the minimum");
    expect_default(placed(0, 0, 2000, 640), "wider than the work area");
    expect_default(placed(0, 0, 960, 1100), "taller than the work area");
    expect_default(placed(100, 80, 0, 640), "a broken size");
    expect_default(placed(100, 80, 960, -5), "a broken size");
    struct window_placement broken = placed(100, 80, 960, 640);
    broken.dpi = 0;
    expect_default(broken, "a broken DPI");
    static const struct window_area unusable[] = {
        {0, 0, 1920, 1040, 0}, {0, 0, 0, 1040, 96}, {0, 0, 1920, 0, 96}};
    struct window_placement fine = placed(100, 80, 960, 640);
    struct window_fit_request request = {.saved = &fine,
                                         .areas = unusable,
                                         .count = 3,
                                         .minimum_width = 560,
                                         .minimum_height = 360,
                                         .header_height = 48,
                                         .grip_width = 160};
    require(window_fit_resolve(&request, &out) == WINDOW_FIT_DEFAULT,
            "unusable screens are skipped");
}

void run_window_placement_tests(void)
{
    verify_format();
    verify_unwritable();
    verify_refusals();
    verify_fit();
}
