#!/usr/bin/env python3
"""Read or set the ground LoRa-UART module (Ebyte E32-433T30D) from a Mac or Linux laptop.
Replaces Ebyte's Windows-only "RF Setting" program.

1. Put both mode switches on the module DOWN (ON) = sleep / config mode (M0 = M1 = 1).
2. Plug it into USB (antenna connected first!).
3. Read:   python3 e32_config.py --port /dev/tty.usbserial-XXXX
   Write:  python3 e32_config.py --port /dev/tty.usbserial-XXXX --channel 15 --air 2400 --power 21
4. Put both switches UP = normal mode, then run the ground station at --baud 9600.

Channel C = 410 + C MHz (0..31). It must match the satellite's comm board (AT+LCHANNEL).
Air rate and FEC must also match. Power: 21 dBm on the bench, 30 dBm for the flight.
In config mode the module always talks 9600 8N1, whatever its normal UART rate is.
Needs: pip3 install pyserial
"""
import argparse, sys, time

AIR = {300: 0, 1200: 1, 2400: 2, 4800: 3, 9600: 4, 19200: 5}
BAUD = {1200: 0, 2400: 1, 4800: 2, 9600: 3, 19200: 4, 38400: 5, 57600: 6, 115200: 7}
PAR = {'8N1': 0, '8O1': 1, '8E1': 2}
PWR = {30: 0, 27: 1, 24: 2, 21: 3}


def decode(b):
    if len(b) != 6 or b[0] not in (0xC0, 0xC2):
        return f'unexpected reply: {b.hex(" ")}'
    sped, chan, opt = b[3], b[4], b[5]
    inv = lambda d, v: next((k for k, x in d.items() if x == v), '?')
    return (f'address {b[1]:02X}{b[2]:02X} | UART {inv(BAUD, (sped >> 3) & 7)} {inv(PAR, sped >> 6)} | '
            f'air {inv(AIR, sped & 7)} bps | channel {chan} = {410 + chan} MHz | '
            f'{"FIXED" if opt & 0x80 else "transparent"} mode | FEC {"on" if opt & 4 else "OFF"} | '
            f'power {inv(PWR, opt & 3)} dBm   (raw {b.hex(" ")})')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--port', required=True)
    ap.add_argument('--channel', type=int, help='0..31 (410+C MHz)')
    ap.add_argument('--air', type=int, default=2400, choices=sorted(AIR))
    ap.add_argument('--power', type=int, default=21, choices=sorted(PWR))
    ap.add_argument('--uart', type=int, default=9600, choices=sorted(BAUD), help='normal-mode UART rate for the ground script')
    ap.add_argument('--parity', default='8N1', choices=sorted(PAR))
    a = ap.parse_args()
    import serial
    s = serial.Serial(a.port, 9600, timeout=1)
    time.sleep(0.1); s.reset_input_buffer()
    s.write(bytes([0xC1, 0xC1, 0xC1])); now = s.read(6)
    print('now :', decode(now))
    if a.channel is None:
        return 0
    if not 0 <= a.channel <= 31:
        sys.exit('channel must be 0..31')
    sped = (PAR[a.parity] << 6) | (BAUD[a.uart] << 3) | AIR[a.air]
    opt = 0x40 | 0x04 | PWR[a.power]          # transparent, push-pull IO, 250 ms wake, FEC on
    s.write(bytes([0xC0, 0x00, 0x00, sped, a.channel, opt])); time.sleep(0.2); s.read(6)
    s.reset_input_buffer(); s.write(bytes([0xC1, 0xC1, 0xC1]))
    print('new :', decode(s.read(6)))
    print('Now flip both switches UP (normal mode).')
    return 0


if __name__ == '__main__':
    sys.exit(main())
