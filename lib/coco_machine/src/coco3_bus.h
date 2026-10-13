// coco3_bus.h: the CoCo 3's side of the GIME (PIZERO-133), host-tested.
//
// On a CoCo 3 the GIME is the bus: the 6809's every cycle goes to
// tcc1014_mem_cycle (lib/xroar_core/src/tcc1014), which decodes the address
// through its MMU and calls the machine back with the result in S, RAS and Z.
// This header is that call-back's portable half, the parts both the firmware
// (coco_machine.cpp) and the host boot test (test/test_coco3_boot) need:
//
//   * the 128 KB of RAM, as two 64 KB halves (so neither has to be one
//     allocation), and the 32 KB Super Extended Color BASIC ROM
//   * moving the byte for a RAM, ROM or cartridge-ROM cycle; the PIAs and
//     the cartridge I/O are left to the caller, which has its own side
//     effects for them (interrupt lines, the DAC, the disk controller)
//   * the GIME's video fetch
//   * turning the GIME's scanlines into the 320x240 RGB565 framebuffer
//
// Upstream's version of all this is src/coco3.c (read_byte, write_byte,
// fetch_vram, create_ram), which is desktop glue and is not vendored.
//
// MEMORY: a 128 KB machine sees physical addresses modulo 128 KB, as
// upstream does with RAM_ORG(17, 9, 0): the GIME's 19-bit Z wraps.
//
// VIDEO: the GIME hands over each finished scanline as 912 six-bit colours
// (one per 14.318 MHz pixel). The 640 pixels around the active area are
// halved to the framebuffer's 320 columns: a 256- or 320-wide mode is exact,
// a 640-wide one (80 columns) loses every other pixel until PIZERO-134. The
// active area is centred vertically: 192 lines start at row 24, as the
// CoCo 2 picture does, and 225 lines at row 7.

#ifndef COCO3_BUS_H
#define COCO3_BUS_H

#include <stdbool.h>
#include <stdint.h>

#include "coco_palette.h"       // coco_pal_gime_to_rgb565

#define COCO3_ROM_SIZE   32768u
#define COCO3_FB_W       320
#define COCO3_FB_H       240

// What the caller still has to do for this cycle.
enum { C3_BUS_DONE = 0, C3_BUS_PIA0, C3_BUS_PIA1, C3_BUS_CARTIO };

struct coco3_bus {
    uint8_t *ram_lo;              // physical $00000-$0FFFF
    uint8_t *ram_hi;              // physical $10000-$1FFFF
    const uint8_t *rom;           // COCO3_ROM_SIZE bytes
    const uint8_t *cart;          // cartridge ROM, NULL for none
    uint16_t cart_mask;           // its size - 1

    // Video
    uint16_t *fb;                 // COCO3_FB_W x COCO3_FB_H, NULL for none
    uint16_t rgb[64];             // GIME colour to RGB565
    int line;                     // scanlines since the frame-sync pulse rose
    int act_x, act_y, act_w, act_h;   // the GIME's last set_active_area
};

static inline void coco3_bus_init(struct coco3_bus *b, uint8_t *lo, uint8_t *hi,
                                  const uint8_t *rom, uint16_t *fb) {
    b->ram_lo = lo;
    b->ram_hi = hi;
    b->rom = rom;
    b->cart = 0;
    b->cart_mask = 0;
    b->fb = fb;
    for (int i = 0; i < 64; i++) b->rgb[i] = coco_pal_gime_to_rgb565((uint8_t)i);
    b->line = 0;
    b->act_x = 239; b->act_y = 39; b->act_w = 512; b->act_h = 192;   // 32 columns
}

static inline uint8_t *coco3_ram_at(const struct coco3_bus *b, uint32_t Z) {
    Z &= 0x1FFFFu;
    return ((Z & 0x10000u) ? b->ram_hi : b->ram_lo) + (Z & 0xFFFFu);
}

// One bus cycle after tcc1014_mem_cycle has decoded it. Moves the byte for
// RAM, ROM and cartridge ROM; says what is left for the caller.
static inline int coco3_bus_cycle(const struct coco3_bus *b, unsigned S, bool RAS,
                                  uint32_t Z, bool RnW, uint16_t A, uint8_t *D) {
    int left = C3_BUS_DONE;
    switch (S) {
    case 0:                                    // internal ROM
        if (RnW) *D = b->rom[A & 0x7FFF];
        break;
    case 1:                                    // cartridge ROM (CTS)
        if (RnW && b->cart) *D = b->cart[A & b->cart_mask];
        break;
    case 2:                                    // the PIAs
        left = (A & 0x20) ? C3_BUS_PIA1 : C3_BUS_PIA0;
        break;
    case 6:                                    // cartridge I/O (SCS)
        left = C3_BUS_CARTIO;
        break;
    default:
        break;
    }
    if (RAS) {
        uint8_t *p = coco3_ram_at(b, Z);
        if (RnW) *D = *p;
        else     *p = *D;
    }
    return left;
}

// PIZERO-213: where the 6809 core may read and write one 8 KB logical page
// itself (coco_fastbus.h), given the GIME's decode of it (S as
// tcc1014_decode_page returns it, and the bank): the same bytes
// coco3_bus_cycle would move, or NULL for a page that must take the full
// cycle. Internal ROM is addressed by the logical address, as in
// coco3_bus_cycle; a cartridge smaller than a page, or none, is left slow.
static inline void coco3_bus_page(const struct coco3_bus *b, unsigned page, unsigned S,
                                  unsigned bank, const uint8_t **rd, uint8_t **wr) {
    *rd = 0;
    *wr = 0;
    switch (S) {
    case 0:
        *rd = b->rom + ((page << 13) & 0x7FFFu);
        break;
    case 1:
        if (b->cart && b->cart_mask >= 0x1FFFu) *rd = b->cart + ((page << 13) & b->cart_mask);
        break;
    case 7: {
        uint8_t *p = coco3_ram_at(b, (uint32_t)bank << 13);
        *rd = p;
        *wr = p;
        break;
    }
    default:
        break;
    }
}

// The GIME's video fetch: 16 bits at an even physical address.
static inline uint16_t coco3_bus_fetch_vram(const struct coco3_bus *b, uint32_t A) {
    const uint8_t *p = coco3_ram_at(b, A & ~1u);
    return (uint16_t)((p[0] << 8) | p[1]);
}

static inline void coco3_bus_set_active_area(struct coco3_bus *b, int x, int y, int w, int h) {
    if (h > 225) h = 225;                      // LPF "infinite": show what fits
    b->act_x = x; b->act_y = y; b->act_w = w; b->act_h = h;
}

static inline void coco3_bus_frame_sync(struct coco3_bus *b, bool level) {
    if (level) b->line = 0;
}

// The framebuffer row a scanline lands on, or -1.
static inline int coco3_bus_row(const struct coco3_bus *b, int line) {
    int row = line - b->act_y + (COCO3_FB_H - b->act_h) / 2;
    return (row >= 0 && row < COCO3_FB_H) ? row : -1;
}

// The first of the 640 GIME pixels shown: centred on the active area.
static inline int coco3_bus_first_pixel(const struct coco3_bus *b) {
    return b->act_x + b->act_w / 2 - COCO3_FB_W;
}

static inline void coco3_bus_render_line(struct coco3_bus *b, const uint8_t *data) {
    int row = coco3_bus_row(b, b->line++);
    if (row < 0 || !b->fb || !data) return;
    const uint8_t *src = data + coco3_bus_first_pixel(b);
    uint16_t *dst = b->fb + row * COCO3_FB_W;
    for (int x = 0; x < COCO3_FB_W; x++) dst[x] = b->rgb[src[2 * x] & 63];
}

#endif
