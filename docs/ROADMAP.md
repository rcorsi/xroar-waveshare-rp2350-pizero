# ROADMAP

Where the RP2350-PiZero XRoar port stands and what's next. This is a navigation
map over `issues.jsonl`: it sequences the open work and links the tickets; it
does **not** restate ticket detail. Authoritative status always lives in the
ticket (`PIZERO-NN`) and the deep docs (`README.md`, `docs/`). The short ranked
list of what to pick up next is `roadmap.jsonl` (shown at the top of
`issues.html`).

_Last updated: 2026-10-11 (tracker clean-up after the stability and NitrOS-9
work; the focus moves back to features)._

## Where things stand

From late September to early October the work was mostly features (58 issues
closed in two weeks, 33 of them features, 5 bugs). The ten days to 2026-10-10
were bring-up and stability: NitrOS-9, writable drives, and the first
laptop-free soak. That soak came out well, so the stability work now runs in
the background and features are the focus again.

## Shipped / working

- **Video**: libdvi/PIO DVI, 320×240 → true **640×480p60** at 252 MHz
  (`PIZERO-45`); CoCo VDG render + blit, NTSC artifact color (`PIZERO-43`), F8
  cycles artifact colors per game (`PIZERO-186`). Screen fonts: the classic set,
  the 6847T1, and our 6847T2 by default (`PIZERO-166`).
- **Audio**: streaming data-island audio, the default since `PIZERO-45`. The
  rate servo (`PIZERO-121`) held the ring at its set-point with **0 skips over
  26.7 h**. SN76489 sound chip (`PIZERO-143`).
- **CoCo 3 extras**: the GIME palette at `$FFB0-$FFBF` (`PIZERO-85`, `55`) and
  the GIME timer at `$FF90-$FF95` (`PIZERO-62`), both on by default.
- **Storage**: microSD ROM/disk/cartridge load and autorun (`PIZERO-182`, `183`:
  drives remembered across power-off). **Writable `.DSK` drives**: SAVE from
  BASIC and NitrOS-9, W lock per drive, N makes a named blank disk, R renames
  (`PIZERO-66`, `193`, `194`, `196`). Firmware whole-file writes for settings,
  per-game files and Print Screen PNGs (`PIZERO-67`, `146`, `165`).
- **NitrOS-9 Level 1 boots** to `OS9:` and saves survive a power cycle
  (`PIZERO-190`, `191`, `194`). `scripts/mkos9disk.py` builds a working disk
  (`PIZERO-199`, booted on the board 2026-10-12).
- **Overlay**: F9 to F12 open programs, cartridges, files and disks
  (`PIZERO-81`, `114`, `153`); F1 INFO page (`PIZERO-156`); per-game `NAME.TXT`
  (`PIZERO-154`); `settings.txt` and `autorun.txt` edited on screen
  (`PIZERO-145`, `146`, `147`); the pad drives the overlay (`PIZERO-169`).
- **Cartridges** of 2 to 16 KB (`PIZERO-139`); bank-switched carts over 16 KB
  are built and host-tested, waiting on a test image (`PIZERO-142`).
- **Input**: USB keyboard that types what the keycaps say, keypad, auto-repeat
  (`PIZERO-163`, `49`, `167`); USB gamepad as both joysticks, buttons as keys
  (`PIZERO-13`, `160`, `164`); keyboard and pad together through a simple hub
  (`PIZERO-54`). A device straight into the board must be attached at power-on
  (`PIZERO-51`). Lessons: `docs/usb-retrospective.md`.
- **Stability**: watchdog auto-recovery with phase, sub-phase and reset reason
  carried across the reset (`PIZERO-33`, `203`). The 26.7 h laptop-free soak
  (`PIZERO-168`, logged to the card by `PIZERO-201`) measured **0.075
  freezes/h**, against 0.46/h in September (`PIZERO-98`). Figures in the
  README's "Stability, measured".
- **Trust tooling**: 237 host tests in about 9 s (`pio test -e native`,
  `PIZERO-109`), including a 6809 lockstep harness that runs the live core
  against a frozen reference (`PIZERO-129`). `scripts/soak.py` records and
  analyses soaks; `scripts/qc_flash.py` flashes and logs a batch of boards
  (`PIZERO-189`, 12 boards so far).
- **Docs**: README, SETTINGS, AUTORUN, BUILD, pipeline, video-audio-notes,
  cpu-speed, kit, coco3-plan, usb-retrospective, show-prep-retrospective.

## In flight (waiting on a hardware step)

These need the board or the user rather than more code (`PIZERO-199`, NitrOS-9
on the board, passed 2026-10-12):

- **`PIZERO-203`: the frame-pace recoveries.** The one live stability bug. A
  re-soak with deliberate overlay use is running; the serial-stall theory is
  tested and ruled out. Keep it soaking on one board while features are built
  on the other.
- **`PIZERO-142`**: start a 32 KB and a 64 KB banked cart (Mind Roll, Dunjunz)
  once an image is found.

## Now: features, starting with the CoCo 3 track

The groundwork is done: `PIZERO-129`'s lockstep harness exists so the 6809 can
be made faster without changing what it does. The plan and its reasoning are in
`docs/coco3-plan.md`.

1. **`PIZERO-130`** (done 2026-10-12): 185.4 host clocks per emulated cycle; 1.79 MHz takes 131.7% of a frame. Numbers in `docs/coco3-feasibility.md`.
2. **`PIZERO-131`** (done 2026-10-13): 185.4 to about 101 host clocks per cycle; 1.79 MHz fits a 60 fps frame, so the gate is met. Remainders: `PIZERO-210` (one game at about 141), `PIZERO-211` (13 KB of RAM).
3. **`PIZERO-135`**: the open decisions (how the mode is chosen, audio below
   real time). Settle them before 133.
4. **`PIZERO-133`** (done 2026-10-13): env `pizero_coco3` boots a 128 KB CoCo 3
   to OK on hardware, at about half speed (29 fps, 35 ms of CPU per frame).
5. **`PIZERO-213`** (done 2026-10-13): the MMU fast path and a per-row redraw skip;
   58 fps at OK at 0.89 MHz, 48 at 1.79. **`PIZERO-218`**: the GIME's video, the
   rest of the frame. Then
   `PIZERO-214` (check WIDTH 40, palette, timer, HSCREEN), `PIZERO-215` (ALT,
   CTRL, F1, F2, fire2; includes `PIZERO-205`'s F1 caveat), `PIZERO-178` (GIME
   render cost per mode), `PIZERO-134` and `PIZERO-179` (640-wide modes and the
   path to show them), `PIZERO-216` and `PIZERO-217`.

Related CPU-speed bug: **`PIZERO-132`** (SAM double speed is never applied).

## Next: experience features (small, unblocked)

Good change-of-pace work between CoCo 3 stages. None waits on anything.

- **`PIZERO-82`**: boot back into the last game (`laststate.txt`), with a
  documented precedence against autorun. Pairs with **`PIZERO-188`** (what
  autorun is for).
- **`PIZERO-117`**: after mounting a disk, offer to run what is on it.
- **`PIZERO-171`**: attract mode, a playlist of demos when idle.
- **`PIZERO-177`**: a one-line `classic` setting for an authentic CoCo 2.
- **`PIZERO-173`**: type `{ } | ~` and paste faster through BASIC's keyboard
  routine. **`PIZERO-197`**: Caps Lock chord for NitrOS-9.
- **`PIZERO-205`**: the F1 help reader (CoCo-styled Markdown, built-in help,
  card documents). **`PIZERO-206`**: the rest of the INFO facts.
- **`PIZERO-57`**: load a `.FNT` from the card (the built-in font choice
  shipped as `PIZERO-166`).
- Media: **`PIZERO-138`** (`.cas` in the overlay).

## Add-on hardware (bigger, unstarted)

- **Sound**: `PIZERO-148` (Orchestra-90CC), `PIZERO-149` (AY-3-8910).
- **Video**: `PIZERO-16` (TMS9918A), `PIZERO-176` (MC6845 80-column card);
  the new-device chain `PIZERO-83` (decision) → `PIZERO-84` (linear 320×240) and
  `PIZERO-89` (wide text).
- **Joystick on the header pins**: `PIZERO-50`, reusing the joystick API
  `PIZERO-13` built.

## Storage & filesystem layer (direction set 2026-08-05)

The long-term storage model is a real filesystem with the 6809 side kept thin;
floppy emulation becomes peripheral support (`PIZERO-79`). Writable `.DSK`
drives shipped first (`PIZERO-66`), and they write synchronously because a SAVE
costs at most 2.1 ms per sector and 6 ms per sync (so `PIZERO-65`, the deferred
queue, closed as not needed).

The chain, bottom to top: `PIZERO-70` (backend interface) → `PIZERO-71` (VFS)
→ `PIZERO-72` (hypercall ABI, the contract and freeze point) → `PIZERO-73`
(replacement cart ROM) → `PIZERO-74` (guest commands). Backends need only 70:
`PIZERO-75` (RAM disk, build it early as the test target), `PIZERO-76`
(DriveWire), `PIZERO-77`, `78` (deferred). `PIZERO-113` (power loss mid-write)
matters now that the guest writes to the card. `PIZERO-69` (save-states) and
`PIZERO-68` (DECB maintenance) are deferred.

## Fixes and checks still owed (low urgency)

- **Bugs**: `PIZERO-202` (volume ignores the DAC path), `PIZERO-158` (serial
  typing drops a character), `PIZERO-195` (audio blip on launch), `PIZERO-204`
  (clock seeded hours ahead), `PIZERO-100` (SAM base `$0000`), `PIZERO-198`
  (fdctrace env crash), `PIZERO-159` (chained USB-C hubs), `PIZERO-157` (pad
  Switch mode).
- **Hot-plug**: `PIZERO-51`. Software paths are disproved; test unplug and
  replug through the hub before considering a VBUS switch mod.
- **Other screens**: `PIZERO-99`, and `PIZERO-120` (the centre band on one LCD;
  palette-walk experiment next). `PIZERO-32` (ACR on sinks that honor it).
- **Audio by ear**: `PIZERO-41`, a listening test against desktop XRoar now that
  timing and the servo changed under it.
- **Hardware sightings**: `PIZERO-137`, `144`, `151`, `155`, `200`.
- **Tooling**: `PIZERO-110` (on-board self-test), `PIZERO-207` (ring accounting
  test), `PIZERO-88` (frame-time headroom, refresh), `PIZERO-42` (remove code
  behind retired flags), `PIZERO-106` (issues.html tag pills, user check).
- **Kit and case**: `PIZERO-93` (power and keyboard advice in `kit.md`),
  `PIZERO-97`, `PIZERO-102`, `PIZERO-185` (no-name SD cards).

## Later / stretch (deferred)

- **`PIZERO-56`**: higher-fidelity NTSC artifacts (the current path is a
  deliberate 2-bit LUT, not composite decoding).
- **Synth and MIDI**: `PIZERO-58` (voice-budget bench) → `PIZERO-17`
  (wavetable synth device); `PIZERO-61` (Lyra's serial MIDI driver) →
  `PIZERO-59` (MIDI bus over the bit-banger port) → `PIZERO-60` (USB-MIDI and
  DIN endpoints). All build-flag gated; `PIZERO-170` (GIME timer reload quirk)
  sets the tempo of timer music.
- **`PIZERO-128`**: the BOOT button as an overlay key. **`PIZERO-80`**: SDIO.

## Dependency notes

- **CoCo 3**: `PIZERO-129` (done) → `130` → `131` (gate) → `133` (done) → `213` (done) → `218`;
  `135` still decides how the mode is chosen; `134`/`179`/`214`-`217` after 133. `PIZERO-142`'s banked carts allocate on the
  heap only while installed, so they do not take the RAM 133 needs.
- **Storage**: `PIZERO-70` → `71` → `72` → `73` → `74`; `PIZERO-79` needs 73
  for the default cart. `PIZERO-62`'s GIME timer occupies `$FF90-$FF95`, so
  `PIZERO-72`'s hypercall registers must go elsewhere.
- **Joystick**: the injection API from `PIZERO-13` is the shared root;
  `PIZERO-50` reuses it.
- **Fonts**: `PIZERO-57`'s remembered choice uses the existing whole-file write.
