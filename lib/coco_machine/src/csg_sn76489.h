// csg_sn76489.h: the TI SN76489 sound chip on the Games Master Cartridge
// (PIZERO-143). Pure integer logic, host-tested.
//
// Behaviour follows XRoar's model (~/github/xroar/src/sn76489.c), which is
// built from the SN76489AN data sheet and SMS Power's measurements, at the
// 4 MHz clock XRoar gives the GMC:
//   * the chip steps at 4 MHz / 16 = 250 kHz ("reference ticks")
//   * 3 tone channels: a counter reloads from a 10-bit N (0 means 1024) and
//     the square output toggles each time it runs out, so f = 125 kHz / N
//   * 1 noise channel: a 15-bit shift register, reset to $4000 by every write
//     to the noise register, stepped when its clock goes high. White noise
//     feeds back the parity of bits 0-1; periodic noise feeds back bit 0,
//     giving a 15-step pulse. The clock is a counter reloading 16, 32 or 64,
//     or tone channel 3's output (rate 3), which makes the noise tunable
//   * 4-bit attenuation per channel, 2 dB a step, 15 = off
//   * every channel adds zero or a positive level; the four are summed
//   * write protocol: a byte with bit 7 set latches channel and register
//     (1 c c t d d d d) and sets the low 4 bits; a byte with bit 7 clear sets
//     a tone's upper 6 bits (0 x d d d d d d), or a volume's whole 4 bits
//   * a write less than 32 chip clocks after the last one is ignored
// It starts silent (all attenuation 15), where XRoar mimics the real chip's
// half-random power-on noise; every program sets the chip up before use.
//
// Output is averaged over each audio sample, the same integrate-and-average
// the DAC path uses, so a tone above the sample rate does not alias.

#ifndef CSG_SN76489_H
#define CSG_SN76489_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

// PIZERO-248: in RAM on the board, as psg_ay8910.h explains.
#ifndef SOUND_HOT
#define SOUND_HOT
#define SOUND_TABLE
#endif

#define SN_REF_HZ       250000u     // 4 MHz clock / 16
#define SN_EVENT_HZ     14318180u   // the machine's event ticks per second

// The shortest gap between accepted writes: 32 chip clocks, in event ticks
// (32 * 14.318 MHz / 4 MHz, rounded up), about 7 CPU cycles.
#define SN_READY_TICKS  115u

// Level of one channel at each attenuation, 10^(-i/10) of full scale: 2 dB
// per step. Full scale per channel is a quarter of the DAC's swing, so all
// four at full volume match the DAC's loudest, as in XRoar's mix.
static const uint16_t SN_LEVEL[16] SOUND_TABLE = {   // in RAM on the board (PIZERO-248)
    945, 751, 596, 474, 376, 299, 237, 188,
    150, 119,  95,  75,  60,  47,  38,   0
};

struct sn76489 {
    uint16_t freq[4];        // tone reload N (1-1024); [3] = noise reload
    uint16_t count[4];
    uint8_t  state[4];       // current output bit per channel
    uint8_t  att[4];         // 0-15
    uint8_t  reg_sel;        // latched register: channel * 2 + (1 = volume)
    uint16_t reg_val[8];     // raw register values, as XRoar keeps them
    bool     noise_white;
    bool     noise_tone3;
    bool     nstate;         // the independent noise clock
    uint16_t lfsr;
    uint32_t frac;           // event ticks not yet turned into chip ticks
    int32_t  last;           // last output level
};

static inline void sn_reset(struct sn76489 *c) {
    memset(c, 0, sizeof *c);
    for (int i = 0; i < 4; i++) {
        c->freq[i] = 0x400;
        c->count[i] = 1;
        c->att[i] = 15;
        c->reg_val[i * 2 + 1] = 15;
    }
    c->freq[3] = 0x10;
    c->lfsr = 0x4000;
}

static inline void sn_update_reg(struct sn76489 *c, unsigned sel, unsigned val) {
    c->reg_val[sel] = (uint16_t)val;
    unsigned ch = sel >> 1;
    if (sel & 1) {
        c->att[ch] = (uint8_t)(val & 0x0F);
    } else if (ch < 3) {
        c->freq[ch] = (uint16_t)(val ? val : 0x400);
    } else {
        c->noise_white = (val & 0x04) != 0;
        c->noise_tone3 = (val & 3) == 3;
        if ((val & 3) != 3) c->freq[3] = (uint16_t)(0x10u << (val & 3));
        c->lfsr = 0x4000;                    // always reset the shift register
    }
}

// One byte written to the chip.
static inline void sn_write(struct sn76489 *c, uint8_t d) {
    unsigned sel, mask, val;
    if (!(d & 0x80)) {                       // data byte: the latched register
        sel = c->reg_sel;
        if (!(sel & 1)) { mask = 0x000F; val = (unsigned)(d & 0x3F) << 4; }
        else            { mask = 0;      val = d & 0x0F; }
    } else {                                 // latch byte: select + low 4 bits
        sel = c->reg_sel = (d >> 4) & 7;
        mask = 0x03F0;
        val = d & 0x0F;
    }
    sn_update_reg(c, sel, (c->reg_val[sel] & mask) | val);
}

static inline bool sn_ready(uint32_t last_write_tick, uint32_t now_tick) {
    return (uint32_t)(now_tick - last_write_tick) >= SN_READY_TICKS;
}

// The summed output level as the chip stands.
static inline SOUND_HOT int32_t sn_mix(const struct sn76489 *c) {
    int32_t out = 0;
    for (int ch = 0; ch < 4; ch++)
        if (c->state[ch]) out += SN_LEVEL[c->att[ch]];
    return out;
}

// Advance one chip tick (250 kHz) and return the summed output level.
static inline SOUND_HOT int32_t sn_tick(struct sn76489 *c) {
    bool noise_clock = false;
    for (int ch = 0; ch < 3; ch++) {
        if (--c->count[ch] == 0) {
            c->state[ch] ^= 1;
            c->count[ch] = c->freq[ch];
            if (ch == 2 && c->noise_tone3) noise_clock = c->state[2];
        }
    }
    if (!c->noise_tone3 && --c->count[3] == 0) {
        c->nstate = !c->nstate;
        c->count[3] = c->freq[3];
        noise_clock = c->nstate;
    }
    if (noise_clock) {                       // rising edge steps the register
        unsigned fb = c->noise_white ? ((c->lfsr ^ (c->lfsr >> 1)) & 1) : (c->lfsr & 1);
        c->lfsr = (uint16_t)((c->lfsr >> 1) | (fb << 14));
        c->state[3] = c->lfsr & 1;
    }
    return sn_mix(c);
}

// PIZERO-248: the ticks until a counter runs out (a tone edge, or the noise
// clock's), at least 1. Nothing changes before then, so ticks short of it
// are taken in one go.
static inline SOUND_HOT uint32_t sn_quiet_ticks(const struct sn76489 *c) {
    uint32_t q = c->count[0];
    if (c->count[1] < q) q = c->count[1];
    if (c->count[2] < q) q = c->count[2];
    if (!c->noise_tone3 && c->count[3] < q) q = c->count[3];
    return q;
}

static inline SOUND_HOT void sn_skip(struct sn76489 *c, uint32_t k) {
    for (int ch = 0; ch < 3; ch++) c->count[ch] = (uint16_t)(c->count[ch] - k);
    if (!c->noise_tone3) c->count[3] = (uint16_t)(c->count[3] - k);
}

// The chip's average output over `event_ticks` of machine time: one audio
// sample's worth. Averaging, not point-sampling, keeps high tones from
// aliasing, as the DAC path does. PIZERO-248: 32-bit arithmetic, as
// ay_render: a 64-bit divide is a library call (in flash) on the RP2350.
// 16000 ticks * 250 kHz plus the carry stays under 2^32.
static inline SOUND_HOT int32_t sn_render(struct sn76489 *c, uint32_t event_ticks) {
    if (event_ticks > 16000) event_ticks = 16000;
    uint32_t f = c->frac + event_ticks * SN_REF_HZ;
    uint32_t n = f / SN_EVENT_HZ;
    c->frac = f - n * SN_EVENT_HZ;
    if (n == 0) return c->last;
    int32_t sum = 0, out = sn_mix(c);
    for (uint32_t left = n;;) {                  // PIZERO-248: quiet ticks in one go
        uint32_t q = sn_quiet_ticks(c);
        if (q > left) { sn_skip(c, left); sum += out * (int32_t)left; break; }
        sn_skip(c, q - 1);
        sum += out * (int32_t)(q - 1);
        out = sn_tick(c);
        sum += out;
        if ((left -= q) == 0) break;
    }
    c->last = sum / (int32_t)n;
    return c->last;
}

#endif  // CSG_SN76489_H
