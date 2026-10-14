// audio_integ.h: the DAC-to-sample integrator (PIZERO-18), host-tested.
//
// The DAC level is piecewise constant between PIA1 writes. Each bus access
// adds level x ticks to the current sample's integral; a Bresenham
// accumulator (ticks x rate against the tick clock) places the sample
// boundaries, and an access that crosses one is split exactly at it, so each
// sample is the true time-average over its period.
//
// PIZERO-131 step (c) stops integrating every access one by one: accesses
// that cross no boundary are added as one span, and the machine works out in
// advance how many accesses it can skip. audio_integ_span must therefore give
// exactly what audio_integ_step would have, access by access; the host test
// (test/test_audio_integ) checks that sample for sample.

#ifndef AUDIO_INTEG_H
#define AUDIO_INTEG_H

#include <stdint.h>

#define AUDIO_TICK_HZ 14318180u   // event ticks per second (16 per CPU cycle)

struct audio_integ {
    int32_t  acc;    // integral of level x ticks this sample
    uint32_t tk;     // ticks accumulated this sample
    uint32_t terr;   // Bresenham accumulator (ticks x rate), below AUDIO_TICK_HZ
};

// One access of `ticks` at `level`. Returns 1 when a sample boundary falls
// inside it, with the finished sample's average in *s and its length in
// ticks in *tk (the ticks past the boundary carry into the next sample).
static inline int audio_integ_step(struct audio_integ *a, int32_t level, uint32_t ticks,
                                   uint32_t rate, int *s, uint32_t *tk) {
    a->acc  += level * (int32_t)ticks;
    a->tk   += ticks;
    a->terr += ticks * rate;
    if (a->terr < AUDIO_TICK_HZ) return 0;
    a->terr -= AUDIO_TICK_HZ;
    uint32_t over_tk  = a->terr / rate;            // ticks of this access past the boundary
    int32_t  over_val = level * (int32_t)over_tk;  // ...belong to the next sample
    int32_t  acc = a->acc - over_val;
    uint32_t len = a->tk - over_tk;
    *s  = len ? (int)(acc / (int32_t)len) : (int)level;
    *tk = len;
    a->acc = over_val;
    a->tk  = over_tk;
    return 1;
}

// `ticks` of accesses at `level` that cross no boundary between them (the
// caller guarantees it with audio_integ_accesses_to_boundary).
static inline void audio_integ_span(struct audio_integ *a, int32_t level, uint32_t ticks,
                                    uint32_t rate) {
    a->acc  += level * (int32_t)ticks;
    a->tk   += ticks;
    a->terr += ticks * rate;
}

// How many accesses of `access_ticks` from now until one crosses a sample
// boundary: the returned one (at least 1) crosses, the ones before it do not.
static inline uint32_t audio_integ_accesses_to_boundary(const struct audio_integ *a,
                                                        uint32_t rate, uint32_t access_ticks) {
    uint32_t step = access_ticks * rate;
    return (AUDIO_TICK_HZ - a->terr + step - 1) / step;
}

// PIZERO-220: `n` accesses of `access_ticks` at `level` that may cross any
// number of boundaries: spans up to each access that crosses, which is
// stepped, so the samples (handed to emit with their length in ticks) and
// the state are exactly those of n calls of audio_integ_step. This lets the
// machine catch the audio up only at events, not at every sample boundary.
typedef void (*audio_integ_emit_fn)(int s, uint32_t tk);

static inline __attribute__((always_inline)) void audio_integ_run(
        struct audio_integ *a, int32_t level, uint32_t n, uint32_t access_ticks,
        uint32_t rate, audio_integ_emit_fn emit) {
    while (n) {
        uint32_t k = audio_integ_accesses_to_boundary(a, rate, access_ticks);
        if (k > n) {
            audio_integ_span(a, level, n * access_ticks, rate);
            return;
        }
        audio_integ_span(a, level, (k - 1) * access_ticks, rate);
        int s; uint32_t tk;
        audio_integ_step(a, level, access_ticks, rate, &s, &tk);
        emit(s, tk);
        n -= k;
    }
}

#endif
