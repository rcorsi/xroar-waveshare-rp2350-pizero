// PIZERO-255: the volume keys of keyboards and USB headphone DACs
// (src/hid_consumer.h), from their HID report descriptors.

#include <unity.h>
#include "../../src/hid_consumer.h"

void setUp(void) {}
void tearDown(void) {}

static struct hidc_map m;

static uint8_t dec(const uint8_t *r, uint16_t n, bool *mine) { return hidc_decode(&m, r, n, mine); }

// A keyboard's media keys: report ID 3, one 16-bit usage code a report,
// Usage Min 0 to Max 0x3FF (the usual layout).
static void test_keyboard_style_array_with_report_id(void) {
    static const uint8_t d[] = {
        0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,       // a keyboard collection first
        0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
        0xC0,
        0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x03,
        0x15, 0x00, 0x26, 0xFF, 0x03, 0x19, 0x00, 0x2A, 0xFF, 0x03,
        0x75, 0x10, 0x95, 0x01, 0x81, 0x00, 0xC0 };
    TEST_ASSERT_TRUE(hidc_parse(d, sizeof d, &m));
    bool mine;
    const uint8_t up[] = { 0x03, 0xE9, 0x00 }, down[] = { 0x03, 0xEA, 0x00 },
                  mute[] = { 0x03, 0xE2, 0x00 }, none[] = { 0x03, 0x00, 0x00 },
                  play[] = { 0x03, 0xCD, 0x00 }, kbd[] = { 0x01, 0x02 };
    TEST_ASSERT_EQUAL_UINT8(HIDC_UP, dec(up, 3, &mine));      TEST_ASSERT_TRUE(mine);
    TEST_ASSERT_EQUAL_UINT8(HIDC_DOWN, dec(down, 3, &mine));
    TEST_ASSERT_EQUAL_UINT8(HIDC_MUTE, dec(mute, 3, &mine));
    TEST_ASSERT_EQUAL_UINT8(0, dec(none, 3, &mine));          TEST_ASSERT_TRUE(mine);
    TEST_ASSERT_EQUAL_UINT8(0, dec(play, 3, &mine));
    TEST_ASSERT_EQUAL_UINT8(0, dec(kbd, 2, &mine));           TEST_ASSERT_FALSE(mine);   // the keyboard's own
}

// A headset's buttons: no report ID, one bit a usage, then padding.
static void test_headset_style_bits(void) {
    static const uint8_t d[] = {
        0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x15, 0x00, 0x25, 0x01,
        0x09, 0xE9, 0x09, 0xEA, 0x09, 0xE2, 0x09, 0xCD, 0x09, 0xB5, 0x09, 0xB6,
        0x75, 0x01, 0x95, 0x06, 0x81, 0x02, 0x75, 0x02, 0x95, 0x01, 0x81, 0x01, 0xC0 };
    TEST_ASSERT_TRUE(hidc_parse(d, sizeof d, &m));
    bool mine;
    uint8_t r = 0x01;
    TEST_ASSERT_EQUAL_UINT8(HIDC_UP, dec(&r, 1, &mine));      TEST_ASSERT_TRUE(mine);
    r = 0x02; TEST_ASSERT_EQUAL_UINT8(HIDC_DOWN, dec(&r, 1, &mine));
    r = 0x04; TEST_ASSERT_EQUAL_UINT8(HIDC_MUTE, dec(&r, 1, &mine));
    r = 0x08; TEST_ASSERT_EQUAL_UINT8(0, dec(&r, 1, &mine));  // play/pause
    r = 0x03; TEST_ASSERT_EQUAL_UINT8(HIDC_UP | HIDC_DOWN, dec(&r, 1, &mine));
}

// Bits after another field in the same report, with a report ID: offsets
// count from the report's own start, past its ID.
static void test_bits_after_another_field(void) {
    static const uint8_t d[] = {
        0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x02, 0x15, 0x00, 0x25, 0x01,
        0x09, 0xCD, 0x09, 0xB5, 0x09, 0xB6, 0x75, 0x01, 0x95, 0x03, 0x81, 0x02,   // bits 0-2
        0x75, 0x05, 0x95, 0x01, 0x81, 0x01,                                         // bits 3-7 padding
        0x09, 0xE2, 0x09, 0xE9, 0x09, 0xEA, 0x75, 0x01, 0x95, 0x03, 0x81, 0x02,   // bits 8-10
        0xC0 };
    TEST_ASSERT_TRUE(hidc_parse(d, sizeof d, &m));
    bool mine;
    const uint8_t mute[] = { 0x02, 0x00, 0x01 }, up[] = { 0x02, 0x07, 0x02 }, down[] = { 0x02, 0x00, 0x04 },
                  other[] = { 0x05, 0xFF, 0xFF };
    TEST_ASSERT_EQUAL_UINT8(HIDC_MUTE, dec(mute, 3, &mine));
    TEST_ASSERT_EQUAL_UINT8(HIDC_UP, dec(up, 3, &mine));
    TEST_ASSERT_EQUAL_UINT8(HIDC_DOWN, dec(down, 3, &mine));
    TEST_ASSERT_EQUAL_UINT8(0, dec(other, 3, &mine));          TEST_ASSERT_FALSE(mine);
}

// An array of listed usages: value 1 is the first listed (Logical Min 1).
static void test_array_of_listed_usages(void) {
    static const uint8_t d[] = {
        0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x15, 0x01, 0x25, 0x03,
        0x09, 0xEA, 0x09, 0xE9, 0x09, 0xE2, 0x75, 0x02, 0x95, 0x01, 0x81, 0x00,
        0x75, 0x06, 0x95, 0x01, 0x81, 0x01, 0xC0 };
    TEST_ASSERT_TRUE(hidc_parse(d, sizeof d, &m));
    bool mine;
    uint8_t r = 1; TEST_ASSERT_EQUAL_UINT8(HIDC_DOWN, dec(&r, 1, &mine));
    r = 2;         TEST_ASSERT_EQUAL_UINT8(HIDC_UP, dec(&r, 1, &mine));
    r = 3;         TEST_ASSERT_EQUAL_UINT8(HIDC_MUTE, dec(&r, 1, &mine));
    r = 0;         TEST_ASSERT_EQUAL_UINT8(0, dec(&r, 1, &mine));
}

// A usage given with its page in the top 16 bits (a 4-byte Usage item),
// under another page.
static void test_extended_usage(void) {
    static const uint8_t d[] = {
        0x05, 0x01, 0x09, 0x80, 0xA1, 0x01, 0x15, 0x00, 0x25, 0x01,
        0x0B, 0xE9, 0x00, 0x0C, 0x00, 0x0B, 0xEA, 0x00, 0x0C, 0x00,
        0x75, 0x01, 0x95, 0x02, 0x81, 0x02, 0x75, 0x06, 0x95, 0x01, 0x81, 0x01, 0xC0 };
    TEST_ASSERT_TRUE(hidc_parse(d, sizeof d, &m));
    bool mine;
    uint8_t r = 2; TEST_ASSERT_EQUAL_UINT8(HIDC_DOWN, dec(&r, 1, &mine));
}

// A plain keyboard has none, and nothing is claimed.
static void test_plain_keyboard_has_none(void) {
    static const uint8_t d[] = {
        0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
        0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
        0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0 };
    TEST_ASSERT_FALSE(hidc_parse(d, sizeof d, &m));
    bool mine;
    const uint8_t r[8] = { 0, 0, 0x04 };
    TEST_ASSERT_EQUAL_UINT8(0, dec(r, 8, &mine));
    TEST_ASSERT_FALSE(mine);
}

// The user's USB-C headphone dongle (001F:0B21), as it reported on the
// board: ten one-bit Consumer buttons (Volume Up and Down are bits 8 and 9),
// a Telephony bit, then padding; no report ID. Its headphones sent only
// 04 00 (Play/Pause) for every button, which must not change the volume.
static void test_the_users_dongle(void) {
    static const uint8_t d[] = {
        0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x0A,
        0x09, 0xB5, 0x09, 0xB6, 0x09, 0xCD, 0x09, 0xE2, 0x09, 0xB7, 0x0A, 0x5B, 0x02, 0x0A, 0x5C, 0x02,
        0x0A, 0x62, 0x02, 0x09, 0xE9, 0x09, 0xEA, 0x81, 0x02, 0x05, 0x0B, 0x95, 0x01, 0x09, 0x21, 0x81, 0x02,
        0x75, 0x05, 0x95, 0x01, 0x81, 0x03, 0xC0 };
    TEST_ASSERT_TRUE(hidc_parse(d, sizeof d, &m));
    bool mine;
    const uint8_t play[] = { 0x04, 0x00 }, mute[] = { 0x08, 0x00 },
                  up[] = { 0x00, 0x01 }, down[] = { 0x00, 0x02 }, none[] = { 0x00, 0x00 };
    TEST_ASSERT_EQUAL_UINT8(0, dec(play, 2, &mine));          TEST_ASSERT_TRUE(mine);
    TEST_ASSERT_EQUAL_UINT8(HIDC_MUTE, dec(mute, 2, &mine));
    TEST_ASSERT_EQUAL_UINT8(HIDC_UP, dec(up, 2, &mine));
    TEST_ASSERT_EQUAL_UINT8(HIDC_DOWN, dec(down, 2, &mine));
    TEST_ASSERT_EQUAL_UINT8(0, dec(none, 2, &mine));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_keyboard_style_array_with_report_id);
    RUN_TEST(test_headset_style_bits);
    RUN_TEST(test_bits_after_another_field);
    RUN_TEST(test_array_of_listed_usages);
    RUN_TEST(test_extended_usage);
    RUN_TEST(test_plain_keyboard_has_none);
    RUN_TEST(test_the_users_dongle);
    return UNITY_END();
}
