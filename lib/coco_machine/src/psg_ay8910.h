// psg_ay8910.h: the General Instrument AY-3-8910 (and Yamaha YM2149) sound
// chip, and the CoCo cartridges that carry it (PIZERO-149). Pure integer
// logic, host-tested.
//
// The chip follows the AY-3-8910 data sheet, with XRoar's model
// (~/github/xroar/src/ay891x.c) and MAME's (src/devices/sound/ay8910.cpp)
// as references:
//   * the chip steps at its clock / 8 ("chip ticks")
//   * 3 tone channels: a 12-bit period TP (0 counts as 1); the square output
//     toggles every TP ticks, so f = clock / (16 * TP)
//   * 1 noise generator: a 17-bit shift register (feedback bit 0 xor bit 3)
//     stepped every 2 * NP ticks (5-bit NP, 0 counts as 1), f = clock / (16 * NP)
//   * mixer (R7, bits active low): a channel is on when (tone or tone off) and
//     (noise or noise off). A channel with both off is steadily on at its
//     level, which is how AY sample playback works; XRoar's model makes that
//     silent instead
//   * 4-bit level per channel, about 3 dB a step (each step 1/sqrt 2), 0 off;
//     bit 4 hands the channel to the envelope
//   * envelope: 16 steps, one every 2 * EP ticks (16-bit EP, 0 counts as 1);
//     R13's CONT, ATT, ALT and HOLD bits give the data sheet's ten shapes,
//     and writing R13 restarts it
//   * bus: latch a register number (0-15), then read or write it. Reads give
//     the bits the register has (R1/3/5 4 bits, R6 5, R8-10 5, R13 4); the
//     I/O ports (R14, R15) are not wired here and read $FF
// It starts silent: every level 0.
//
// The cartridges, sharing one bank of four chips (PIZERO-149):
//   * CoCo PSG (a YM2149): $FF5E latches a register, $FF5F reads and writes
//     it, $FF5D bit 0 picks the clock (0 = 2 MHz, 1 = 1 MHz). Chip 0.
//   * Symphony-12 (Speech Systems, four AY-3-8910s): a 6821 PIA at
//     $FF60-$FF63. Port A is the chips' data bus; port B carries two bits per
//     chip (bits 1-0 chip 0 ... bits 7-6 chip 3): 11 latch, 10 write, 01 read.
//     A port A write acts with port B as it stands, a port B write with port A
//     as it stands, as MAME's coco_sym12.cpp does. Clocked from the cartridge
//     port, 14.31818 MHz / 16. Chips 0-3.
//   * Deluxe CoCo (Tandy's 1984 prototype, an AY-3-8913): $FF38 writes data,
//     $FF39 latches a register on a write and reads data, at 14.31818 MHz / 8
//     as XRoar clocks it. Chip 0. On a CoCo 2 these addresses mirror PIA1.
// Each sets its chips' clock when the program uses it, so the pitch is right
// for whichever card the program was written for.
//
// Output is averaged over each audio sample, the same integrate-and-average
// the DAC and the SN76489 use, so a tone above the sample rate does not alias.

#ifndef PSG_AY8910_H
#define PSG_AY8910_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define AY_EVENT_HZ      14318180u        // the machine's event ticks per second
#define AY_CHIPS         4

#define AY_CLK_PSG_2MHZ  2000000u
#define AY_CLK_PSG_1MHZ  1000000u
#define AY_CLK_SYM12     (AY_EVENT_HZ / 16)
#define AY_CLK_DELUXE    (AY_EVENT_HZ / 8)

// Level of one channel at each 4-bit amplitude: full scale 1260, each step
// down 1/sqrt 2. One chip's three channels at full match the DAC's loudest,
// as the SN76489's four do (csg_sn76489.h).
static const uint16_t AY_LEVEL[16] = {
       0,   10,   14,   20,   28,   39,   56,   79,
     111,  158,  223,  315,  445,  630,  891, 1260
};

// Bits each register keeps.
static const uint8_t AY_REG_MASK[16] = {
    0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0xFF,
    0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0x0F, 0xFF, 0xFF
};

struct ay8910 {
    uint8_t  reg[16];
    uint8_t  addr;               // latched register number
    uint16_t tone_count[3];
    uint8_t  tone_out[3];
    uint8_t  noise_count;
    uint8_t  noise_half;         // the noise generator runs at half the tone rate
    uint8_t  noise_out;
    uint32_t lfsr;
    uint16_t env_count;
    uint8_t  env_half;
    uint8_t  env_pos;            // 0-15 within a ramp
    uint8_t  env_level;          // 0-15
    bool     env_att, env_alt, env_hold, env_cont, env_holding;
    uint32_t clock_hz;
    uint32_t frac;               // event ticks not yet turned into chip ticks
    int32_t  last;               // last output level
};

static inline void ay_set_clock(struct ay8910 *c, uint32_t hz) { c->clock_hz = hz; }

static inline void ay_reset(struct ay8910 *c, uint32_t clock_hz) {
    memset(c, 0, sizeof *c);
    for (int i = 0; i < 3; i++) c->tone_count[i] = 1;
    c->noise_count = 1;
    c->env_count = 1;
    c->lfsr = 1;
    c->env_level = 15;           // shape 0 starts at the top
    c->clock_hz = clock_hz;
}

static inline unsigned ay_tone_period(const struct ay8910 *c, int ch) {
    unsigned tp = ((unsigned)(c->reg[ch * 2 + 1] & 0x0F) << 8) | c->reg[ch * 2];
    return tp ? tp : 1;
}

static inline void ay_env_set_level(struct ay8910 *c) {
    c->env_level = c->env_att ? c->env_pos : (uint8_t)(15 - c->env_pos);
}

static inline void ay_latch(struct ay8910 *c, uint8_t d) { c->addr = d & 0x0F; }

static inline void ay_write(struct ay8910 *c, uint8_t d) {
    unsigned r = c->addr;
    c->reg[r] = d & AY_REG_MASK[r];
    if (r == 13) {                           // a new shape restarts the envelope
        c->env_cont = (d & 8) != 0;
        c->env_att  = (d & 4) != 0;
        c->env_alt  = (d & 2) != 0;
        c->env_hold = (d & 1) != 0;
        c->env_pos = 0;
        c->env_holding = false;
        c->env_count = 1;
        c->env_half = 0;
        ay_env_set_level(c);
    }
}

static inline uint8_t ay_read(const struct ay8910 *c) {
    return c->addr >= 14 ? 0xFF : c->reg[c->addr];
}

// One envelope step: the next level of the ramp, or what the shape does at
// its end.
static inline void ay_env_step(struct ay8910 *c) {
    if (c->env_holding) return;
    if (++c->env_pos < 16) { ay_env_set_level(c); return; }
    if (!c->env_cont) {                      // shapes 0-7: one ramp, then 0
        c->env_holding = true;
        c->env_level = 0;
    } else if (c->env_hold) {                // stop at the end, or the other end
        c->env_holding = true;
        bool top = c->env_att != c->env_alt;
        c->env_level = top ? 15 : 0;
    } else {                                 // repeat, turning round if ALT
        if (c->env_alt) c->env_att = !c->env_att;
        c->env_pos = 0;
        ay_env_set_level(c);
    }
}

// Advance one chip tick (clock / 8) and return the summed output level.
static inline int32_t ay_tick(struct ay8910 *c) {
    for (int ch = 0; ch < 3; ch++) {
        if (--c->tone_count[ch] == 0) {
            c->tone_count[ch] = (uint16_t)ay_tone_period(c, ch);
            c->tone_out[ch] ^= 1;
        }
    }
    if ((c->noise_half ^= 1) == 0 && --c->noise_count == 0) {
        unsigned np = c->reg[6] & 0x1F;
        c->noise_count = (uint8_t)(np ? np : 1);
        uint32_t fb = (c->lfsr ^ (c->lfsr >> 3)) & 1;
        c->lfsr = (c->lfsr >> 1) | (fb << 16);
        c->noise_out = c->lfsr & 1;
    }
    if ((c->env_half ^= 1) == 0 && --c->env_count == 0) {
        unsigned ep = ((unsigned)c->reg[12] << 8) | c->reg[11];
        c->env_count = (uint16_t)(ep ? ep : 1);
        ay_env_step(c);
    }
    uint8_t mix = c->reg[7];
    int32_t out = 0;
    for (int ch = 0; ch < 3; ch++) {
        bool on = (c->tone_out[ch] | ((mix >> ch) & 1)) & (c->noise_out | ((mix >> (ch + 3)) & 1));
        if (!on) continue;
        uint8_t a = c->reg[8 + ch];
        out += AY_LEVEL[(a & 0x10) ? c->env_level : (a & 0x0F)];
    }
    return out;
}

// The chip's average output over `event_ticks` of machine time: one audio
// sample's worth. 32-bit arithmetic only: the RP2350 divides 32 bits in
// hardware but 64 in a library call, and that call, once per chip per
// sample, cost about 2 ms a frame per chip (PIZERO-149). A sample is about
// 300 event ticks; the clamp keeps the product in range for any caller.
static inline int32_t ay_render(struct ay8910 *c, uint32_t event_ticks) {
    if (event_ticks > 16000) event_ticks = 16000;
    uint32_t f = c->frac + event_ticks * (c->clock_hz >> 3);
    uint32_t n = f / AY_EVENT_HZ;
    c->frac = f - n * AY_EVENT_HZ;
    if (n == 0) return c->last;
    int32_t sum = 0;
    for (uint32_t i = 0; i < n; i++) sum += ay_tick(c);
    c->last = sum / (int32_t)n;
    return c->last;
}

// - - - the cartridges - - - - - - - - - - - - - - - - - - - - - - - - - -

enum { AYB_PSG = 1, AYB_SYM12 = 2, AYB_DELUXE = 4 };   // which front-ends answer

struct ay_bank {
    struct ay8910 chip[AY_CHIPS];
    uint8_t  enabled;            // AYB_* the settings allow
    uint8_t  active;             // bit per chip: written since reset, so rendered
    uint8_t  psg_ctrl;           // CoCo PSG $FF5D
    uint8_t  ora, ddra, cra;     // Symphony-12's PIA
    uint8_t  orb, ddrb, crb;
    uint32_t writes;             // register writes, for telemetry
};

static inline void ayb_reset(struct ay_bank *b) {
    uint8_t en = b->enabled;
    memset(b, 0, sizeof *b);
    b->enabled = en;
    for (int i = 0; i < AY_CHIPS; i++) ay_reset(&b->chip[i], AY_CLK_PSG_2MHZ);
}

static inline void ayb_chip_write(struct ay_bank *b, int k, uint8_t d) {
    ay_write(&b->chip[k], d);
    b->active |= (uint8_t)(1u << k);
    b->writes++;
}

// Symphony-12: chips act on what the PIA drives (outputs only; an input bit
// reads as 0, so a port left as input does nothing).
static inline void ayb_sym12_bus(struct ay_bank *b) {
    uint8_t ctl = b->orb & b->ddrb;
    uint8_t data = b->ora & b->ddra;
    for (int k = 0; k < AY_CHIPS; k++) {
        unsigned f = (ctl >> (2 * k)) & 3;
        if (f == 3) ay_latch(&b->chip[k], data);
        else if (f == 2) ayb_chip_write(b, k, data);
    }
}

static inline uint8_t ayb_sym12_read_a(const struct ay_bank *b) {
    uint8_t ctl = b->orb & b->ddrb, d = 0;
    for (int k = 0; k < AY_CHIPS; k++)
        if (((ctl >> (2 * k)) & 3) == 1) d |= ay_read(&b->chip[k]);
    // Bits driven as outputs read back what the PIA drives.
    return (uint8_t)((d & ~b->ddra) | (b->ora & b->ddra));
}

static inline void ayb_sym12_clock(struct ay_bank *b) {
    for (int k = 0; k < AY_CHIPS; k++) ay_set_clock(&b->chip[k], AY_CLK_SYM12);
}

// True when `a` is an address some enabled front-end answers. One compare
// covers the span, so the bus pays little for addresses outside it.
static inline bool ayb_owns(const struct ay_bank *b, uint16_t a) {
    if ((uint16_t)(a - 0xFF38) > (0xFF63 - 0xFF38) || !b->enabled) return false;
    if (a == 0xFF38 || a == 0xFF39) return b->enabled & AYB_DELUXE;
    if (a >= 0xFF5D && a <= 0xFF5F) return b->enabled & AYB_PSG;
    if (a >= 0xFF60) return b->enabled & AYB_SYM12;
    return false;
}

static inline void ayb_write(struct ay_bank *b, uint16_t a, uint8_t d) {
    switch (a) {
    case 0xFF38: ay_set_clock(&b->chip[0], AY_CLK_DELUXE); ayb_chip_write(b, 0, d); break;
    case 0xFF39: ay_latch(&b->chip[0], d); break;
    case 0xFF5D:
        b->psg_ctrl = d;
        ay_set_clock(&b->chip[0], (d & 1) ? AY_CLK_PSG_1MHZ : AY_CLK_PSG_2MHZ);
        break;
    case 0xFF5E: ay_latch(&b->chip[0], d); break;
    case 0xFF5F:
        ay_set_clock(&b->chip[0], (b->psg_ctrl & 1) ? AY_CLK_PSG_1MHZ : AY_CLK_PSG_2MHZ);
        ayb_chip_write(b, 0, d);
        break;
    case 0xFF60:
        if (b->cra & 4) b->ora = d; else b->ddra = d;
        ayb_sym12_clock(b);
        ayb_sym12_bus(b);
        break;
    case 0xFF61: b->cra = d & 0x3F; break;
    case 0xFF62:
        if (b->crb & 4) b->orb = d; else b->ddrb = d;
        ayb_sym12_clock(b);
        ayb_sym12_bus(b);
        break;
    case 0xFF63: b->crb = d & 0x3F; break;
    default: break;
    }
}

static inline uint8_t ayb_read(const struct ay_bank *b, uint16_t a) {
    switch (a) {
    case 0xFF39: return ay_read(&b->chip[0]);
    case 0xFF5F: return ay_read(&b->chip[0]);
    case 0xFF60: return (b->cra & 4) ? ayb_sym12_read_a(b) : b->ddra;
    case 0xFF61: return b->cra;
    case 0xFF62: return (b->crb & 4) ? (uint8_t)(b->orb & b->ddrb) : b->ddrb;
    case 0xFF63: return b->crb;
    default:     return 0xFF;   // $FF38 and $FF5D are write-only
    }
}

// Every chip in use, mixed: one chip at its own level, more scaled down so
// four at full are 1.6 times one rather than four times. A chip with all
// three levels at 0 is silent whatever its tones do, so it is not run.
static inline int32_t ayb_render(struct ay_bank *b, uint32_t event_ticks) {
    int32_t sum = 0;
    int n = 0;
    for (int k = 0; k < AY_CHIPS; k++) {
        if (!(b->active & (1u << k))) continue;
        const uint8_t *r = b->chip[k].reg;
        if (!(r[8] | r[9] | r[10])) continue;
        sum += ay_render(&b->chip[k], event_ticks);
        n++;
    }
    return n <= 1 ? sum : sum * 2 / (n + 1);
}

#endif  // PSG_AY8910_H
