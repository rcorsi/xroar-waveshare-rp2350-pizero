# CoCo 3 on the RP2350-PiZero: what fits and what does not

**PIZERO-111.** CoCo 3 support is a stated goal. This is the arithmetic behind
the answer, done against measured numbers rather than instinct, so the decision
is about scope rather than hope.

_Measured 2026-09-17 on the post-PIZERO-119 build. Sources: the build's own RAM
report, `docs/measurements/2026-09.jsonl`, and `docs/cpu-speed.md`._

## Outcome (2026-10-14)

This study's conclusion held for RAM and for the 512 KB answer, and was
overtaken for the CPU. The 6809 was made fast enough (PIZERO-131), and a
128 KB CoCo 3 now runs at full speed in its own build, `pizero_coco3`
(PIZERO-133). Per [`coco3-plan.md`](coco3-plan.md), measured on the bench
board:

| | 0.89 MHz | 1.79 MHz |
|---|---|---|
| BASIC at OK | 60 fps, about 8.5-9 ms emulation a frame | 60 fps, about 13 ms |
| Full-screen HCLS loop on HSCREEN 2 | | 60 fps, median 12.9 ms, worst 15.2 ms, no late frames, sound 0% short (PIZERO-220) |

Two things went differently from what is predicted below:

- **The framebuffer stayed.** A 128 KB CoCo 3 fits beside it: the
  `pizero_coco3` build uses 400,020 bytes, 76.3% of RAM. Per-line scanout is
  needed only for the 640-wide modes (PIZERO-134, PIZERO-179).
- **The GIME's video was a real cost.** Once the CPU was fast, upstream's
  scanline renderer plus the halving sink took about 7.6 + 3.5 ms a frame.
  A port-side renderer that draws each changed line once at 320 pixels
  (PIZERO-218, PIZERO-220) cut that to about 2,500 clocks per redrawn line
  on HSCREEN 2.

The 141-clocks-per-cycle game mentioned below (PIZERO-210) was never
identified, and that issue was closed.

## The short answer

| Question | Answer |
|---|---|
| 512 KB CoCo 3 | **No.** Not on this board, at any speed. |
| 128 KB CoCo 3, 320-wide modes | Possible on RAM and video, **blocked on CPU speed**. |
| 128 KB CoCo 3, 640-wide modes | Possible on RAM, needs a different scanout, **blocked on CPU speed**. |
| The wall | **The 6809, not the video.** This is the opposite of what we assumed. |

The ticket expected the 640-wide 16-color modes to be the wall and the
320-wide ones to be reachable. Video turns out not to be the binding
constraint at all.

## RAM

The board has 520 KB of SRAM and **no PSRAM**: the pad exists on the PCB but is
unpopulated (`README.md`), so every byte is on-chip.

| Item | Bytes |
|---|---|
| Firmware static (measured, 70.9%) | 371,660 |
| of which the scanout framebuffer, 320x240 RGB565 | 153,600 |
| of which the packed VDG buffer, 192 x 128 | 24,576 |
| Guest RAM today, allocated at runtime | 65,536 |
| **Free** | **~87,000** |

Note (2026-09-27): the product build's static RAM is now ~75% (PIZERO-150),
up from the 70.9% measured here, so the free figure above is smaller today.

- **128 KB CoCo 3** needs +64 KB over today. It fits, with roughly 23 KB to
  spare. Tight but real.
- **512 KB CoCo 3** needs +448 KB. There is nowhere to take it from: even
  deleting the framebuffer entirely (see per-line scanout below) frees only
  ~150 KB, leaving us ~300 KB short. Populating the PSRAM pad is the only route,
  and that is a hardware change to a board we are about to ship to other people.

## Video, mode by mode

Guest framebuffer sizes are the GIME's own. Conversion cost is our per-pixel
rate into the scanout format, measured on this build: the SG4 path runs at
9.7 ns/pixel and the blit at 20.6 ns/pixel, so the range below is
optimistic-to-pessimistic for a similar LUT-driven path.

| GIME mode | Guest bytes | Pixels/frame | Convert (9.7-20.6 ns/px) | Fits our scanout? |
|---|---|---|---|---|
| 160x192x16 | 15,360 | 30,720 | 0.3-0.6 ms | Yes, 1:1 into 320 wide with doubling |
| 320x192x4 | 15,360 | 61,440 | 0.6-1.3 ms | Yes, 1:1 |
| 320x192x16 | 30,720 | 61,440 | 0.6-1.3 ms | Yes, 1:1 |
| 640x192x2 | 15,360 | 122,880 | 1.2-2.5 ms | No, needs per-line scanout |
| 640x192x4 | 30,720 | 122,880 | 1.2-2.5 ms | No, needs per-line scanout |
| 640x192x16 | 61,440 | 122,880 | 1.2-2.5 ms | No, needs per-line scanout |
| 640x225x16 | 72,000 | 144,000 | 1.4-3.0 ms | No, needs per-line scanout |
| 80x25 text + attributes | 4,000 | ~122,880 | 1.2-2.5 ms | No, needs per-line scanout |

Two structural notes:

- **320-wide modes drop straight in.** Our framebuffer is 320 wide and libdvi
  doubles it to 640, which is exactly what the GIME's 320-wide modes want.
- **640-wide modes cannot use a framebuffer.** _(Overtaken: PIZERO-134 uses
  a 640x240 framebuffer of one-byte colours, looked up in a palette by the
  RP2350's hardware TMDS encoder, the same 153,600 bytes as the RGB565 one.)_
  A 640x240 RGB565 buffer is
  307,200 bytes, more than twice what is free. The route is per-scanline
  generation into libdvi's scanline queue (`dvi_scanbuf_main_16bpp`), which
  needs two line buffers of 1,280 bytes instead. That is a real change to the
  scanout architecture, but it is libdvi's own supported path, and it would also
  free the 153,600-byte framebuffer, which is where the RAM for a 128 KB guest
  would come from comfortably.
- **Vertically**, 192 lines doubled is 384 of 480, so those modes letterbox.
  225 lines doubled is 450, which nearly fills the screen.

## The CPU, which is the actual wall

A CoCo 3 runs at **1.79 MHz**, double the CoCo 2's 0.895 MHz. Emulation cost
scales with emulated cycles, and we have measured both points:

| | emulated clock | emulation cost per frame |
|---|---|---|
| Today, measured | 0.895 MHz | **10.4 ms** of a 16.67 ms frame |
| CoCo 3 at native speed | 1.79 MHz | **~20.8 ms**, i.e. 125% of the frame |

**Measured 2026-10-12 (PIZERO-130)**, unit 10, product build against the
`pizero_cpu179` build (twice the cycles per frame), both idle at the BASIC
prompt, 3 minutes each (`docs/measurements/2026-10.jsonl`):

| | cycles per frame | emulation per frame | whole frame | fps |
|---|---|---|---|---|
| 0.895 MHz (product) | 14,915 | **10.98 ms**, 65.8% | 13.0 ms, 78.1% | 60 |
| 1.79 MHz (`pizero_cpu179`) | 29,830 | **21.95 ms**, 131.7% | 24.0 ms, 144.2% | 42 |

Both cost **185.4 host clocks per emulated cycle**: emulation scales exactly
with clock rate, so the 2x build is a fair stand-in for the CoCo 3's CPU load.
The base has grown from 10.4 ms since the estimate above (features added since
September), so the real overrun is 32%, not 25%. Render and blit take about
2.1 ms at either speed. To fit 1.79 MHz in about 12.5 ms of emulation, leaving
room for GIME work and margin, the core needs about **105 clocks per cycle,
a 1.77x speed-up**. That is PIZERO-131's target.

**PIZERO-131 progress**, `pizero_cpu179` on the bench board `C58D06257AE30E85`,
about 4 minutes each at the same boot workload. Compare steps on this board
only: its baseline is 180.6 clocks, where unit 10 measured 185.4.

| step | emulation per frame | host clocks per cycle | fps |
|---|---|---|---|
| baseline (`b3f6808`) | 21.38 ms | 180.6 | 43 |
| (e) 17 run-loop helpers from flash to RAM | 21.31 ms | 180.0 | 43 |
| (a) dummy cycles skip the decode and data switch | 19.63 ms | 165.8 | 47 |
| (d) the core reads RAM, ROM and cartridge, and writes RAM, itself | 18.53 ms | 156.6 | 49 |
| (c) cycles before the next deadline skip the per-cycle tail | 12.11 ms | **102.3** | 60 |

| (c), Dungeons of Daggorath attract mode (graphics and sound) | 11.97 ms | **101.1** | 60 |

The target is met at the BASIC prompt (1.79 MHz in 72.7% of a frame for
emulation, 85.3% for the whole frame) and in a busy game: Daggorath's attract
mode, in full graphics with sound, held 60 fps for 99.2% of 4 minutes, its
worst second at 12.9 ms of emulation. One open question: a game played at the
keyboard in the contaminated 2026-10-12-2325 run took about 141 clocks per
cycle (16.69 ms), so some program or input pattern costs more; which one is
not known yet.

Target about 105.

`docs/cpu-speed.md` reached the same conclusion from the other direction: a
guest that flips the SAM into full high speed already runs ~18.8 ms of emulation
per frame and goes over budget. CoCo 3 is not a special case of that problem, it
**is** that problem, made permanent.

So even with RAM and video solved, a CoCo 3 would run at roughly **80% of real
speed**, before adding anything the GIME itself costs: MMU translation on every
memory access, the GIME timer, and its interrupt controller.

To close a 25% overrun the 6809 core needs to get about 1.6x faster once GIME
overhead is included. That is a plausible target for a dispatch rewrite on a
core we currently treat as trusted and untouched, but it is a project, not a
tuning pass, and it is the first thing that would have to succeed.

## Recommendation

_As written on 2026-09-17; see the Outcome section at the top for what
happened._

**CoCo 3 stays an aspiration, and the next step is a CPU experiment, not a
feature branch.**

_2026-09-26: picked up. The staged plan, with a correctness harness ahead of
any optimization, is in [`coco3-plan.md`](coco3-plan.md) (PIZERO-129 to 135).
It keeps the framebuffer for the first milestone, since 128 KB fits without
freeing it, and moves per-line scanout to the 640-wide follow-on._

If it is ever picked up, the order is forced by the arithmetic:

1. **Make the 6809 core faster**, and prove it with the soak harness at an
   emulated 1.79 MHz before writing any GIME code. If that fails, everything
   after it is wasted.
2. **Move to per-line scanout**, which frees 153,600 bytes and is what makes
   both a 128 KB guest and the 640-wide modes possible at all.
3. **Then** GIME: MMU, palette, timer, interrupts, and the modes in the table
   above, 320-wide first because they need no new scanout.

Scope it as **128 KB only** and say so out loud. 512 KB is not a stretch goal on
this board, it is a different board.

## What would change this

- **Populating the PSRAM pad.** It would unlock 512 KB and remove the RAM
  argument entirely, at the cost of a hardware change and PSRAM bandwidth that
  would need its own measurement.
- **A faster 6809 core**, as above, which is the single lever that matters.
- **Accepting sub-real-time.** An 80%-speed CoCo 3 is a legitimate product
  decision for some uses and a bad one for games and music, which are most of
  what this machine is for.
