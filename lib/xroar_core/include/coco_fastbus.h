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
 *
 *  Step (c): most cycles need nothing from the tail either. The machine sets
 *  deadline to the earliest event tick at which something is due (the next
 *  event, the next audio sample boundary, the end of the frame's budget, or
 *  now when the interrupt lines need refreshing). A cycle that ends before it
 *  only advances event_current_tick; the audio for those cycles is caught up
 *  exactly the next time the tail runs.
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
	uint32_t deadline;
};

extern struct coco_fastbus coco_fastbus;
extern uint32_t event_current_tick;   // events.h

// One 16-tick bus cycle that ends before the deadline: advance time and
// report 1. Otherwise leave time alone and report 0: call the tail.
static inline int coco_fastbus_advance(void) {
	uint32_t t = event_current_tick + 16;
	if ((int32_t)(t - coco_fastbus.deadline) >= 0)
		return 0;
	event_current_tick = t;
	return 1;
}

// One bus cycle whose data the core has already moved. A and RnW are what
// was on the bus; the machine ignores them, the lockstep harness records them.
void coco_mem_tick(void *sptr, _Bool RnW, uint16_t A);

#ifdef __cplusplus
}
#endif

#endif
