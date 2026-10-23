// coco_sound.h: the CoCo's analog sound path (PIZERO-253), host-tested.
//
// On the real machine the sound reaching the TV is picked by a multiplexer:
// PIA0 CA2 and CB2 select one of four sources, 0 the 6-bit DAC, 1 the
// cassette input, 2 the cartridge's sound line, 3 nothing; PIA1 CB2 is the
// sound enable. The same two select lines pick the joystick axis for the
// comparator, which is why reading a joystick switches the sound source.
// The single-bit output, PIA1 PB1, counts only while it is an output; it
// shifts every source's gain and offset. With the multiplexer off, a
// single-bit output still drives the line, and with neither the line holds
// its last level.
//
// The levels are XRoar's (~/github/xroar/src/sound.c), measured on a real
// Dragon, whose sound section is the CoCo's, in millivolts. The cassette
// input carries nothing here (no tape is played), so it gives only its
// offset. Color BASIC leaves PB1 an input and the sound enable off, and
// SOUND and PLAY turn the enable on, so in normal use the DAC has the gain
// in column 0.
//
// SND_ALL is the CoCo Zero's way: every source is heard at once, as if the
// multiplexer passed the DAC and the cartridge line together and the
// enable were always on. The single-bit output still shapes the DAC.

#ifndef COCO_SOUND_H
#define COCO_SOUND_H

#include <stdbool.h>
#include <stdint.h>

enum { SND_SRC_DAC = 0, SND_SRC_TAPE = 1, SND_SRC_CART = 2, SND_SRC_NONE = 3 };
enum { SND_SWITCHED = 0, SND_ALL = 1 };   // the sound_sources setting

// [source][single-bit: 0 not an output, 1 output low, 2 output high]
static const int16_t SND_GAIN_MV[4][3] = {
    { 4500, 2840, 3400 },        // DAC
    {  500,  400,  500 },        // cassette
    { 4700, 2840, 3400 },        // cartridge
    {    0,    0,    0 },        // nothing
};
static const int16_t SND_OFFSET_MV[4][3] = {
    {  200,  180, 1300 },
    { 2050, 1600, 2350 },
    {    0,  180, 1300 },
    {    0,    0,    0 },
};
static const int16_t SND_SBS_ONLY_MV[3] = { 0, 0, 3900 };   // multiplexer off
#define SND_CART_FULL_MV 4700    // the cartridge line's gain that is 1.0

// What the PIAs set, as the sound path sees it.
struct snd_inputs {
    uint8_t dac6;                // PIA1 PA2-7, 0-63
    bool    sbs_output;          // PIA1 PB1 is an output
    bool    sbs_high;            // ...and drives high
    bool    enable;              // PIA1 CB2
    uint8_t source;              // PIA0 CB2 << 1 | CA2
};

// The line's level in millivolts, without the cartridge's own sound (that
// is added by the caller, times *cart_q10 / 1024). `held_mv` is the level
// the line had, which it keeps when nothing drives it.
static inline int32_t snd_line_mv(const struct snd_inputs *in, int mode,
                                  int32_t held_mv, int32_t *cart_q10) {
    unsigned si = in->sbs_output ? (in->sbs_high ? 2u : 1u) : 0u;
    bool en = mode == SND_ALL || in->enable;
    unsigned src = mode == SND_ALL ? (unsigned)SND_SRC_DAC : (in->source & 3u);
    *cart_q10 = mode == SND_ALL ? 1024
              : (en && src == SND_SRC_CART) ? SND_GAIN_MV[SND_SRC_CART][si] * 1024 / SND_CART_FULL_MV
              : 0;
    if (en) {
        int32_t mv = SND_OFFSET_MV[src][si];
        if (src == SND_SRC_DAC) mv += SND_GAIN_MV[SND_SRC_DAC][si] * in->dac6 / 63;
        return mv;
    }
    if (in->sbs_output) return SND_SBS_ONLY_MV[si];
    return held_mv;
}

// joystick_quiet. A program reads a joystick by writing trial values to
// the DAC and testing the comparator (PIA0 PA7) after each; with the sound
// on, the trials are heard as a buzz (Double Back does this). A DAC write
// followed within SND_PROBE_TICKS by a read of PIA0 port A is taken to be a
// trial, and its sound is taken back from the write on, as if never heard.
// The comparator still sees the value written. 256 ticks is 16 CPU cycles
// at 0.89 MHz; a trial is a store and a test, about 12.
//
// The span lies in the sample being integrated and, past its start, in the
// one before, which the caller keeps back one sample for this: a sample is
// about 298 ticks, longer than the window.
#define SND_PROBE_TICKS 256u

struct snd_probe {
    bool     armed;
    uint32_t tick;               // the write's cycle
    int32_t  level;              // the level heard before it
};

// The level changed at `tick` from `before`; a trial if a read follows.
static inline void snd_probe_write(struct snd_probe *p, uint32_t tick, int32_t before) {
    p->armed = true;
    p->tick = tick;
    p->level = before;
}

// A read of the comparator at `now`, with the audio integrated up to it at
// `level` since the write. Takes the trial's sound back from the sample in
// progress (*acc, cur_tk ticks into it) and the one kept back (*held_l,
// *held_r, held_tk long; held false if there is none) and returns true, the
// caller then restoring the level to p->level. False if it was no trial.
static inline bool snd_probe_read(struct snd_probe *p, uint32_t now, int32_t level,
                                  int32_t *acc, uint32_t cur_tk,
                                  bool held, int32_t *held_l, int32_t *held_r, uint32_t held_tk) {
    if (!p->armed) return false;
    p->armed = false;
    uint32_t span = now - p->tick;
    if (span > SND_PROBE_TICKS) return false;
    int32_t delta = p->level - level;
    uint32_t in_cur = span < cur_tk ? span : cur_tk;
    *acc += delta * (int32_t)in_cur;
    uint32_t rest = span - in_cur;
    if (rest && held && held_tk) {
        int32_t d = delta * (int32_t)rest / (int32_t)held_tk;
        *held_l += d;
        *held_r += d;
    }
    return true;
}

#endif  // COCO_SOUND_H
