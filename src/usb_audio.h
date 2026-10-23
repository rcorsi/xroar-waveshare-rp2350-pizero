// usb_audio.h: sound out of a USB Audio Class headphone DAC (PIZERO-255).
//
// When a device mounts, its configuration descriptor is read and searched
// for a playback interface that takes 16-bit stereo PCM at 48 kHz on an
// isochronous OUT endpoint (UAC1 or UAC2). If there is one, the driver
// selects that alternate setting, sets the sampling rate (UAC1: the
// endpoint's SAMPLING_FREQ; UAC2: the clock source's), opens the endpoint
// and starts Pico-PIO-USB's isochronous stream, which asks for 48 sample
// pairs every 1 ms frame.
//
// While it streams, the sound goes to the DAC and the video carries silence;
// with no DAC at boot, unplugged, or usb_audio = off, it stays on the video.
// Built only with USB_AUDIO (the streaming builds). With USB_AUDIO_TONE the
// stream is a 440 Hz test tone rather than the machine's sound.

#pragma once
#include <stdint.h>

#ifdef USB_AUDIO
void usb_audio_mount(uint8_t daddr);      // from tuh_mount_cb
void usb_audio_umount(uint8_t daddr);     // from tuh_umount_cb
void usb_audio_enable(bool on);           // the usb_audio setting
void usb_audio_set_db(int db);            // usb_audio_db: the DAC's own level, -60 to 0 dB, at setup
void usb_audio_telemetry(uint32_t under); // once a second: [uac] ...

// Supplied by main.cpp: a packet of the machine's sound (48 left and right
// pairs), called from the USB frame interrupt, and the switch that gives
// the ring to USB (and silence to the video) or back.
uint16_t usb_audio_fill(uint8_t *buf, uint16_t max);
void usb_audio_route(bool usb);
#endif
