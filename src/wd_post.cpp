// wd_post.cpp: see wd_post.h (PIZERO-227).

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "hardware/irq.h"
#include "hardware/structs/powman.h"
#include "hardware/structs/scb.h"
#include "hardware/structs/sio.h"
#include "hardware/structs/watchdog.h"
#include "hardware/timer.h"

#include "wd_post.h"

#define WDP_PERIOD_US 200000u       // the early warning looks this often
#define WDP_WARN_US   1000000u      // and saves once the watchdog has less than this left
#define WDP_MAGIC     0x57445031u   // "WDP1" in powman scratch[7]: the record is this layout

enum { WDP_NONE = 0, WDP_ARMED = 1, WDP_HANG = 2, WDP_FAULT = 3 };

// The record lives only in registers that a watchdog reset leaves alone. RAM
// does not do: the boot reuses the start of SRAM (a test found code there).
//
//   watchdog scratch[5]  PC of the interrupted or faulting code
//   watchdog scratch[6]  its LR
//   watchdog scratch[7]  kind << 24 | core << 20 | saves << 12 (to 255) | exception number
//   powman scratch[0]    uptime in ms at the last save
//   powman scratch[1]    ms from the first save to the last << 16 | core 1 lines meanwhile (to 65535)
//   powman scratch[2]    6809 PC << 16 | USB devices << 8 | EXC_RETURN's low byte
//   powman scratch[3..6] a hang: core 0's NVIC active[0], active[1], pending[0], pending[1]
//                        a fault: CFSR, HFSR, BFAR, MMFAR
//   powman scratch[7]    WDP_MAGIC
#define S5 watchdog_hw->scratch[5]
#define S6 watchdog_hw->scratch[6]
#define S7 watchdog_hw->scratch[7]
#define P(i) powman_hw->scratch[i]

volatile uint32_t g_wd_core1_lines;

// The context loop() leaves each frame, for the handlers to copy.
static volatile uint32_t g_ctx_pc, g_ctx_usb;
static volatile bool g_wd_running;
static int g_alarm = -1;
static uint32_t g_saves, g_first_ms, g_c1_first;
static volatile uint32_t g_kind = WDP_NONE;

#define NVIC_ISPR ((volatile uint32_t *)0xE000E200u)
#define NVIC_IABR ((volatile uint32_t *)0xE000E300u)
#define SCB_CFSR  (*(volatile uint32_t *)0xE000ED28u)
#define SCB_HFSR  (*(volatile uint32_t *)0xE000ED2Cu)
#define SCB_MMFAR (*(volatile uint32_t *)0xE000ED34u)
#define SCB_BFAR  (*(volatile uint32_t *)0xE000ED38u)

// Uptime from the raw timer registers: the handlers must not call into flash.
static inline uint32_t now_ms(void) {
    uint32_t hi, lo;
    do { hi = timer_hw->timerawh; lo = timer_hw->timerawl; } while (hi != timer_hw->timerawh);
    return (uint32_t)((((uint64_t)hi << 32) | lo) / 1000u);
}

static void __not_in_flash_func(save_common)(uint32_t kind, uint32_t core, const uint32_t *frame,
                                             uint32_t exc_return) {
    uint32_t t = now_ms();
    if (g_saves == 0) { g_first_ms = t; g_c1_first = g_wd_core1_lines; }
    g_saves++;
    uint32_t span = t - g_first_ms, c1 = g_wd_core1_lines - g_c1_first;
    S5 = frame[6];
    S6 = frame[5];
    S7 = (kind << 24) | ((core & 0xFu) << 20) | ((g_saves > 255 ? 255 : g_saves) << 12) | (frame[7] & 0x1FFu);
    P(0) = t;
    P(1) = ((span > 0xFFFFu ? 0xFFFFu : span) << 16) | (c1 > 0xFFFFu ? 0xFFFFu : c1);
    P(2) = (g_ctx_pc << 16) | ((g_ctx_usb & 0xFFu) << 8) | (exc_return & 0xFFu);
    g_kind = kind;
}

// frame: the stacked R0-R3, R12, LR, PC, xPSR of what was interrupted.
extern "C" void __not_in_flash_func(wd_post_warn_c)(uint32_t *frame, uint32_t exc_return) {
    timer_hw->intr = 1u << g_alarm;
    timer_hw->alarm[g_alarm] = timer_hw->timerawl + WDP_PERIOD_US;
    if (!g_wd_running || g_kind == WDP_FAULT) return;
    uint32_t left = watchdog_hw->ctrl & WATCHDOG_CTRL_TIME_BITS;   // us on the RP2350
    if (left >= WDP_WARN_US) return;
    save_common(WDP_HANG, 0, frame, exc_return);
    P(3) = NVIC_IABR[0] & ~(1u << g_alarm); P(4) = NVIC_IABR[1];   // less this handler
    P(5) = NVIC_ISPR[0]; P(6) = NVIC_ISPR[1];
}

extern "C" void __not_in_flash_func(wd_post_fault_c)(uint32_t *frame, uint32_t exc_return) {
    if (g_kind != WDP_FAULT) {
        g_saves = 0;
        save_common(WDP_FAULT, sio_hw->cpuid, frame, exc_return);
        P(3) = SCB_CFSR; P(4) = SCB_HFSR; P(5) = SCB_BFAR; P(6) = SCB_MMFAR;
    }
    // Wait for the watchdog. On core 1, loop() sees the fault and stops petting.
    for (;;) __asm volatile ("nop");
}

// Exception entries: hand the stacked frame (MSP or PSP, by EXC_RETURN bit 2)
// and EXC_RETURN to the C halves above.
extern "C" __attribute__((naked, section(".time_critical.wd_post_warn"))) void wd_post_warn_isr(void) {
    __asm volatile (
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "b wd_post_warn_c\n");
}

// Installed over the vector table's HardFault entry by wd_post_arm. The SD
// library (no-OS-FatFS, crash.c) links its own isr_hardfault, which stops at a
// breakpoint and records nothing we read, so this one is set at run time.
extern "C" __attribute__((naked, section(".time_critical.wd_post_fault"))) void wd_post_fault_isr(void) {
    __asm volatile (
        "tst lr, #4\n"
        "ite eq\n"
        "mrseq r0, msp\n"
        "mrsne r0, psp\n"
        "mov r1, lr\n"
        "b wd_post_fault_c\n");
}

static const char *irq_name(unsigned exc) {
    if (exc == 0) return "thread mode (no interrupt)";
    if (exc == 3) return "HardFault";
    if (exc == 11) return "SVCall";
    if (exc == 14) return "PendSV";
    if (exc == 15) return "SysTick";
    if (exc < 16) return "a system exception";
    static char s[24];
    unsigned irq = exc - 16;
    const char *nm = nullptr;
    switch (irq) {
        case 0: case 1: case 2: case 3: snprintf(s, sizeof s, "TIMER0_IRQ_%u", irq); return s;
        case 10: case 11: case 12: case 13: snprintf(s, sizeof s, "DMA_IRQ_%u", irq - 10); return s;
        case 14: nm = "USBCTRL_IRQ"; break;
        case 15: nm = "PIO0_IRQ_0"; break;  case 16: nm = "PIO0_IRQ_1"; break;
        case 17: nm = "PIO1_IRQ_0"; break;  case 18: nm = "PIO1_IRQ_1"; break;
        case 21: nm = "IO_IRQ_BANK0"; break;
        case 25: nm = "SIO_IRQ_FIFO"; break; case 26: nm = "SIO_IRQ_BELL"; break;
        case 33: nm = "UART0_IRQ"; break;
        default: break;
    }
    if (nm) return nm;
    snprintf(s, sizeof s, "IRQ %u", irq);
    return s;
}

extern "C" bool wd_post_describe(char *buf, size_t n) {
    uint32_t s7 = S7, kind = s7 >> 24;
    if (P(7) != WDP_MAGIC || kind == WDP_NONE || kind > WDP_FAULT) {
        snprintf(buf, n, "[watchdog] post-mortem: no record (a power-on, or a build before PIZERO-227)");
        return true;
    }
    if (kind == WDP_ARMED) {
        snprintf(buf, n, "[watchdog] post-mortem: the early warning never ran: core 0 had interrupts "
                         "masked, was in a top-priority handler, or the bus stalled");
        return true;
    }
    uint32_t core = (s7 >> 20) & 0xFu, saves = (s7 >> 12) & 0xFFu, exc = s7 & 0x1FFu;
    uint32_t span = P(1) >> 16, c1 = P(1) & 0xFFFFu;
    uint32_t guest_pc = P(2) >> 16, usb = (P(2) >> 8) & 0xFFu, excret = P(2) & 0xFFu;
    if (kind == WDP_FAULT) {
        // The fault addresses mean something only when CFSR says so.
        uint32_t cfsr = P(3);
        char bfar[24] = "", mmfar[24] = "";
        if (cfsr & (1u << 15)) snprintf(bfar, sizeof bfar, " bfar=%08lx", (unsigned long)P(5));
        if (cfsr & (1u << 7))  snprintf(mmfar, sizeof mmfar, " mmfar=%08lx", (unsigned long)P(6));
        snprintf(buf, n, "[watchdog] post-mortem: HARD FAULT on core %lu pc=%08lx lr=%08lx in %s "
                         "cfsr=%08lx hfsr=%08lx%s%s exc_return=..%02lx "
                         "guest_pc=%04lx usb=%lu up=%lums",
                 (unsigned long)core, (unsigned long)S5, (unsigned long)S6, irq_name(exc),
                 (unsigned long)cfsr, (unsigned long)P(4), bfar, mmfar,
                 (unsigned long)excret, (unsigned long)guest_pc, (unsigned long)usb,
                 (unsigned long)P(0));
        return true;
    }
    snprintf(buf, n, "[watchdog] post-mortem: hang, core 0 at pc=%08lx lr=%08lx in %s; "
                     "%lu warnings over %lu ms, core 1 %s (+%lu lines); "
                     "active=%05lx:%08lx pending=%05lx:%08lx exc_return=..%02lx; "
                     "guest_pc=%04lx usb=%lu up=%lums",
             (unsigned long)S5, (unsigned long)S6, irq_name(exc),
             (unsigned long)saves, (unsigned long)span,
             saves < 2 ? "unknown" : (c1 ? "running" : "STOPPED"), (unsigned long)c1,
             (unsigned long)P(4), (unsigned long)P(3), (unsigned long)P(6), (unsigned long)P(5),
             (unsigned long)excret, (unsigned long)guest_pc, (unsigned long)usb,
             (unsigned long)P(0));
    return true;
}

extern "C" void wd_post_arm(void) {
    S5 = S6 = 0;
    S7 = (uint32_t)WDP_ARMED << 24;
    for (int i = 0; i < 7; i++) P(i) = 0;
    P(7) = WDP_MAGIC;
    g_saves = 0;
    g_kind = WDP_ARMED;
    if (g_alarm >= 0) return;
    // HardFault is exception 3. The table is in RAM and core 1 is started
    // with the same one, so this covers both cores.
    ((void (**)(void))scb_hw->vtor)[3] = wd_post_fault_isr;
    g_alarm = hardware_alarm_claim_unused(false);
    if (g_alarm < 0) { Serial.print("[watchdog] no spare timer alarm: post-mortem early warning off\r\n"); return; }
    unsigned irq = TIMER_ALARM_IRQ_NUM(timer_hw, g_alarm);
    irq_set_exclusive_handler(irq, wd_post_warn_isr);
    irq_set_priority(irq, 0);                     // above everything else on core 0
    hw_set_bits(&timer_hw->inte, 1u << g_alarm);
    timer_hw->alarm[g_alarm] = timer_hw->timerawl + WDP_PERIOD_US;
    irq_set_enabled(irq, true);
}

extern "C" void wd_post_watchdog_started(void) { g_wd_running = true; }

extern "C" void wd_post_frame(uint16_t guest_pc, uint32_t usb_devices) {
    g_ctx_pc = guest_pc;
    g_ctx_usb = usb_devices;
    // A fault on core 1: stop here so the watchdog resets the board.
    if (g_kind == WDP_FAULT) for (;;) tight_loop_contents();
}
