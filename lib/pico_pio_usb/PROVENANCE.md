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

The first commit of this directory (29f10b0) is the upstream files unchanged,
so the changes read as a diff against it. All are marked `PIZERO-255 (this
project)` in the source:

- `pio_usb.c`, `pio_usb_ll.h`: `pio_usb_ll_encode_tx_data` takes and returns
  16-bit lengths and indexes its output with a 16-bit index. The upstream
  `uint8_t` wrapped for any packet that encodes past 255 bytes, which a
  192-byte isochronous audio packet does (about 460 bytes).
- `usb_definitions.h`: `endpoint_t.encoded_data_len` is `uint16_t`.
- `pio_usb_host.c`, `pio_usb.h`: an isochronous OUT stream,
  `pio_usb_host_iso_out_stream(root, address, endpoint, fill)` and
  `pio_usb_host_iso_frames()`. The 1 ms frame handler calls `fill` for the
  frame's packet, builds DATA0 and its CRC16, encodes it into its own buffer
  (up to `PIO_USB_ISO_MAX` bytes) before sending the OUT token, then sends the
  packet and waits for no handshake. Upstream had no isochronous support: an
  OUT transaction always waited for a handshake.
