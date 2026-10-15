// wd_post.h: the watchdog post-mortem (PIZERO-227, for PIZERO-203).
//
// The watchdog (src/main.cpp, PIZERO-33) says only which loop() phase core 0
// was in when it stopped petting. This records where core 0 really was:
//
//  - An early warning: a spare timer0 alarm at the highest interrupt priority
//    on core 0 looks at the watchdog every 200 ms. Once it has under 1 s left
//    it saves what it interrupted (PC, LR, xPSR, so which IRQ core 0 was in,
//    if any) and the context around it. If it never runs, core 0 had
//    interrupts masked, sat in a handler of the same priority, or the bus
//    stalled; that is an answer too.
//  - A HardFault handler, set in the vector table at boot, that saves the
//    fault frame and the fault status registers, then waits for the watchdog,
//    so a crash is told apart from a hang. A fault on core 1 makes loop()
//    stop petting, so it resets as well.
//
// The record is kept in watchdog scratch[5..7] and powman scratch[0..7],
// which a watchdog reset leaves alone (RAM does not survive: the boot reuses
// it). Everything the handlers touch lives in RAM. Layout in wd_post.cpp.

#ifndef WD_POST_H
#define WD_POST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Core 1's scanline count, bumped by the audio stream callback: did core 1
// keep running while core 0 was stuck?
extern volatile uint32_t g_wd_core1_lines;

// After a watchdog freeze reboot, before wd_post_arm: describe the record in
// buf as one line (no line ending). Returns false if there is nothing to say.
bool wd_post_describe(char *buf, size_t n);

// Clear the record and start the early warning (core 0, once, in setup()).
void wd_post_arm(void);

// Once a frame from loop(): the context the warning saves, and the hook that
// lets a core 1 fault reach the watchdog.
void wd_post_frame(uint16_t guest_pc, uint32_t usb_devices);

// The watchdog is now running, so the early warning may judge it (loop()'s
// first pass). Before this its countdown means nothing.
void wd_post_watchdog_started(void);

#ifdef __cplusplus
}
#endif

#endif
