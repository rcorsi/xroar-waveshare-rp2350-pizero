// hid_consumer.h: volume keys from any USB HID device (PIZERO-255).
// Pure, host-tested (test/test_hid_consumer).
//
// Keyboards send their media keys, and USB headphone DACs their buttons, as
// Consumer page (0x0C) reports whose layout only the device's HID report
// descriptor gives: an array of 16-bit usage codes (the usual keyboard way)
// or one bit per usage (the usual headset way), with or without a report ID.
// hidc_parse reads a report descriptor once, at mount, and keeps where the
// three usages wanted here live: Volume Increment (0xE9), Volume Decrement
// (0xEA) and Mute (0xE2). hidc_decode then says which of them a report holds
// down, and whether the report was one of these at all.

#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

enum { HIDC_UP = 1, HIDC_DOWN = 2, HIDC_MUTE = 4 };

#define HIDC_BITS   6          // variable (one bit a usage) fields kept
#define HIDC_ARRAYS 2          // array fields kept
#define HIDC_LIST   8          // usages kept for an array listed one by one

struct hidc_array {
    uint8_t  rid; uint16_t bit; uint8_t size, count;
    int32_t  lmin;
    uint16_t umin, umax;       // a range of usages...
    uint8_t  nlist;            // ...or these, one per value from lmin
    uint16_t list[HIDC_LIST];
};

struct hidc_map {
    bool    has;               // a volume or mute usage was found
    bool    ids;               // reports start with a report ID
    uint8_t nbits, narr;
    struct { uint8_t rid; uint16_t bit; uint8_t which; } bits[HIDC_BITS];
    struct hidc_array arr[HIDC_ARRAYS];
};

static inline uint8_t hidc_which(uint32_t page, uint32_t usage) {
    if (page != 0x0C) return 0;
    return usage == 0xE9 ? HIDC_UP : usage == 0xEA ? HIDC_DOWN : usage == 0xE2 ? HIDC_MUTE : 0;
}

// Parse a report descriptor. Returns true if it carries any of the three.
static inline bool hidc_parse(const uint8_t *d, uint16_t len, struct hidc_map *m) {
    memset(m, 0, sizeof *m);
    uint32_t page = 0, rsize = 0, rcount = 0;
    int32_t lmin = 0;
    uint8_t rid = 0;
    uint16_t off[256];                                  // bit offset per report ID, inputs
    memset(off, 0, sizeof off);
    struct { uint32_t page, usage; } use[16];           // local usages, page included
    uint8_t nuse = 0;
    uint32_t umin = 0, umax = 0, umin_page = 0;
    bool have_range = false;
    // Global state saved by PUSH, one level deep (rare in practice).
    uint32_t s_page = 0, s_rsize = 0, s_rcount = 0; int32_t s_lmin = 0; uint8_t s_rid = 0;

    for (uint16_t i = 0; i < len;) {
        uint8_t b = d[i];
        if (b == 0xFE) {                                // long item: skip
            if (i + 2 >= len) break;
            i = (uint16_t)(i + 3 + d[i + 1]);
            continue;
        }
        uint8_t sz = b & 3;
        if (sz == 3) sz = 4;
        if (i + 1 + sz > len) break;
        uint32_t u = 0;
        for (uint8_t k = 0; k < sz; k++) u |= (uint32_t)d[i + 1 + k] << (8 * k);
        int32_t sv = (int32_t)u;                         // signed reading of the data
        if (sz == 1) sv = (int8_t)u; else if (sz == 2) sv = (int16_t)u;
        uint8_t type = (b >> 2) & 3, tag = b >> 4;
        if (type == 1) {                                 // global
            switch (tag) {
            case 0x0: page = u; break;
            case 0x1: lmin = sv; break;
            case 0x7: rsize = u; break;
            case 0x8: rid = (uint8_t)u; m->ids = true; break;
            case 0x9: rcount = u; break;
            case 0xA: s_page = page; s_rsize = rsize; s_rcount = rcount; s_lmin = lmin; s_rid = rid; break;
            case 0xB: page = s_page; rsize = s_rsize; rcount = s_rcount; lmin = s_lmin; rid = s_rid; break;
            default: break;
            }
        } else if (type == 2) {                          // local
            uint32_t pg = sz == 4 ? (u >> 16) : page;
            uint32_t us = sz == 4 ? (u & 0xFFFF) : u;
            if (tag == 0x0 && nuse < 16) { use[nuse].page = pg; use[nuse].usage = us; nuse++; }
            else if (tag == 0x1) { umin = us; umin_page = pg; have_range = true; }
            else if (tag == 0x2) { umax = us; have_range = true; }
        } else if (type == 0) {                          // main
            if (tag == 0x8) {                            // input
                bool constant = u & 1, variable = u & 2;
                uint16_t start = off[rid];
                if (!constant && variable) {
                    for (uint32_t e = 0; e < rcount; e++) {
                        uint32_t pg, us;
                        if (nuse) {
                            uint32_t k = e < nuse ? e : (uint32_t)(nuse - 1);
                            pg = use[k].page; us = use[k].usage;
                        } else if (have_range) {
                            pg = umin_page ? umin_page : page; us = umin + e;
                            if (us > umax) break;
                        } else break;
                        uint8_t w = hidc_which(pg, us);
                        if (w && m->nbits < HIDC_BITS && rsize == 1) {
                            m->bits[m->nbits].rid = rid;
                            m->bits[m->nbits].bit = (uint16_t)(start + e * rsize);
                            m->bits[m->nbits].which = w;
                            m->nbits++;
                        }
                    }
                } else if (!constant) {                  // array
                    uint32_t pg = nuse ? use[0].page : (umin_page ? umin_page : page);
                    bool hit = false;
                    if (nuse) {
                        for (uint8_t k = 0; k < nuse; k++) if (hidc_which(use[k].page, use[k].usage)) hit = true;
                    } else if (have_range && pg == 0x0C) {
                        hit = umin <= 0xEA && umax >= 0xE2;
                    }
                    if (hit && m->narr < HIDC_ARRAYS && rsize >= 1 && rsize <= 16) {
                        struct hidc_array *a = &m->arr[m->narr++];
                        a->rid = rid; a->bit = start; a->size = (uint8_t)rsize; a->count = (uint8_t)rcount;
                        a->lmin = lmin; a->umin = (uint16_t)umin; a->umax = (uint16_t)umax;
                        a->nlist = 0;
                        if (nuse) {
                            for (uint8_t k = 0; k < nuse && k < HIDC_LIST; k++)
                                a->list[a->nlist++] = pg == 0x0C ? (uint16_t)use[k].usage : 0;
                        }
                    }
                }
                off[rid] = (uint16_t)(start + rsize * rcount);
            }
            nuse = 0; umin = umax = umin_page = 0; have_range = false;   // locals end with a main item
        }
        i = (uint16_t)(i + 1 + sz);
    }
    m->has = m->nbits || m->narr;
    return m->has;
}

static inline uint32_t hidc_field(const uint8_t *p, uint16_t n, uint16_t bit, uint8_t size) {
    uint32_t v = 0;
    for (uint8_t k = 0; k < size; k++) {
        uint16_t b = (uint16_t)(bit + k);
        if ((b >> 3) < n && (p[b >> 3] >> (b & 7)) & 1) v |= 1u << k;
    }
    return v;
}

// Which of HIDC_UP/DOWN/MUTE a report holds down. *mine is true when the
// report is one these fields live in (its report ID matches), so the caller
// can keep it away from other decoders.
static inline uint8_t hidc_decode(const struct hidc_map *m, const uint8_t *r, uint16_t len, bool *mine) {
    *mine = false;
    if (!m->has || !len) return 0;
    uint8_t rid = 0;
    if (m->ids) { rid = r[0]; r++; len--; }
    uint8_t out = 0;
    for (uint8_t k = 0; k < m->nbits; k++) {
        if (m->bits[k].rid != rid) continue;
        *mine = true;
        if (hidc_field(r, len, m->bits[k].bit, 1)) out |= m->bits[k].which;
    }
    for (uint8_t k = 0; k < m->narr; k++) {
        const struct hidc_array *a = &m->arr[k];
        if (a->rid != rid) continue;
        *mine = true;
        for (uint8_t e = 0; e < a->count; e++) {
            int32_t v = (int32_t)hidc_field(r, len, (uint16_t)(a->bit + e * a->size), a->size);
            if (v == 0 && a->lmin == 0 && !a->nlist && a->umin == 0) continue;   // 0: nothing pressed
            int32_t idx = v - a->lmin;
            if (idx < 0) continue;
            uint32_t usage = a->nlist ? (idx < a->nlist ? a->list[idx] : 0) : (uint32_t)(a->umin + idx);
            if (!a->nlist && usage > a->umax) continue;
            out |= hidc_which(0x0C, usage);
        }
    }
    return out;
}
