// c3host.h: a bare 128 KB CoCo 3 on the host (PIZERO-133). See c3host.c.
#ifndef C3HOST_H
#define C3HOST_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int c3_init(const uint8_t *rom32k);       // 1 on success
void c3_run_ticks(int32_t ticks);         // run for this many 14.318 MHz ticks
uint16_t c3_pc(void);
const uint16_t *c3_fb(void);              // 320x240 RGB565
uint8_t c3_phys(uint32_t Z);              // physical RAM, modulo 128 KB
#ifdef __cplusplus
}
#endif
#endif
