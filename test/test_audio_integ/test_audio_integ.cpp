// PIZERO-131: host tests for the audio integrator's skip-ahead.
//
// Step (c) stops calling the integrator on every bus access: accesses that
// cross no sample boundary are added later as one span. These tests run the
// old way (audio_integ_step on every access) and the new way (spans up to the
// access that crosses, worked out in advance) over the same random streams,
// and require every sample and the final state to match exactly.
//
//   pio test -e native

#include <unity.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../lib/coco_machine/src/audio_integ.h"

void setUp(void) {}
void tearDown(void) {}

struct sample { int s; uint32_t tk; };

#define MAX_SAMPLES 200000
static struct sample g_old[MAX_SAMPLES], g_new[MAX_SAMPLES];

static uint32_t rnd(void) {           // fixed sequence, so a failure repeats
    static uint32_t x = 0x9E3779B9u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return x;
}

// One bus access in the stream: its length, and the DAC level from it on.
struct access { uint32_t ticks; int32_t level; uint8_t new_run; };

// The machine's way: plain 16-tick accesses before the deadline only add
// up; the tail (span, then this access stepped) runs for the access that
// crosses a boundary, for a level change or an odd length (both through
// coco_mem_cycle), and at the end of each run, when the servo may change
// the rate.
static unsigned run_new(const struct access *a, unsigned n, const uint32_t *rates,
                        struct audio_integ *out) {
    struct audio_integ st = {0, 0, 0};
    unsigned ns = 0, run = 0;
    uint32_t rate = rates[0];
    int32_t level = 0;
    uint32_t pend = 0, left = audio_integ_accesses_to_boundary(&st, rate, 16);
    for (unsigned i = 0; i < n; i++) {
        if (a[i].new_run) {                 // run boundary: catch up, servo step
            audio_integ_span(&st, level, pend, rate);
            pend = 0;
            rate = rates[++run];
            left = audio_integ_accesses_to_boundary(&st, rate, 16);
        }
        bool slow = a[i].ticks != 16 || a[i].level != level || left == 1;
        if (!slow) { pend += 16; left--; continue; }
        audio_integ_span(&st, level, pend, rate);
        pend = 0;
        level = a[i].level;
        int s; uint32_t tk;
        if (audio_integ_step(&st, level, a[i].ticks, rate, &s, &tk))
            g_new[ns++] = (struct sample){s, tk};
        left = audio_integ_accesses_to_boundary(&st, rate, 16);
    }
    audio_integ_span(&st, level, pend, rate);
    *out = st;
    return ns;
}

// PIZERO-220: the machine's way since then. Plain 16-tick accesses only
// add up until an event (here every 20-90 accesses), whatever sample
// boundaries they cross; the catch-up then runs them all with
// audio_integ_run, which emits every sample they crossed.
static unsigned g_run_ns;
static void run_emit(int s, uint32_t tk) { g_new[g_run_ns++] = (struct sample){s, tk}; }

static unsigned run_events(const struct access *a, unsigned n, const uint32_t *rates,
                           struct audio_integ *out) {
    struct audio_integ st = {0, 0, 0};
    unsigned run = 0;
    uint32_t rate = rates[0];
    int32_t level = 0;
    uint32_t pend = 0, left = 20 + rnd() % 70;
    g_run_ns = 0;
    for (unsigned i = 0; i < n; i++) {
        if (a[i].new_run) {
            audio_integ_run(&st, level, pend / 16, 16, rate, run_emit);
            pend = 0;
            rate = rates[++run];
        }
        bool slow = a[i].ticks != 16 || a[i].level != level || --left == 0;
        if (!slow) { pend += 16; continue; }
        audio_integ_run(&st, level, pend / 16, 16, rate, run_emit);
        pend = 0;
        level = a[i].level;
        int s; uint32_t tk;
        if (audio_integ_step(&st, level, a[i].ticks, rate, &s, &tk))
            g_new[g_run_ns++] = (struct sample){s, tk};
        if (!left) left = 20 + rnd() % 70;
    }
    audio_integ_run(&st, level, pend / 16, 16, rate, run_emit);
    *out = st;
    return g_run_ns;
}

static unsigned run_old(const struct access *a, unsigned n, const uint32_t *rates,
                        struct audio_integ *out) {
    struct audio_integ st = {0, 0, 0};
    unsigned ns = 0, run = 0;
    uint32_t rate = rates[0];
    for (unsigned i = 0; i < n; i++) {
        if (a[i].new_run) rate = rates[++run];
        int s; uint32_t tk;
        if (audio_integ_step(&st, a[i].level, a[i].ticks, rate, &s, &tk))
            g_old[ns++] = (struct sample){s, tk};
    }
    *out = st;
    return ns;
}

static void compare_streams(unsigned n, unsigned level_every, unsigned odd_every, int events = 0) {
    static struct access a[1000000];
    static uint32_t rates[2000];
    unsigned runs = 0;
    rates[0] = 48000;
    int32_t level = 0;
    for (unsigned i = 0; i < n; i++) {
        a[i].ticks = (odd_every && rnd() % odd_every == 0) ? 8 + 8 * (rnd() % 3) : 16;
        if (level_every && rnd() % level_every == 0)
            level = (int32_t)(rnd() % 64) * 60 - 1920;   // the 6-bit DAC at volume 10
        a[i].level = level;
        a[i].new_run = (i && i % 14915 == 0);            // one frame of cycles
        if (a[i].new_run) rates[++runs] = 47900 + rnd() % 200;   // the servo's range
    }
    struct audio_integ so, sn;
    unsigned no = run_old(a, n, rates, &so);
    unsigned nn = events ? run_events(a, n, rates, &sn) : run_new(a, n, rates, &sn);
    TEST_ASSERT_TRUE(no > 0);
    TEST_ASSERT_EQUAL_UINT32(no, nn);
    for (unsigned i = 0; i < no; i++) {
        if (g_old[i].s != g_new[i].s || g_old[i].tk != g_new[i].tk) {
            char msg[96];
            snprintf(msg, sizeof msg, "sample %u: old %d/%u new %d/%u",
                     i, g_old[i].s, (unsigned)g_old[i].tk, g_new[i].s, (unsigned)g_new[i].tk);
            TEST_FAIL_MESSAGE(msg);
        }
    }
    TEST_ASSERT_EQUAL_INT32(so.acc, sn.acc);
    TEST_ASSERT_EQUAL_UINT32(so.tk, sn.tk);
    TEST_ASSERT_EQUAL_UINT32(so.terr, sn.terr);
}

// A held level (silence or a held DAC): only the boundaries matter.
static void test_constant_level_matches(void) { compare_streams(300000, 0, 0); }

// A tone: the level changes every few dozen accesses.
static void test_changing_level_matches(void) { compare_streams(600000, 40, 0); }

// Busy sound with odd-length accesses mixed in (SAM register writes).
static void test_odd_accesses_match(void) { compare_streams(600000, 7, 50); }

// PIZERO-220: the same three streams caught up only at events.
static void test_events_constant_level_matches(void) { compare_streams(300000, 0, 0, 1); }
static void test_events_changing_level_matches(void) { compare_streams(600000, 40, 0, 1); }
static void test_events_odd_accesses_match(void) { compare_streams(600000, 7, 50, 1); }

// The access the helper names crosses a boundary and none before it does.
static void test_accesses_to_boundary_is_exact(void) {
    for (unsigned k = 0; k < 20000; k++) {
        uint32_t rate = 47900 + rnd() % 200;
        struct audio_integ st = {0, 0, rnd() % AUDIO_TICK_HZ};
        uint32_t n = audio_integ_accesses_to_boundary(&st, rate, 16);
        TEST_ASSERT_TRUE(n >= 1);
        int s; uint32_t tk;
        for (uint32_t i = 1; i < n; i++)
            TEST_ASSERT_FALSE(audio_integ_step(&st, 100, 16, rate, &s, &tk));
        TEST_ASSERT_TRUE(audio_integ_step(&st, 100, 16, rate, &s, &tk));
    }
}

// PIZERO-132: a CoCo 2 at address-dependent speed (POKE 65495) mixes 8 and
// 16 tick cycles, and the machine catches each run of them up as 8 tick
// steps. At one level that must give exactly the samples of stepping every
// cycle at its own length.
static void test_mixed_lengths_caught_up_in_eights(void) {
    static struct access a[600000];
    const unsigned n = 600000;
    const uint32_t rates[1] = {48000};
    int32_t level = 0;
    for (unsigned i = 0; i < n; i++) {
        a[i].ticks = (rnd() & 1) ? 8 : 16;
        if (rnd() % 40 == 0) level = (int32_t)(rnd() % 64) * 60 - 1920;
        a[i].level = level;
        a[i].new_run = 0;
    }
    struct audio_integ so, sn = {0, 0, 0};
    unsigned no = run_old(a, n, rates, &so);
    g_run_ns = 0;
    uint32_t pend = 0;
    int32_t held = a[0].level;
    for (unsigned i = 0; i < n; i++) {
        if (a[i].level != held || rnd() % 300 == 0) {   // a level change or an event
            audio_integ_run(&sn, held, pend / 8, 8, rates[0], run_emit);
            pend = 0;
            held = a[i].level;
        }
        pend += a[i].ticks;
    }
    audio_integ_run(&sn, held, pend / 8, 8, rates[0], run_emit);
    TEST_ASSERT_TRUE(no > 0);
    TEST_ASSERT_EQUAL_UINT32(no, g_run_ns);
    for (unsigned i = 0; i < no; i++) {
        TEST_ASSERT_EQUAL_INT(g_old[i].s, g_new[i].s);
        TEST_ASSERT_EQUAL_UINT32(g_old[i].tk, g_new[i].tk);
    }
    TEST_ASSERT_EQUAL_INT32(so.acc, sn.acc);
    TEST_ASSERT_EQUAL_UINT32(so.tk, sn.tk);
    TEST_ASSERT_EQUAL_UINT32(so.terr, sn.terr);
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_constant_level_matches);
    RUN_TEST(test_changing_level_matches);
    RUN_TEST(test_odd_accesses_match);
    RUN_TEST(test_accesses_to_boundary_is_exact);
    RUN_TEST(test_events_constant_level_matches);
    RUN_TEST(test_events_changing_level_matches);
    RUN_TEST(test_events_odd_accesses_match);
    RUN_TEST(test_mixed_lengths_caught_up_in_eights);
    return UNITY_END();
}
