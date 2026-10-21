# CoCo Zero 2.0: a proposal

Written 2026-10-21 (PIZERO-244) from a discussion about ordering more boards,
fitting PSRAM, and whether CoCo Zero should one day have its own board. It is
a direction, not a plan with dates. The supplier side is in
`supplier-primer.md`.

## The goal

Make CoCo Zero as cheap and as easy to get as possible. Every decision below
is weighed against that.

## Two steps

**1.x: the Waveshare board with PSRAM fitted at the factory.** Ask Waveshare
to fit the 8 MB PSRAM chip on the existing empty footprint (U1, chip select
on GPIO 47). No board design, little or no setup charge, and Waveshare's
product is very good, so keeping them as a partner is welcome. The samples
also answer the one open technical question: whether a 512K CoCo 3 runs at
full speed with its memory in PSRAM.

**2.0: our own board,** designed around the pain points the project has
already found (below), made in small runs at a board house such as PCBWay,
once 1.x has real sales numbers to quote against.

## Why PSRAM

- **The best reason is the 512K CoCo 3.** Today's board gives a 128K CoCo 3;
  titles such as Montezuma's Revenge, Sinistar and Golaxian need 512K
  (`coco3-titles.md`). It also ends the squeeze on the chip's own memory
  (about 81% used by the product firmware).
- **It is a selling point, not a lock.** "512K CoCo 3, ready to go, no
  soldering" is a good reason to buy the kit. It will not keep copies out:
  the firmware builds on XRoar, which is GPL, so the PSRAM support is public
  like the rest; the chip is easy to fit by hand on a footprint that is
  already there; and Waveshare may well sell the variant to anyone.
- **Doing it yourself is part of the fun.** Early CoCo owners found they
  could upgrade a 4K machine to 64K. Someone soldering the PSRAM chip onto a
  stock board is in that tradition, and that is welcome.
- **The lasting edge is everything around the board:** the case, a card that
  works out of the box, tested game files, the docs, support, and being the
  original that keeps getting updates. The name "CoCo Zero" may be worth
  protecting as a trademark; that is a question for a lawyer.

## Pain points 2.0 would fix

Each one comes from work already done on the Waveshare board.

| Pain point today | What 2.0 would do |
|---|---|
| The RP2350's video block (HSTX) is on GPIO 12 to 19, which the Waveshare board does not wire to the video connector, so video runs on PIO and costs CPU time (`pipeline.md`). | Route HSTX to the video connector. Frees PIO and CPU, and may make the audio-on-some-monitors problems (PIZERO-232, 233, 234) easier to solve. |
| Keyboards and gamepads go through PIO-USB, a software USB host. It pins the CPU clock to particular speeds, and hot-plugging was never made to work (PIZERO-51). | A hardware USB host port, possibly with a small hub chip, so devices can be plugged in and out freely. |
| No PSRAM, so the CoCo 3 stops at 128K. | PSRAM fitted as standard; 512K is the normal CoCo 3. |
| The SD card runs over SPI, one data line (PIZERO-80 weighed 4-bit). | Wire the card for 4-bit mode: faster loading and screenshots. |
| The board's connectors are where a Raspberry Pi Zero has them, not where the case wants them. | Connectors placed for the case, nothing unused, perhaps a power light and a reset button. |

## Cautions

- **At small volume our own board will probably cost more per unit, not
  less.** Waveshare spreads setup over thousands of boards; a 100-board run
  pays for the stencil, assembly setup and parts in small quantities. 2.0
  gains fit and capability first. Get quotes at 50, 100 and 250 and compare
  them with the Waveshare PSRAM quote before committing.
- **A finished product may need certification.** A board sold as our own
  device can bring testing requirements (in the US, FCC Part 15 for anything
  clocked this fast) that a stock development board in a kit may avoid. Find
  out how kit sellers in the target markets handle it before the design gets
  far.
- **The firmware must keep running on the Waveshare board.** 1.x owners
  should keep getting updates; 2.0 differences belong behind a board setting
  or build option, not a split project.

## Next steps

1. Send the Waveshare request in `supplier-primer.md` and order samples.
2. Measure the 512K CoCo 3 from PSRAM on the samples.
3. Sell 1.x with PSRAM; keep notes on what buyers struggle with, since that
   list feeds 2.0.
4. When the numbers justify it, sketch the 2.0 board and get board-house
   quotes to compare.
