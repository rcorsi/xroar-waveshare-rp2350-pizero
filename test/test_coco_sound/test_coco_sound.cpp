// PIZERO-253: the CoCo's sound path (coco_sound.h): what the multiplexer,
// the sound enable and the single-bit output make of the line, and
// joystick_quiet taking a joystick read's trial values back out of the
// sound, checked against the audio integrator the machine uses.

#include <unity.h>
#include <stdlib.h>
#include <string.h>
#include "../../lib/coco_machine/src/coco_sound.h"
#include "../../lib/coco_machine/src/audio_integ.h"

void setUp(void) {}
void tearDown(void) {}

static struct snd_inputs in(uint8_t dac6, bool en, uint8_t src, bool sbs_out, bool sbs_hi) {
    struct snd_inputs i = { dac6, sbs_out, sbs_hi, en, src };
    return i;
}

static void test_switched_follows_the_multiplexer(void) {
    int32_t cart;
    struct snd_inputs i;
    // Color BASIC at rest: PB1 an input, the enable off. The line holds.
    i = in(40, false, SND_SRC_DAC, false, false);
    TEST_ASSERT_EQUAL_INT32(1234, snd_line_mv(&i, SND_SWITCHED, 1234, &cart));
    TEST_ASSERT_EQUAL_INT32(0, cart);
    // SOUND: the enable on, the DAC selected.
    i = in(63, true, SND_SRC_DAC, false, false);
    TEST_ASSERT_EQUAL_INT32(200 + 4500, snd_line_mv(&i, SND_SWITCHED, 0, &cart));
    i.dac6 = 0;
    TEST_ASSERT_EQUAL_INT32(200, snd_line_mv(&i, SND_SWITCHED, 0, &cart));
    TEST_ASSERT_EQUAL_INT32(0, cart);
    // The cartridge line: the DAC is not heard, the cartridge is, at 1.0.
    i = in(63, true, SND_SRC_CART, false, false);
    TEST_ASSERT_EQUAL_INT32(0, snd_line_mv(&i, SND_SWITCHED, 0, &cart));
    TEST_ASSERT_EQUAL_INT32(1024, cart);
    // The cassette (no tape: its offset) and nothing.
    i = in(63, true, SND_SRC_TAPE, false, false);
    TEST_ASSERT_EQUAL_INT32(2050, snd_line_mv(&i, SND_SWITCHED, 0, &cart));
    TEST_ASSERT_EQUAL_INT32(0, cart);
    i = in(63, true, SND_SRC_NONE, false, false);
    TEST_ASSERT_EQUAL_INT32(0, snd_line_mv(&i, SND_SWITCHED, 0, &cart));
}

static void test_single_bit_shapes_the_line(void) {
    int32_t cart;
    struct snd_inputs i;
    // PB1 an output, low, with the DAC: the gain drops, as measured.
    i = in(63, true, SND_SRC_DAC, true, false);
    TEST_ASSERT_EQUAL_INT32(180 + 2840, snd_line_mv(&i, SND_SWITCHED, 0, &cart));
    i.sbs_high = true;
    TEST_ASSERT_EQUAL_INT32(1300 + 3400, snd_line_mv(&i, SND_SWITCHED, 0, &cart));
    // With the cartridge line, the cartridge's share follows the gain.
    i = in(0, true, SND_SRC_CART, true, false);
    TEST_ASSERT_EQUAL_INT32(180, snd_line_mv(&i, SND_SWITCHED, 0, &cart));
    TEST_ASSERT_EQUAL_INT32(2840 * 1024 / 4700, cart);
    // The multiplexer off: the single-bit output alone drives the line.
    i = in(63, false, SND_SRC_DAC, true, true);
    TEST_ASSERT_EQUAL_INT32(3900, snd_line_mv(&i, SND_SWITCHED, 77, &cart));
    i.sbs_high = false;
    TEST_ASSERT_EQUAL_INT32(0, snd_line_mv(&i, SND_SWITCHED, 77, &cart));
    TEST_ASSERT_EQUAL_INT32(0, cart);
}

static void test_all_hears_everything(void) {
    int32_t cart;
    // Whatever the multiplexer and the enable say: the DAC and the cartridge.
    for (uint8_t src = 0; src < 4; src++)
        for (int en = 0; en < 2; en++) {
            struct snd_inputs i = in(63, en, src, false, false);
            TEST_ASSERT_EQUAL_INT32(200 + 4500, snd_line_mv(&i, SND_ALL, 0, &cart));
            TEST_ASSERT_EQUAL_INT32(1024, cart);
        }
    // The single-bit output still shapes the DAC.
    struct snd_inputs i = in(0, false, SND_SRC_NONE, true, true);
    TEST_ASSERT_EQUAL_INT32(1300, snd_line_mv(&i, SND_ALL, 0, &cart));
}

// A machine's audio, cycle by cycle, as coco_machine.cpp does it: the
// integrator, and the sample before the one in progress kept back.
struct sim {
    struct audio_integ a;
    struct snd_probe p;
    bool quiet;
    int32_t level;
    bool held;
    int32_t hl, hr;
    uint32_t htk;
    uint32_t tick;
    int out[4000];
    int n;
};

static void sim_cycle(struct sim *m, uint32_t ticks) {
    int s; uint32_t tk;
    if (audio_integ_step(&m->a, m->level, ticks, 48000, &s, &tk)) {
        if (m->held && m->n < 4000) m->out[m->n++] = m->hl;
        m->held = true;
        m->hl = m->hr = s;
        m->htk = tk;
    }
    m->tick += ticks;
}

static void sim_dac_write(struct sim *m, int32_t level) {
    if (m->quiet && level != m->level) snd_probe_write(&m->p, m->tick, m->level);
    else m->p.armed = false;
    m->level = level;
    sim_cycle(m, 16);
}

static void sim_comparator_read(struct sim *m) {
    if (m->quiet && snd_probe_read(&m->p, m->tick, m->level, &m->a.acc, m->a.tk,
                                   m->held, &m->hl, &m->hr, m->htk))
        m->level = m->p.level;
    sim_cycle(m, 16);
}

// A program playing a tone while it reads the joystick now and then, by
// successive approximation: six trials, each a DAC write and a comparator
// read two cycles later. With joystick_quiet the samples are those of the
// tone alone (to within the rounding of one division); without it they are
// not.
static void run_program(struct sim *m, bool probes) {
    srand(7);
    int32_t tone = 3000;
    for (int step = 0; step < 20000; step++) {
        if (step % 37 == 0) { tone = -tone; sim_dac_write(m, tone); }   // the tone
        else if (probes && step % 211 == 5) {
            int32_t trial = 8000;
            for (int k = 0; k < 6; k++) {
                sim_dac_write(m, trial);
                sim_cycle(m, 16);
                sim_comparator_read(m);
                trial = (trial / 2) + (rand() % 2 ? 6000 : -6000);
            }
            for (int k = 0; k < (rand() % 5); k++) sim_cycle(m, 8);    // land anywhere
        } else sim_cycle(m, 16);
    }
}

static void test_joystick_trials_are_not_heard(void) {
    static struct sim ref, quiet, loud;
    memset(&ref, 0, sizeof ref);
    memset(&quiet, 0, sizeof quiet);
    memset(&loud, 0, sizeof loud);
    quiet.quiet = true;
    run_program(&ref, false);
    run_program(&quiet, true);
    run_program(&loud, true);
    TEST_ASSERT_TRUE(ref.n > 1000);
    // The probe runs take extra time, so compare the runs as far as the
    // first one diverges in time: up to the first trial both match exactly,
    // and then the quiet run must never stray more than rounding.
    int worst_quiet = 0, worst_loud = 0;
    struct sim r2;
    memset(&r2, 0, sizeof r2);
    // A reference that takes the same time: the trials' cycles at the
    // tone's level, which is what joystick_quiet should give.
    srand(7);
    int32_t tone = 3000;
    for (int step = 0; step < 20000; step++) {
        if (step % 37 == 0) { tone = -tone; r2.level = tone; sim_cycle(&r2, 16); }
        else if (step % 211 == 5) {
            for (int k = 0; k < 6; k++) { sim_cycle(&r2, 16); sim_cycle(&r2, 16); sim_cycle(&r2, 16); (void)(rand() % 2); }
            for (int k = 0; k < (rand() % 5); k++) sim_cycle(&r2, 8);
        } else sim_cycle(&r2, 16);
    }
    TEST_ASSERT_EQUAL_INT(r2.n, quiet.n);
    for (int i = 0; i < r2.n; i++) {
        int dq = abs(quiet.out[i] - r2.out[i]), dl = abs(loud.out[i] - r2.out[i]);
        if (dq > worst_quiet) worst_quiet = dq;
        if (dl > worst_loud) worst_loud = dl;
    }
    TEST_ASSERT_LESS_OR_EQUAL_INT(1, worst_quiet);
    TEST_ASSERT_GREATER_THAN_INT(1000, worst_loud);
}

// A DAC write with no comparator read after it is sound, and stays.
static void test_a_write_without_a_read_is_heard(void) {
    struct snd_probe p = { false, 0, 0 };
    int32_t acc = 0, hl = 0, hr = 0;
    snd_probe_write(&p, 1000, 500);
    TEST_ASSERT_FALSE(snd_probe_read(&p, 1000 + SND_PROBE_TICKS + 16, 900, &acc, 100, true, &hl, &hr, 298));
    TEST_ASSERT_EQUAL_INT32(0, acc);
    // Read once: a second read is not a second trial.
    snd_probe_write(&p, 2000, 500);
    TEST_ASSERT_TRUE(snd_probe_read(&p, 2032, 900, &acc, 100, true, &hl, &hr, 298));
    TEST_ASSERT_EQUAL_INT32((500 - 900) * 32, acc);
    TEST_ASSERT_FALSE(snd_probe_read(&p, 2048, 900, &acc, 100, true, &hl, &hr, 298));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_switched_follows_the_multiplexer);
    RUN_TEST(test_single_bit_shapes_the_line);
    RUN_TEST(test_all_hears_everything);
    RUN_TEST(test_joystick_trials_are_not_heard);
    RUN_TEST(test_a_write_without_a_read_is_heard);
    return UNITY_END();
}
