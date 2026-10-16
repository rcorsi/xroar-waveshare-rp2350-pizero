#!/usr/bin/env python3
"""serial_type.py: type a BASIC listing into the board over USB serial (PIZERO-214).

The firmware types what arrives on the serial link (serial_keyboard = on, the
default), one key every 6 fields. This sends a file a line at a time and waits
for each line to be typed, plus a second after its ENTER: BASIC at 0.89 MHz
takes over a third of a second to store a long line and misses keys meanwhile
(found on the CoCo 3 by the host typist in test/test_coco3_boot). Lines that
start with ' are comments and are not sent. A '~' in a line waits a second,
as in autorun.txt.

Usage: python3 scripts/serial_type.py docs/coco3-checks/pizero214.bas [port]
"""
import glob
import sys
import time

import serial

FIELD = 1 / 60
# main.cpp pump_keyboard: the press field, 3 held (the last releases), 2 of
# gap, so a key every 6 fields; a little over, so the board never falls behind.
PER_KEY = 6.3 * FIELD


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    port = sys.argv[2] if len(sys.argv) > 2 else glob.glob('/dev/cu.usbmodem*')[0]
    lines = [l.rstrip('\r\n') for l in open(sys.argv[1])]
    lines = [l for l in lines if l.strip() and not l.startswith("'")]
    s = serial.Serial(port, 115200)
    for line in lines:
        print(line)
        parts = line.split('~')
        for i, part in enumerate(parts):
            if part:
                s.write(part.encode('ascii'))
                time.sleep(len(part) * PER_KEY + 0.2)
            if i < len(parts) - 1:
                time.sleep(1)
        s.write(b'\r')
        time.sleep(PER_KEY + 1.5)
    s.close()


if __name__ == '__main__':
    main()
