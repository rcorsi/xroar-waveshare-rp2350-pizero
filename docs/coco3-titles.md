# CoCo 3 titles on the CoCo Zero

What real CoCo 3 software does on the board, title by title (PIZERO-228). Run
on 2026-10-19 with the `pizero_coco3` firmware, a 128 KB CoCo 3, from disks on
the card, launched from the **F12** list.

## Results

| Title | Disk | Start | Result |
|---|---|---|---|
| Space Raid | `SPACE-RAID.DSK` | `RUN"START"` | Works: loads and plays. |
| Starcast | `STARCAST.DSK` | autostarts | Works. |
| Galactus | `galactus-2.dsk` | autostarts | Works. |
| Golaxian | `GOLAXIAN.DSK` | autostarts | Needs 512K. Checks the memory and says so ("512K REQUIRED", "128K DETECTED"). |
| Montezuma's Revenge | `montrev.dsk` | `RUN"MONTREV"` | Needs 512K (this copy). Title screen shows; ENTER gives `?IE ERROR` with a nonsense line number. |
| Sinistar preview v0.02 | `sinistar.dsk` | `RUN"SINI"` | Needs 512K (its readme says so). Hangs loading `M4TAIL.BNK`. Its first screen is 80 columns and unreadable here. |
| NitrOS-9 Level 2 | `nos9l2.dsk` | `DOS` | Boot fails. The image is for CoCo SDC hardware, not a floppy (PIZERO-236). |

## What it shows

- **128K games run.** All three that fit in 128K work.
- **512K games do not, and cannot on this board.** The board has no room for
  the extra 384K (its PSRAM chip is not fitted; see `coco3-feasibility.md`).
  A game that checks, like Golaxian, says so. One that does not, like
  Montezuma's Revenge and Sinistar, maps memory pages that only exist on 512K.
  On 128K those page numbers wrap onto memory already in use, so it crashes or
  hangs, as it would on a real 128K CoCo 3.
- **80-column screens are not readable yet.** 640-wide modes are squeezed into
  the 320-wide picture, and text breaks up (PIZERO-134).

## Why the two 512K failures look the way they do

Both were read off the disks, not guessed:

- **Montezuma's Revenge:** `LOADDATA.BIN` sets the memory page for
  `$4000-$5FFF` (`$FFA2`) to pages `$08`-`$11` between its segments. On 128K
  page `$08` wraps to `$38`, which is BASIC's own `$0000-$1FFF`, so the load
  overwrites Disk BASIC mid-read.
- **Sinistar:** after each `.BNK` loads, `EXEC &H3000` copies it into pages
  `$26` and `$1E`, which wrap to `$36` and `$3E` on 128K.

## Still to try

- NitrOS-9 Level 2 from a floppy built for the board (PIZERO-236).
- Something whose music runs off the GIME timer (feeds PIZERO-170).
- A 40-column program.
- Games that need ALT, CTRL, F1 or F2 (PIZERO-215).
