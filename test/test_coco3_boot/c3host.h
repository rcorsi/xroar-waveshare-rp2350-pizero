// c3host.h: a bare 128 KB CoCo 3 on the host (PIZERO-133). See c3host.c.
#ifndef C3HOST_H
#define C3HOST_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// 1 on success. fast_path: the firmware's MMU fast path (PIZERO-213).
int c3_init(const uint8_t *rom32k, int fast_path);
void c3_run_ticks(int32_t ticks);         // run for this many 14.318 MHz ticks
uint16_t c3_pc(void);
const uint16_t *c3_fb(void);              // 320x240 RGB565
uint8_t c3_phys(uint32_t Z);              // physical RAM, modulo 128 KB
struct c3_state {
    uint32_t tick;
    uint16_t pc, s, u, x, y;
    uint8_t cc;
};
void c3_state(struct c3_state *st);       // time and the CPU's registers
void c3_poke(uint32_t Z, uint8_t v);      // physical RAM, modulo 128 KB
void c3_set_pc(uint16_t pc);              // between runs: jump there
// PIZERO-214: hold (down=1) or release a key in the CoCo matrix: column
// 0-7 (PIA0 B bit), row 0-6 (PIA0 A bit), as upstream's keyboard.c.
void c3_key(int col, int row, int down);
uint8_t c3_gime_reg(int r);               // $FF90+r (0-15), palette $FFB0+r-16 (16-31), as last written
void c3_set_port_render(int on);          // PIZERO-218: the GIME's 320-pixel lines
                                          // (1: reading RAM directly, 2: through fetch_vram)
#ifdef __cplusplus
}
#endif
#endif
