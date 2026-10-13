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

/*  PIZERO-213, the CoCo 3 (COCO_FASTBUS_PAGES, implied by COCO3): the GIME's
 *  MMU maps each 8 KB logical page anywhere in physical RAM, or to ROM, so
 *  flat windows cannot describe it. The core instead looks the page up:
 *  below page_top, a read of A comes from rd[A >> 13][A & $1FFF] when that
 *  page's rd is set, and a write goes to wr[A >> 13][A & $1FFF] when its wr
 *  is set; everything else uses coco_mem_cycle. A bus cycle is ticks event
 *  ticks long (8 at 1.79 MHz). The machine rebuilds the table after every
 *  write that can change the map: the MMU registers, $FF90/$FF91, the SAM
 *  subset and the cartridge. rom is still what a dummy cycle reads:
 *  rom[$3FFF] is the byte at $FFFF.
 */
#if defined(COCO3) && !defined(COCO_FASTBUS_PAGES)
#define COCO_FASTBUS_PAGES 1
#endif

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
#ifdef COCO_FASTBUS_PAGES
	const uint8_t *rd[8];
	uint8_t *wr[8];
	uint16_t page_top;
	uint16_t ticks;
#endif
};

extern struct coco_fastbus coco_fastbus;
extern uint32_t event_current_tick;   // events.h

// One 16-tick (paged: ticks-long) bus cycle that ends before the deadline: advance time and
// report 1. Otherwise leave time alone and report 0: call the tail.
static inline int coco_fastbus_advance(void) {
#ifdef COCO_FASTBUS_PAGES
	uint32_t t = event_current_tick + coco_fastbus.ticks;
#else
	uint32_t t = event_current_tick + 16;
#endif
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
