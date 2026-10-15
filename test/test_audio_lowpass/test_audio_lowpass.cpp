// PIZERO-212: host tests for the low-pass filter on the sound output.
//
//   pio test -e native -f test_audio_lowpass

#include <unity.h>
#include <math.h>

#include "../../lib/coco_machine/src/audio_lowpass.h"

void setUp(void) {}
void tearDown(void) {}

#define RATE 48000u

// The gain of a sine at hz, in dB, measured after the filter has settled.
static double gain_db(uint32_t cutoff, double hz, int32_t amp) {
    struct audio_lp f;
    audio_lp_set(&f, cutoff, RATE);
    double in = 0, out = 0;
    for (int i = 0; i < 4 * (int)RATE; i++) {
        int32_t x = (int32_t)lround(amp * sin(2 * M_PI * hz * i / RATE));
        int32_t y = audio_lp_run(&f, x);
        if (i >= (int)RATE) { in += (double)x * x; out += (double)y * y; }
    }
    return 10 * log10(out / in);
}

// Off: every sample passes unchanged.
static void test_off_is_identity(void) {
    struct audio_lp f;
    audio_lp_set(&f, 0, RATE);
    for (int32_t x = -32768; x <= 32767; x += 97)
        TEST_ASSERT_EQUAL_INT32(x, audio_lp_run(&f, x));
    audio_lp_set(&f, 30000, RATE);              // above Nyquist: off too
    TEST_ASSERT_EQUAL_INT32(1234, audio_lp_run(&f, 1234));
}

// A held level comes out exactly, for every cutoff in range, both signs.
static void test_step_settles_exactly(void) {
    static const uint32_t cuts[] = { 1000, 3000, 8000, 12000, 20000 };
    static const int32_t levels[] = { 1920, -1920, 1, -1, 2880, -32768, 32767, 0 };
    for (unsigned c = 0; c < sizeof cuts / sizeof cuts[0]; c++) {
        struct audio_lp f;
        audio_lp_set(&f, cuts[c], RATE);
        for (unsigned l = 0; l < sizeof levels / sizeof levels[0]; l++) {
            int32_t y = 0;
            for (int i = 0; i < 4800; i++) y = audio_lp_run(&f, levels[l]);
            TEST_ASSERT_EQUAL_INT32(levels[l], y);
            for (int i = 0; i < 100; i++)        // and stays there
                TEST_ASSERT_EQUAL_INT32(levels[l], audio_lp_run(&f, levels[l]));
        }
    }
}

// The Butterworth shape at 8 kHz: flat in the passband, -3 dB at the
// cutoff, and falling 12 dB an octave above it.
static void test_response_at_8k(void) {
    TEST_ASSERT_TRUE(fabs(gain_db(8000, 1000, 10000)) < 0.1);
    TEST_ASSERT_TRUE(fabs(gain_db(8000, 8000, 10000) + 3.0) < 0.2);
    TEST_ASSERT_TRUE(gain_db(8000, 16000, 10000) < -12.0);
    TEST_ASSERT_TRUE(gain_db(8000, 20000, 10000) < -20.0);
}

// The cutoff follows the setting.
static void test_cutoff_moves(void) {
    TEST_ASSERT_TRUE(fabs(gain_db(3000, 3000, 10000) + 3.0) < 0.2);
    TEST_ASSERT_TRUE(fabs(gain_db(12000, 12000, 10000) + 3.0) < 0.2);
    TEST_ASSERT_TRUE(gain_db(3000, 8000, 10000) < gain_db(12000, 8000, 10000) - 10.0);
}

// A full-scale square wave, at the lowest and highest cutoffs: no overflow.
// Band-limiting a square overshoots its edges (by about a third near 20 kHz);
// audio_emit clamps after the filter. The machine's own levels are far below
// full scale (about +-2,900 at volume 15), so this is only a safety check.
static void test_full_scale_square_is_safe(void) {
    static const uint32_t cuts[] = { 1000, 20000 };
    for (unsigned c = 0; c < 2; c++) {
        struct audio_lp f;
        audio_lp_set(&f, cuts[c], RATE);
        int32_t lo = 0, hi = 0;
        for (int i = 0; i < 48000; i++) {
            int32_t x = ((i / 24) & 1) ? 32767 : -32768;
            int32_t y = audio_lp_run(&f, x);
            if (y < lo) lo = y;
            if (y > hi) hi = y;
        }
        TEST_ASSERT_TRUE(hi > 30000 && hi < 32767 * 3 / 2);
        TEST_ASSERT_TRUE(lo < -30000 && lo > -32768 * 3 / 2);
    }
}

// PIZERO-223: the DC blocker takes a held level to exactly 0 and passes a
// tone at full strength.
static void test_dc_blocker(void) {
    struct audio_dc f = {0, 0};
    int32_t y = 0;
    for (int i = 0; i < 48000; i++) y = audio_dc_run(&f, -5760);
    TEST_ASSERT_EQUAL_INT32(0, y);
    struct audio_dc g = {0, 0};
    int32_t peak = 0;
    for (int i = 0; i < 48000; i++) {
        int32_t x = (int32_t)lround(10000.0 * sin(2 * M_PI * 440.0 * i / 48000.0)) + 3000;
        int32_t o = audio_dc_run(&g, x);
        if (i > 24000 && abs(o) > peak) peak = abs(o);
    }
    TEST_ASSERT_INT32_WITHIN(60, 10000, peak);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_off_is_identity);
    RUN_TEST(test_step_settles_exactly);
    RUN_TEST(test_response_at_8k);
    RUN_TEST(test_cutoff_moves);
    RUN_TEST(test_full_scale_square_is_safe);
    RUN_TEST(test_dc_blocker);
    return UNITY_END();
}
