// PIZERO-133: boot a 128 KB CoCo 3 on the host.
//
// The firmware's 6809 core and PIAs with the vendored GIME (tcc1014.c) and
// coco3_bus.h, running Super Extended Color BASIC from a real coco3.rom
// (32 KB; the repo ships no ROMs). Passes when BASIC has printed its banner
// and OK on the 32-column screen.
//
// PIZERO-213: the boot runs with the firmware's MMU fast path, and a second
// test boots with and without it and checks the two machines end up the same:
// time, CPU registers and all 128 KB of RAM.
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
    TEST_ASSERT_TRUE(c3_init(g_rom, 1));
    for (int f = 0; f < 180; f++) c3_run_ticks(FRAME_TICKS);   // 3 s
    char row[33];
    for (int r = 0; r < 16; r++) { screen_row(r, row); printf("  |%s|\n", row); }
    printf("  pc=%04x\n", c3_pc());
    dump();
    TEST_ASSERT_TRUE_MESSAGE(screen_has("EXTENDED COLOR BASIC"), "no banner on the screen");
    TEST_ASSERT_TRUE_MESSAGE(screen_has("OK"), "no OK prompt");
}

// Boot for `frames` fields and capture the machine: time, registers, RAM.
static void boot(int fast, int frames, struct c3_state *st, uint8_t *ram) {
    TEST_ASSERT_TRUE(c3_init(g_rom, fast));
    for (int f = 0; f < frames; f++) c3_run_ticks(FRAME_TICKS);
    c3_state(st);
    for (uint32_t z = 0; z < 0x20000; z++) ram[z] = c3_phys(z);
}

static void test_fast_path_matches(void) {
    if (!load_rom()) TEST_IGNORE_MESSAGE("set COCO_ROM_DIR to a folder with coco3.rom (32 KB)");
    static uint8_t ram_slow[0x20000], ram_fast[0x20000];
    struct c3_state slow, fastst;
    // Every frame boundary is a check: the first difference names its field.
    for (int frames = 1; frames <= 180; frames = frames < 8 ? frames + 1 : frames * 2) {
        boot(0, frames, &slow, ram_slow);
        boot(1, frames, &fastst, ram_fast);
        char msg[96];
        snprintf(msg, sizeof msg, "after %d fields: slow pc=%04x fast pc=%04x", frames, slow.pc, fastst.pc);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(slow.tick, fastst.tick, msg);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(slow.pc, fastst.pc, msg);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(slow.s, fastst.s, msg);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(slow.u, fastst.u, msg);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(slow.x, fastst.x, msg);
        TEST_ASSERT_EQUAL_HEX16_MESSAGE(slow.y, fastst.y, msg);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(slow.cc, fastst.cc, msg);
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(ram_slow, ram_fast, sizeof ram_slow, msg);
    }
}

// After BASIC is up: switch to 1.79 MHz, then keep mapping bank $30 into
// logical $4000-$5FFF, filling it, and mapping the default bank back, with
// BASIC's 60 Hz interrupt running throughout. Loaded at logical $3000
// (physical $13000 in BASIC's default map); counts its passes at $3080.
static const uint8_t k_remap[] = {
    0xB7, 0xFF, 0xD9,        // 3000 STA $FFD9      1.79 MHz
    0x86, 0x30,              // 3003 LDA #$30       loop:
    0xB7, 0xFF, 0xA2,        // 3005 STA $FFA2      bank $30 at $4000
    0x8E, 0x40, 0x00,        // 3008 LDX #$4000
    0xE7, 0x80,              // 300B STB ,X+        fill:
    0x8C, 0x60, 0x00,        // 300D CMPX #$6000
    0x26, 0xF9,              // 3010 BNE fill
    0x5C,                    // 3012 INCB
    0x86, 0x3A,              // 3013 LDA #$3A
    0xB7, 0xFF, 0xA2,        // 3015 STA $FFA2      the default bank back
    0x7C, 0x30, 0x80,        // 3018 INC $3080
    0x20, 0xE6,              // 301B BRA loop
};

static void remap_run(int fast, struct c3_state *st, uint8_t *ram) {
    boot(fast, 180, st, ram);
    for (unsigned i = 0; i < sizeof k_remap; i++) c3_poke(0x13000 + i, k_remap[i]);
    c3_poke(0x13080, 0);
    c3_set_pc(0x3000);
    for (int f = 0; f < 30; f++) c3_run_ticks(FRAME_TICKS);
    c3_state(st);
    for (uint32_t z = 0; z < 0x20000; z++) ram[z] = c3_phys(z);
}

static void test_fast_path_remap(void) {
    if (!load_rom()) TEST_IGNORE_MESSAGE("set COCO_ROM_DIR to a folder with coco3.rom (32 KB)");
    static uint8_t ram_slow[0x20000], ram_fast[0x20000];
    struct c3_state slow, fastst;
    remap_run(0, &slow, ram_slow);
    remap_run(1, &fastst, ram_fast);
    printf("  passes=%u (slow) %u (fast), bank $30 byte 0 = %02x\n",
           ram_slow[0x13080], ram_fast[0x13080], ram_fast[0x00000]);
    TEST_ASSERT_TRUE_MESSAGE(ram_slow[0x13080] > 2, "the program did not run");
    TEST_ASSERT_EQUAL_UINT32(slow.tick, fastst.tick);
    TEST_ASSERT_EQUAL_HEX16(slow.pc, fastst.pc);
    TEST_ASSERT_EQUAL_HEX16(slow.s, fastst.s);
    TEST_ASSERT_EQUAL_HEX16(slow.x, fastst.x);
    TEST_ASSERT_EQUAL_HEX8(slow.cc, fastst.cc);
    TEST_ASSERT_EQUAL_MEMORY(ram_slow, ram_fast, sizeof ram_slow);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_boots_to_ok);
    RUN_TEST(test_fast_path_matches);
    RUN_TEST(test_fast_path_remap);
    return UNITY_END();
}
