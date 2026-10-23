// usb_audio.cpp: see usb_audio.h (PIZERO-255).

#include <Arduino.h>
#include "pio_usb.h"
#include "Adafruit_TinyUSB.h"
#include "usb_audio.h"

#ifdef USB_AUDIO

// What the search found: the playback interface and alternate setting,
// its isochronous OUT endpoint descriptor, the class version and, for
// UAC2, the clock source that sets its rate.
struct uac_out {
    bool    found;
    bool    uac2;
    uint8_t ac_itf;        // the AudioControl interface (UAC2 clock requests)
    uint8_t itf, alt;
    uint8_t clock_id;      // UAC2: the clock source feeding the terminal
    uint8_t ep[7];         // the endpoint descriptor, as TinyUSB opens it
};

static uint8_t  g_ua_buf[512];
static uint8_t  g_ua_daddr;          // the streaming device, 0 when none
static bool     g_ua_busy;           // a device's setup in progress
static struct uac_out g_ua;
static uint8_t  g_ua_rate[4];        // 48,000, little-endian
static const char *g_ua_state = "none";

// UAC1 and UAC2 both: a type I PCM format, 2 channels of 16 bits in 2-byte
// slots, on an isochronous OUT endpoint. UAC1 lists its rates in the format
// descriptor and must offer 48 kHz; UAC2 rates belong to the clock source,
// which is set to 48 kHz and checked by the request's result.
static bool ua_find(const uint8_t *p, uint16_t len, struct uac_out *o) {
    uint8_t cls = 0, sub = 0, itf = 0, alt = 0;
    uint8_t term_clock[16] = {0};        // UAC2 input terminal id -> clock id
    uint8_t link = 0, channels = 0, slot = 0, bits = 0;
    bool rate_ok = false;
    memset(o, 0, sizeof *o);
    for (uint16_t i = 0; i + 2 <= len && p[i] >= 2 && i + p[i] <= len; i += p[i]) {
        const uint8_t *d = p + i;
        uint8_t dl = d[0], ty = d[1];
        if (ty == 0x04 && dl >= 9) {
            itf = d[2]; alt = d[3]; cls = d[5]; sub = d[6];
            if (cls == 1 && sub == 1) { o->ac_itf = itf; o->uac2 = d[7] == 0x20; }
            link = channels = slot = bits = 0;
            rate_ok = false;
        } else if (ty == 0x24 && cls == 1 && sub == 1 && dl >= 8 && d[2] == 0x02 && o->uac2) {
            if (d[3] < 16) term_clock[d[3]] = d[7];             // UAC2 input terminal's clock
        } else if (ty == 0x24 && cls == 1 && sub == 2 && d[2] == 0x01 && dl >= 4) {
            link = d[3];                                        // AS_GENERAL: terminal link
            if (o->uac2 && dl >= 11) channels = d[10];
        } else if (ty == 0x24 && cls == 1 && sub == 2 && d[2] == 0x02 && dl >= 6 && d[3] == 1) {
            if (!o->uac2 && dl >= 8) {                          // UAC1 type I
                channels = d[4]; slot = d[5]; bits = d[6];
                uint8_t n = d[7];
                if (n == 0 && dl >= 14) {
                    uint32_t lo = d[8] | d[9] << 8 | d[10] << 16, hi = d[11] | d[12] << 8 | d[13] << 16;
                    rate_ok = lo <= 48000 && 48000 <= hi;
                }
                for (uint8_t k = 0; k < n && 8 + 3 * k + 2 < dl; k++)
                    if ((uint32_t)(d[8+3*k] | d[9+3*k] << 8 | d[10+3*k] << 16) == 48000) rate_ok = true;
            } else if (o->uac2) {                               // UAC2 type I
                slot = d[4]; bits = d[5];
                rate_ok = true;
            }
        } else if (ty == 0x05 && dl >= 7 && cls == 1 && sub == 2 && alt) {
            bool iso_out = !(d[2] & 0x80) && (d[3] & 3) == 1;
            if (iso_out && channels == 2 && slot == 2 && bits == 16 && rate_ok && !o->found) {
                o->found = true;
                o->itf = itf;
                o->alt = alt;
                memcpy(o->ep, d, 7);
                o->clock_id = (o->uac2 && link < 16) ? term_clock[link] : 0;
            }
        }
    }
    return o->found;
}

#ifdef USB_AUDIO_TONE
// 440 Hz at about -12 dBFS, the same on both sides, in RAM since the fill
// runs in the USB frame interrupt.
#define UA_SINE 256
static int16_t g_ua_sine[UA_SINE];
static uint32_t g_ua_ph, g_ua_inc;
static uint16_t __not_in_flash_func(ua_fill)(uint8_t *buf, uint16_t max) {
    uint16_t n = 48;
    if (n * 4 > max) n = max / 4;
    int16_t *o = (int16_t *)buf;
    for (uint16_t k = 0; k < n; k++) {
        int16_t s = g_ua_sine[g_ua_ph >> 24];
        g_ua_ph += g_ua_inc;
        o[2 * k] = s;
        o[2 * k + 1] = s;
    }
    return (uint16_t)(n * 4);
}
#endif

static void ua_fail(const char *what) {
    g_ua_state = what;
    Serial.printf("[uac] addr=%u %s\r\n", g_ua_daddr, what);
    g_ua_daddr = 0;
    g_ua_busy = false;
}

static void ua_rate_done(tuh_xfer_t *x) {
    if (x->result != XFER_RESULT_SUCCESS)
        Serial.printf("[uac] addr=%u rate request not taken (%d), going on\r\n", g_ua_daddr, (int)x->result);
    if (!tuh_edpt_open(g_ua_daddr, (tusb_desc_endpoint_t const *)g_ua.ep)) { ua_fail("endpoint open failed"); return; }
#ifdef USB_AUDIO_TONE
    for (int k = 0; k < UA_SINE; k++) g_ua_sine[k] = (int16_t)(8000.0 * sin(2 * M_PI * k / UA_SINE));
    g_ua_inc = (uint32_t)(440.0 * 4294967296.0 / 48000.0);
    bool ok = pio_usb_host_iso_out_stream(0, g_ua_daddr, g_ua.ep[2], ua_fill);
#else
    bool ok = false;
#endif
    if (!ok) { ua_fail("stream not started"); return; }
    g_ua_state = "streaming";
    g_ua_busy = false;
    Serial.printf("[uac] addr=%u streaming: UAC%u interface %u alt %u endpoint %02X max %u\r\n",
                  g_ua_daddr, g_ua.uac2 ? 2 : 1, g_ua.itf, g_ua.alt, g_ua.ep[2], g_ua.ep[4] | g_ua.ep[5] << 8);
}

static void ua_alt_done(tuh_xfer_t *x) {
    if (x->result != XFER_RESULT_SUCCESS) { ua_fail("SET_INTERFACE failed"); return; }
    static tusb_control_request_t req;
    g_ua_rate[0] = 48000 & 0xFF; g_ua_rate[1] = (48000 >> 8) & 0xFF; g_ua_rate[2] = 48000 >> 16; g_ua_rate[3] = 0;
    if (g_ua.uac2) {
        // UAC2 CUR of the clock source's SAM_FREQ control, 4 bytes.
        req.bmRequestType = 0x21;
        req.bRequest = 0x01;
        req.wValue = 0x0100;
        req.wIndex = (uint16_t)(g_ua.clock_id << 8 | g_ua.ac_itf);
        req.wLength = 4;
    } else {
        // UAC1 SET_CUR of the endpoint's SAMPLING_FREQ control, 3 bytes.
        req.bmRequestType = 0x22;
        req.bRequest = 0x01;
        req.wValue = 0x0100;
        req.wIndex = g_ua.ep[2];
        req.wLength = 3;
    }
    tuh_xfer_t xfer = {};
    xfer.daddr = g_ua_daddr;
    xfer.ep_addr = 0;
    xfer.setup = &req;
    xfer.buffer = g_ua_rate;
    xfer.complete_cb = ua_rate_done;
    if (!tuh_control_xfer(&xfer)) ua_fail("rate request not sent");
}

static void ua_config_done(tuh_xfer_t *x) {
    uint8_t daddr = (uint8_t)x->user_data;
    if (x->result != XFER_RESULT_SUCCESS) { g_ua_busy = false; return; }
    uint16_t total = (uint16_t)(g_ua_buf[2] | (g_ua_buf[3] << 8));
    if (total > sizeof g_ua_buf) total = sizeof g_ua_buf;
    if (!ua_find(g_ua_buf, total, &g_ua)) { g_ua_busy = false; return; }   // not an audio device
    g_ua_daddr = daddr;
    g_ua_state = "setting up";
    Serial.printf("[uac] addr=%u UAC%u playback: interface %u alt %u endpoint %02X clock %u\r\n",
                  daddr, g_ua.uac2 ? 2 : 1, g_ua.itf, g_ua.alt, g_ua.ep[2], g_ua.clock_id);
    if (!tuh_interface_set(daddr, g_ua.itf, g_ua.alt, ua_alt_done, 0)) ua_fail("SET_INTERFACE not sent");
}

void usb_audio_mount(uint8_t daddr) {
    if (g_ua_daddr || g_ua_busy) return;      // one DAC at a time
    g_ua_busy = tuh_descriptor_get_configuration(daddr, 0, g_ua_buf, sizeof g_ua_buf, ua_config_done, daddr);
}

void usb_audio_umount(uint8_t daddr) {
    if (daddr != g_ua_daddr) return;
    pio_usb_host_iso_out_stream(0, 0, 0, NULL);
    g_ua_daddr = 0;
    g_ua_state = "unplugged";
}

void usb_audio_telemetry(void) {
    static uint32_t last;
    uint32_t f = pio_usb_host_iso_frames();
    if (g_ua_daddr || f)
        Serial.printf("[uac] %s packets=%lu (%lu/s)\r\n", g_ua_state, (unsigned long)f, (unsigned long)(f - last));
    last = f;
}

#endif  // USB_AUDIO
