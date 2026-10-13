// PIZERO-133: boot a 128 KB CoCo 3 on the host.
//
// The firmware's 6809 core and PIAs with the vendored GIME (tcc1014.c) and
// coco3_bus.h, running Super Extended Color BASIC from a real coco3.rom
// (32 KB; the repo ships no ROMs). Passes when BASIC has printed its banner
// and OK on the 32-column screen.
//
//   COCO_ROM_DIR=~/Desktop/coco/roms pio test -e native -f test_coco3_boot
//
// COCO3_DUMP=<file> also writes the last frame as raw RGB565 (320x240,
// little-endian) for a look at the picture.

#include <unity.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c3host.h"

void setUp(void) {}
void tearDown(void) {}

#define FRAME_TICKS (912 * 263)           // one 60 Hz field in 14.318 MHz ticks

static uint8_t g_rom[32768];

static bool load_rom(void) {
    const char *dir = getenv("COCO_ROM_DIR");
    if (!dir || !dir[0]) return false;
    char path[512];
    snprintf(path, sizeof path, "%s/coco3.rom", dir);
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    size_t n = fread(g_rom, 1, sizeof g_rom, f);
    fclose(f);
    return n == sizeof g_rom;
}

// The 32-column screen as text. BASIC keeps it at logical $0400 in the
// 64 KB its MMU maps to the top of physical RAM ($70000, which a 128 KB
// machine sees at $10000).
static void screen_row(int row, char out[33]) {
    for (int c = 0; c < 32; c++) {
        uint8_t v = c3_phys(0x10400 + row * 32 + c) & 0x7F;
        char ch = (char)(v & 0x3F);
        if (ch < 0x20) ch += 0x40;      // the VDG's @A-Z[\]^_ block
        out[c] = ch;
    }
    out[32] = '\0';
}

static bool screen_has(const char *s) {
    char row[33];
    for (int r = 0; r < 16; r++) {
        screen_row(r, row);
        if (strstr(row, s)) return true;
    }
    return false;
}

static void dump(void) {
    const char *path = getenv("COCO3_DUMP");
    if (!path || !path[0]) return;
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite(c3_fb(), 2, 320 * 240, f);
    fclose(f);
}

static void test_boots_to_ok(void) {
    if (!load_rom()) TEST_IGNORE_MESSAGE("set COCO_ROM_DIR to a folder with coco3.rom (32 KB)");
    TEST_ASSERT_TRUE(c3_init(g_rom));
    for (int f = 0; f < 180; f++) c3_run_ticks(FRAME_TICKS);   // 3 s
    char row[33];
    for (int r = 0; r < 16; r++) { screen_row(r, row); printf("  |%s|\n", row); }
    printf("  pc=%04x\n", c3_pc());
    dump();
    TEST_ASSERT_TRUE_MESSAGE(screen_has("EXTENDED COLOR BASIC"), "no banner on the screen");
    TEST_ASSERT_TRUE_MESSAGE(screen_has("OK"), "no OK prompt");
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_boots_to_ok);
    return UNITY_END();
}
