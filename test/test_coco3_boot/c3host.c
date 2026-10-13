// c3host.c: a bare 128 KB CoCo 3 on the host (PIZERO-133).
//
// The firmware's own CPU core, PIAs and the vendored GIME, wired the way
// upstream's src/coco3.c wires them, with coco3_bus.h doing what the
// firmware's machine does with the GIME's decode. No keyboard, sound, disk
// or cartridge: enough to boot Super Extended Color BASIC and look at it.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "delegate.h"
#include "events.h"
#include "part.h"
#include "xroar.h"
#include "mc6809/mc6809.h"
#include "mc6821.h"
#include "tcc1014/tcc1014.h"
#include "coco_fastbus.h"
#include "../../lib/coco_machine/src/coco3_bus.h"
// stdbool's bool macro would turn the delegate type names below into
// delegate_void__Bool (see lib/xroar_core/include/hot.h); this file says _Bool.
#undef bool

#include "c3host.h"

extern const struct partdb_entry mc6809_part, mc6821_part, tcc1014_1986_part, tcc1014_1987_part;

void *part_new(size_t psize) { return calloc(1, psize); }   // part.c, without its partdb

struct coco_fastbus coco_fastbus;          // every window empty: all cycles take the hook

static struct MC6809 *cpu;
static struct MC6821 *pia0, *pia1;
static struct TCC1014 *gime;
static struct coco3_bus bus;
static uint8_t ram[2][65536];
static uint8_t rom[COCO3_ROM_SIZE];
static uint16_t fb[COCO3_FB_W * COCO3_FB_H];
static int32_t budget;                     // event ticks left in this run

static struct part *make(const struct partdb_entry *e, void *options) {
    struct part *p = e->funcs->allocate();
    *p = (struct part){0};
    p->partdb = e;
    if (e->funcs->initialise) e->funcs->initialise(p, options);
    if (e->funcs->finish && !e->funcs->finish(p)) return NULL;
    return p;
}

// The core calls these by name (mc6809_common.c).
void coco_mem_cycle(void *sptr, _Bool RnW, uint16_t A) {
    (void)sptr;
    tcc1014_mem_cycle(gime, RnW, A);
}
void coco_mem_nvma(void *sptr) {
    (void)sptr;
    tcc1014_mem_cycle(gime, 1, 0xFFFF);
}
void coco_mem_tick(void *sptr, _Bool RnW, uint16_t A) {
    (void)sptr; (void)RnW; (void)A;
    abort();                               // no fast windows, so never reached
}

// The GIME's call-back for every cycle, as upstream's cpu_cycle.
static void cpu_cycle(void *sptr, int ncycles, _Bool RnW, uint16_t A) {
    (void)sptr;
    budget -= ncycles;
    if (budget <= 0) cpu->running = 0;
    event_run_queue(MACHINE_EVENT_LIST, ncycles);
    MC6809_IRQ_SET(cpu, pia0->a.irq || pia0->b.irq || gime->IRQ);
    MC6809_FIRQ_SET(cpu, pia1->a.irq || pia1->b.irq || gime->FIRQ);
    switch (coco3_bus_cycle(&bus, gime->S, gime->RAS, gime->Z, RnW, A, &cpu->D)) {
    case C3_BUS_PIA0:
        if (RnW) cpu->D = mc6821_read(pia0, A); else mc6821_write(pia0, A, cpu->D);
        break;
    case C3_BUS_PIA1:
        if (RnW) cpu->D = mc6821_read(pia1, A); else mc6821_write(pia1, A, cpu->D);
        break;
    default:
        break;
    }
    gime->IL1 = (PIA_VALUE_A(pia0) | 0x80) != 0xff;
    coco_fastbus.deadline = event_current_tick;   // keep the core off its fast path
}

static uint16_t fetch_vram(void *sptr, uint32_t A) { (void)sptr; return coco3_bus_fetch_vram(&bus, A); }
static void gime_hs(void *sptr, _Bool level) { (void)sptr; mc6821_set_cx1(&pia0->a, level); }
static void gime_fs(void *sptr, _Bool level) {
    (void)sptr;
    mc6821_set_cx1(&pia0->b, level);
    coco3_bus_frame_sync(&bus, level);
}
static void render_line(void *sptr, unsigned burst, unsigned npixels, const uint8_t *data) {
    (void)sptr; (void)burst; (void)npixels;
    coco3_bus_render_line(&bus, data);
}
static void set_active_area(void *sptr, int x, int y, int w, int h) {
    (void)sptr;
    coco3_bus_set_active_area(&bus, x, y, w, h);
}

int c3_init(const uint8_t *rom32k) {
    memcpy(rom, rom32k, sizeof rom);
    memset(ram, 0, sizeof ram);
    machine_event_list_global = event_list_new();
    event_current_tick = 0;
    coco3_bus_init(&bus, ram[0], ram[1], rom, fb);

    cpu  = (struct MC6809 *)make(&mc6809_part, NULL);
    pia0 = (struct MC6821 *)make(&mc6821_part, NULL);
    pia1 = (struct MC6821 *)make(&mc6821_part, NULL);
    gime = (struct TCC1014 *)make(&tcc1014_1987_part, NULL);
    if (!cpu || !pia0 || !pia1 || !gime) return 0;

    gime->cpu_cycle = DELEGATE_AS3(void, int, bool, uint16, cpu_cycle, NULL);
    gime->fetch_vram = DELEGATE_AS1(uint16, uint32, fetch_vram, NULL);
    gime->signal_hs = DELEGATE_AS1(void, bool, gime_hs, NULL);
    gime->signal_fs = DELEGATE_AS1(void, bool, gime_fs, NULL);
    gime->render_line = DELEGATE_AS3(void, unsigned, unsigned, uint8cp, render_line, NULL);
    gime->set_active_area = DELEGATE_AS4(void, int, int, int, int, set_active_area, NULL);
    gime->CPUD = &cpu->D;

    // As upstream's coco3_finish: nothing on the ports, serial in held low.
    pia0->b.in_source = 0;
    pia1->b.in_source = 0;
    pia0->a.in_sink = pia0->b.in_sink = 0xff;
    pia1->a.in_sink = pia1->b.in_sink = 0xff;
    pia1->b.in_sink &= ~(1 << 0);

    tcc1014_notify_mode(gime);
    mc6821_reset(pia0);
    mc6821_reset(pia1);
    tcc1014_reset(gime);
    cpu->reset(cpu);
    return 1;
}

void c3_run_ticks(int32_t ticks) {
    budget += ticks;
    while (budget > 0) {
        coco_fastbus.deadline = event_current_tick;
        cpu->running = 1;
        cpu->run(cpu);
    }
}

uint16_t c3_pc(void) { return cpu->reg_pc; }
const uint16_t *c3_fb(void) { return fb; }

uint8_t c3_phys(uint32_t Z) { return *coco3_ram_at(&bus, Z); }
