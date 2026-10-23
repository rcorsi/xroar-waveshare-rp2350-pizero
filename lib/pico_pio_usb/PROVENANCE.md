# pico_pio_usb provenance

## Upstream

- Project: Pico-PIO-USB, USB host and device over PIO on the RP2040/RP2350
- Author: sekigon-gonnoc
- Source: https://github.com/sekigon-gonnoc/Pico-PIO-USB
- Version: 0.7.2, commit `dc9193fed510da5f81f4db520e98282f61db9054`
  (2026-05-19), the revision this project had been building from `lib_deps`
- License: MIT (see `LICENSE` in this directory)

Vendored on 2026-10-23 for PIZERO-255 (sound out of a USB headphone DAC),
which needs isochronous OUT transfers the upstream host does not have. Until
then the library came unpinned from git through `lib_deps`; vendoring also
pins it. `src/`, `LICENSE`, `README.md` and `library.properties` are copied
as they were; the examples and CI files are left out.

## Local modifications (this project)

None yet at the time of vendoring: the first commit of this directory is the
upstream files unchanged, so later changes read as a diff against it.
