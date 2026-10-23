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
// Built only with USB_AUDIO. With USB_AUDIO_TONE the stream is a 440 Hz
// test tone rather than the machine's sound.

#pragma once
#include <stdint.h>

#ifdef USB_AUDIO
void usb_audio_mount(uint8_t daddr);      // from tuh_mount_cb
void usb_audio_umount(uint8_t daddr);     // from tuh_umount_cb
void usb_audio_telemetry(void);           // once a second: [uac] ...
#endif
