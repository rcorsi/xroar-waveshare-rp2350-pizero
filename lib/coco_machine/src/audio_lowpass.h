// audio_lowpass.h: the low-pass filter on the sound output (PIZERO-212),
// host-tested.
//
// The 6-bit DAC moves in hard steps, which gives the sound a harsh top end;
// the real CoCo's output went through analog filtering on its way to the TV.
// This is a two-pole Butterworth low-pass (the RBJ cookbook biquad, Q = 1/sqrt 2)
// on the finished 48 kHz samples, so it runs once a sample, not once a cycle:
// five multiplies, well under 1% of the CPU. A one-pole was not enough: it
// falls only 6 dB an octave, so it barely touches the buzz above 8 kHz.
//
// Integer maths: coefficients in Q28, products summed in 64 bits. The
// outputs it feeds back are kept with 8 more bits than the samples (Q8), and
// the bits the shift drops are carried into the next sample (error
// feedback). Without the extra bits a cutoff near 20 kHz rings by a couple of
// steps forever on a held level; with them a held level comes out exactly.
//
// The coefficients are worked out once, in floating point, when the setting
// is applied (audio_lp_set); audio_lp_run is all integer.

#ifndef AUDIO_LOWPASS_H
#define AUDIO_LOWPASS_H

#include <math.h>
#include <stdint.h>

#define AUDIO_LP_Q      28
#define AUDIO_LP_MIN_HZ 1000u      // the audio_lowpass setting's range
#define AUDIO_LP_MAX_HZ 20000u

struct audio_lp {
    int32_t b0, b1, a1, a2;        // Q28; b2 equals b0 for a low-pass
    int32_t x1, x2;                // the last two inputs
    int32_t y1, y2;                // the last two outputs, Q8
    int64_t err;                   // what the last shift dropped, below 1 << Q
    uint32_t hz;                   // the cutoff, 0 when off
};

// Set the cutoff (0 turns the filter off) for samples at rate_hz. The
// history is cleared, so a change cannot ring with stale state.
static inline void audio_lp_set(struct audio_lp *f, uint32_t hz, uint32_t rate_hz) {
    f->x1 = f->x2 = f->y1 = f->y2 = 0;
    f->err = 0;
    if (hz == 0 || hz * 2 >= rate_hz) { f->hz = 0; return; }
    f->hz = hz;
    double w0 = 2.0 * M_PI * hz / rate_hz;
    double c = cos(w0), alpha = sin(w0) / (2.0 * 0.70710678118654752);
    double a0 = 1.0 + alpha, one = (double)(1 << AUDIO_LP_Q);
    f->b0 = (int32_t)lround((1.0 - c) / 2.0 / a0 * one);
    f->a1 = (int32_t)lround(-2.0 * c / a0 * one);
    f->a2 = (int32_t)lround((1.0 - alpha) / a0 * one);
    // b1 from the others, not rounded on its own, so the gain at DC is
    // exactly 1 and a held level comes out unchanged.
    f->b1 = (int32_t)((1 << AUDIO_LP_Q) + f->a1 + f->a2 - 2 * f->b0);
}

// One sample through the filter (or straight through when it is off).
static inline int32_t audio_lp_run(struct audio_lp *f, int32_t x) {
    if (!f->hz) return x;
    int64_t acc = f->err
                + (((int64_t)f->b0 * (x + f->x2) + (int64_t)f->b1 * f->x1) << 8)
                - (int64_t)f->a1 * f->y1 - (int64_t)f->a2 * f->y2;
    int32_t y = (int32_t)(acc >> AUDIO_LP_Q);              // Q8, floor
    f->err = acc - ((int64_t)y << AUDIO_LP_Q);
    f->x2 = f->x1; f->x1 = x;
    f->y2 = f->y1; f->y1 = y;
    return (y + 128) >> 8;                                 // nearest sample value
}

#endif
