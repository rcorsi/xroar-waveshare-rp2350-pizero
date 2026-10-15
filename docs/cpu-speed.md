# CoCo high speed (SAM double speed) and emulation pacing

Short answer: **the CoCo 2 high-speed POKEs work** (PIZERO-132, 2026-10-14). A
program that pokes the SAM's rate runs faster, as it would on a real CoCo, with
60 fps video and full sound. The machine still starts at normal speed: nothing
speeds it up unless the program asks.

## The POKEs

There is no host-side fast-forward or turbo key. The only speed-up is the one a
CoCo 2 program asks for itself, through the SAM's MPU rate bits:

| POKE | SAM rate | What runs fast | Measured speed-up |
|---|---|---|---|
| `POKE 65494,0` and `POKE 65496,0` | R = 0 | nothing (normal, 0.89 MHz) | 1× |
| `POKE 65495,0` | R = 1, address-dependent | everything except RAM and PIA0 ($FF00-$FF1F): ROM, cartridge, other I/O, dummy cycles | 1.62× on a BASIC `FOR` loop |
| `POKE 65497,0` | R = 2, full | every cycle (1.79 MHz) | 2.00× |

The measurement was a `10 TIMER=0:FOR I=1 TO 3000:NEXT:PRINT TIMER` loop: 188
jiffies at normal speed, 116 at `POKE 65495`, 94 at `POKE 65497`. How much
address-dependent speed gains depends on the program: BASIC runs from ROM, so it
gains most; machine code running from RAM gains least.

**Reset** (the board's RUN button, or the overlay's reset) clears the rate, as the
real SAM's reset does, so a program that left the machine fast does not carry
the speed into the next one.

### Better than the real machine: the screen stays clean

On a real CoCo 2, full speed (`POKE 65497`) takes the SAM's time away from the
VDG and the screen fills with garbage until the program pokes back to normal.
Here the picture is drawn from RAM once a frame, independent of the CPU's rate,
so the screen stays readable at full speed. Programs that blank or ignore the
screen while fast are unaffected; programs that relied on the garbage as a cue
will not see it.

### The CoCo 3 build

The CoCo 3 build honours its own 1.79 MHz bit (`POKE 65497,0`, the GIME's copy
of the SAM rate) through the GIME emulation, and has since PIZERO-133. A
full-screen animation at 1.79 MHz holds 60 fps with no late frames (PIZERO-220).

## How it is emulated

A bus cycle's length is counted in event ticks of the 14.318 MHz master clock:
**16 ticks** for a normal cycle, **8** for a fast one. `sam_ticks` in
`lib/coco_machine/src/coco_machine.cpp` decides it from the rate the SAM holds,
using the same rule as XRoar's `mc6883_mem_cycle` (`lib/xroar_core/src/mc6883.c`,
its `fast_cycle`): fast when R1 is set, or when R = 1 and the cycle is neither
RAM nor PIA0. In all-RAM mode (SAM TY), everything below $FF00 counts as RAM.

The pieces:

- **The rate is shadowed** in `g_m.sam_r` when the guest writes $FFD6-$FFD9, next
  to the existing shadows of TY, F and V. A SAM register write takes the length
  set by the rate before the write, as on the chip.
- **The 6809 core's fast path** (PIZERO-131) moves RAM and ROM bytes without the
  address decode. `struct coco_fastbus` (`lib/xroar_core/include/coco_fastbus.h`)
  now carries two lengths: `ticks` for its RAM window, and `rom_ticks` for its ROM
  and cartridge windows and for dummy cycles. `fastbus_sync` sets both from the
  rate. They differ only at address-dependent speed.
- **The audio catch-up** integrates runs of cycles that had no event in them. At
  address-dependent speed one run mixes 8- and 16-tick cycles, so the catch-up
  takes the ticks 8 at a time. At a steady DAC level the integrator gives exactly
  the same samples however the ticks are split; `test_audio_integ`'s
  `test_mixed_lengths_caught_up_in_eights` checks that sample for sample.
- **Not modelled:** the real SAM spends 15, 17 or 25 ticks on a cycle where the
  rate changes between fast and slow (re-interleaving with the VDG). Those even
  out to within a few percent of the 8/16 model over a frame.

## The pacing model: fixed emulated time per frame

The main loop does **not** run a fixed number of instructions a frame. It runs a
fixed amount of **emulated time**, `CYCLES_PER_FRAME × 16 = 14915 × 16 = 238,640`
event ticks, which is 16.667 ms. `coco_machine_run_cycles` stops the CPU when time
reaches that budget (`g_budget_end`), and each cycle spends its own length. At full
speed twice as many cycles fit into the same frame, which is exactly what the real
machine does.

That is why speed changes are safe for the rest of the system:

- **Sound pitch** stays right. Samples are placed by event tick, and a frame is
  always the same number of ticks, so it is still 800 samples a frame at any rate.
- **Video** stays at 60 Hz. Core 1's video output is decoupled from the guest (see
  [`pipeline.md`](pipeline.md)), and the frame is rendered once per 16.667 ms of
  emulated time whatever the rate.

## What it costs: core 0's headroom

The host does roughly twice the work per frame when every cycle is fast, so the
question was whether it fits in 16.67 ms. Measured on `pizero_stream_60`
(2026-10-14, a BASIC loop, `[run] cpu`):

| Rate | Emulation per frame | Late frames | Audio short |
|---|---|---|---|
| Normal | 6.2 ms | 0 | 0.0% |
| `POKE 65495` | 12.4 ms (BASIC, mostly ROM) | 0 | 0.0% |
| `POKE 65497` | 12.6 ms | 0 | 0.0% |

Render and blit add about 2 ms, so full speed leaves about 2 ms of a 16.67 ms
frame spare. Honouring the rate cost the normal-speed case about 0.1 ms a frame
(6.1 to 6.2 ms), because the core's fast path now reads the cycle length instead
of a constant, and about 2 KB of RAM (`pizero_stream_60` 421,376 bytes).

If a future change pushes full speed past the frame, the pacing loop in
`loop()` (`src/main.cpp`) drops the deficit rather than catching up: the guest
runs slower than 2×, and the audio ring under-fills and underruns. The `[run]`
line's `late` and `cmax`, and the `[aud]` line's `short`, show it.

## History

- **PIZERO-48** (2026-06) wrote up the pacing model and predicted full speed would
  overrun the frame, at about 18.8 ms of emulation.
- **PIZERO-132** (2026-09-26) found the POKEs had never worked: the fast-path
  decode inherited from the AMOLED port charged a flat 16 ticks for every access.
- **PIZERO-130 and PIZERO-131** measured a 2× machine and made the 6809 core fast
  enough to run one (`pizero_cpu179`, about 12 ms a frame).
- **PIZERO-132** (2026-10-14) honoured the rate, and the measurements above
  replaced the prediction.

## See also

- [`pipeline.md`](pipeline.md): the decoupled two-core pipeline this relies on
- [`video-audio-notes.md`](video-audio-notes.md): the audio ring and its metering
- [`../README.md`](../README.md): Performance section
