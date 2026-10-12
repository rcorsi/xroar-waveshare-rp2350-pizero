/** \file
 *
 *  \brief The memory windows the 6809 core may access directly (PIZERO-131).
 *
 *  Most bus cycles are plain RAM or ROM. The core moves those bytes itself
 *  and then calls coco_mem_tick for the per-cycle work (time, events, audio,
 *  interrupt lines, budget); only the rest goes through coco_mem_cycle's
 *  address decode. The machine owns this struct and keeps it current when
 *  the SAM's all-RAM bit or the cartridge changes.
 *
 *  Reads below ram_top come from ram[A], then below rom_top from
 *  rom[A & $3FFF], then below cart_top from cart[A & cart_mask]; writes
 *  below ram_top go to ram[A]. Everything else uses coco_mem_cycle. A window
 *  is empty when its top is not above the one before it.
 */

#ifndef XROAR_COCO_FASTBUS_H_
#define XROAR_COCO_FASTBUS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct coco_fastbus {
	uint8_t *ram;
	const uint8_t *rom;
	const uint8_t *cart;
	uint16_t ram_top;
	uint16_t rom_top;
	uint16_t cart_top;
	uint16_t cart_mask;
};

extern struct coco_fastbus coco_fastbus;

// One bus cycle whose data the core has already moved. A and RnW are what
// was on the bus; the machine ignores them, the lockstep harness records them.
void coco_mem_tick(void *sptr, _Bool RnW, uint16_t A);

#ifdef __cplusplus
}
#endif

#endif
