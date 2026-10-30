#!/usr/bin/env python3
"""SnailPulse link tool: test one side of the FC <-> comm board link from a laptop.

The link (see ../README.md): the flight controller (FC) sends one frame on UART1 at 115200 8E1,
the comm board answers one byte 0x06 (ACK) when the frame has gone out over LoRa.
This tool plays either side, through a 3.3 V USB-UART adapter (CP2102, CH340, FT232 set to 3.3 V):

  fake-comm   the laptop is the comm board. Wire the FC's UART1 to the adapter:
              FC PA9 (TX) -> adapter RX,  FC PA10 (RX) <- adapter TX,  GND - GND.
              Shows every frame the FC sends (decoded), answers 0x06 like the real comm board.
              Use it to test the FC side alone: SnailPulse flight firmware or LinkTest_FC.

  fake-fc     the laptop is the FC. Wire the adapter to the comm board's FC UART (IntroBus_S UART1 lines):
              adapter TX -> comm board RX,  adapter RX <- comm board TX,  GND - GND.
              Sends SnailPulse test frames with the flight timing and measures how fast the 0x06 comes back.
              Use it to test the comm board (stock or your own firmware) without our FC.

  selftest    no hardware: runs fake-fc against fake-comm in memory. Checks this script works.

Examples:
  python3 sp_link_tool.py fake-comm --port COM5
  python3 sp_link_tool.py fake-comm --port /dev/ttyUSB0 --ack-delay 300 --drop 10 --save fc_capture.bin
  python3 sp_link_tool.py fake-fc   --port COM5 --flood
  python3 sp_link_tool.py selftest

Needs Python 3.8+ and pyserial (pip install pyserial) for the two hardware modes.
Never connect the adapter to 5 V logic: the FC and comm board pins are 3.3 V.
Antenna on the comm board BEFORE it is powered: transmitting without one destroys the E32.
"""
import argparse, math, os, random, struct, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "ground"))
from snailpulse_ground import Parser, PACKETS, crc16_ccitt, decode   # noqa: E402

ACK = 0x06
ACK_TIMEOUT_S = 1.5        # same as the flight firmware (radio.h)
QUEUE_MAX = 8              # flight firmware queue; when full the oldest frame is dropped
PKT = {"BASE": 0x01, "SNAIL": 0x10, "ENV": 0x11, "STATUS": 0x7F}


# ------------------------------------------------------------------ frames
def build_frame(ptype, seq, payload):
    body = bytes([ptype, seq & 0xFF, len(payload)]) + payload
    return b"\xAA\x55" + body + struct.pack("<H", crc16_ccitt(body))


def pack(ptype, **vals):
    """Payload with the flight firmware's layout; fields not given are 0 (byte fields: empty)."""
    _, fmt, names = PACKETS[ptype]
    args, num = [], ""
    for c in fmt.lstrip("<"):          # "3h" is three values with three names; "20s" is one bytes value
        if c.isdigit():
            num += c
            continue
        for _ in range(1 if c == "s" else int(num or 1)):
            args.append(vals.get(names[len(args)], b"" if c == "s" else 0))
        num = ""
    return struct.pack(fmt, *args)


def test_payload(ptype, t_ms, n, sent=0, missed=0, dropped=0):
    """The same test values as LinkTest_FC.ino: easy to recognise on the ground."""
    if ptype == PKT["SNAIL"]:
        wave = bytes(128 + int(40 * math.sin(i * 0.6)) for i in range(20))
        return pack(ptype, t_ms=t_ms, bpm_x10=300, last_ibi_ms=2000, beats_total=n, quality=80, amp_counts=200,
                    dc_counts=2000, wave=wave, wave_n=20, max_bpm_x10=450, max_beats_total=n, max_quality=70,
                    max_amp=150, max_ir=20000)
    if ptype == PKT["BASE"]:
        return pack(ptype, t_ms=t_ms, press_pa=100129, baro_t_c100=2000, alt_dm=1000 + 10 * n, az_mg=1000,
                    fc_t_x8=8 * 25)
    if ptype == PKT["ENV"]:
        return pack(ptype, t_ms=t_ms, box_t_c100=-32768, box_rh_c100=0xFFFF, box_press_pa=101300, bmp_t_c100=2150,
                    heater_t_c100=-32768, sensors_ok=0x76, dht_t_c100=2200, dht_rh_c100=4500, mq7_mv=1000,
                    bmp2_t_c100=-32768)
    return pack(ptype, t_ms=t_ms, fw_version=0x0105, boot_count=0xC0DE, frames_sent=sent, acks_missed=missed,
                frames_dropped=dropped, fc_ok=0x0F)


def summary(ptype, payload):
    name, d = decode(ptype, payload)
    if d is None:
        return f"{name or hex(ptype)} ({len(payload)} B, unknown layout)"
    if name == "SNAIL":
        return f"SNAIL  snail1 {d['bpm']:.1f} bpm  snail2 {d['max_bpm']:.1f} bpm IR {d['max_ir']}"
    if name == "BASE":
        return f"BASE   {d['press_pa'] / 100:.2f} mbar  alt {d['alt_dm'] / 10:.1f} m"
    if name == "ENV":
        return f"ENV    BMP280 {d['box_press_pa'] / 100:.1f} mbar {d['bmp_t_c100'] / 100:.2f} C  ok=0x{d['sensors_ok']:02X}"
    v = d["fw_version"]
    return (f"STATUS fw {v >> 8}.{v & 0xFF} boot {d['boot_count']:#x} sent {d['frames_sent']} "
            f"noACK {d['acks_missed']} drop {d['frames_dropped']}")


# ------------------------------------------------------------------ the two sides
class FakeComm:
    """Behaves like the IntroSat comm board towards the FC: 0x06 after each good frame."""
    def __init__(self, port, ack_delay_ms=150, no_ack=False, drop_pct=0.0, save=None, verbose=True, log=print):
        self.port, self.delay, self.no_ack, self.drop = port, ack_delay_ms / 1000, no_ack, drop_pct
        self.save = open(save, "ab") if save else None
        self.verbose, self.log = verbose, log
        self.parser = Parser()
        self.acks_due = []                 # times to send 0x06
        self.frames = self.acked = self.not_acked = self.lost = 0
        self.last_seq = None
        self.bytes_in = 0

    def step(self, now):
        data = self.port.read(256)
        if data:
            self.bytes_in += len(data)
            if self.save:
                self.save.write(data)
            for ptype, seq, payload in self.parser.feed(data):
                self.frames += 1
                if self.last_seq is not None and seq != (self.last_seq + 1) & 0xFF:
                    self.lost += (seq - self.last_seq - 1) & 0xFF
                self.last_seq = seq
                skip = self.no_ack or random.random() * 100 < self.drop
                if skip:
                    self.not_acked += 1
                else:
                    self.acks_due.append(now + self.delay)
                if self.verbose:
                    self.log(f"#{seq:3d} {7 + len(payload):2d} B  {summary(ptype, payload)}"
                             f"{'   (no ACK on purpose)' if skip else ''}")
        while self.acks_due and now >= self.acks_due[0]:
            self.acks_due.pop(0)
            self.port.write(bytes([ACK]))
            self.acked += 1

    def report(self):
        return (f"frames {self.frames}  ACKed {self.acked}  not ACKed {self.not_acked}  "
                f"seq gaps {self.lost}  CRC errors {self.parser.crc_errors}  bytes in {self.bytes_in}")


class FakeFc:
    """Sends frames like the flight firmware: one at a time, next after 0x06 or 1.5 s."""
    def __init__(self, port, flood=False, verbose=True, log=print):
        self.port, self.flood, self.verbose, self.log = port, flood, verbose, log
        self.t0 = None
        self.queue = []                    # (ptype, frame bytes)
        self.seq = 0
        self.n = {k: 0 for k in PKT.values()}
        self.next_at = {}
        self.in_flight = None              # (seq, ptype, sent time)
        self.sent = self.acked = self.missed = self.dropped = 0
        self.lat = []
        self.other = bytearray()

    def enqueue(self, ptype, now):
        t_ms = int((now - self.t0) * 1000)
        payload = test_payload(ptype, t_ms, self.n[ptype], self.sent, self.missed, self.dropped)
        self.n[ptype] += 1
        if len(self.queue) >= QUEUE_MAX:
            self.queue.pop(0)
            self.dropped += 1
        self.queue.append((ptype, build_frame(ptype, self.seq, payload), self.seq))
        self.seq = (self.seq + 1) & 0xFF

    def step(self, now):
        if self.t0 is None:
            self.t0 = now
            self.enqueue(PKT["STATUS"], now)              # boot STATUS, like the flight firmware
            self.next_at = {PKT["SNAIL"]: now, PKT["BASE"]: now, PKT["ENV"]: now, PKT["STATUS"]: now + 10}
        period = {PKT["SNAIL"]: 2, PKT["BASE"]: 2, PKT["ENV"]: 4, PKT["STATUS"]: 10}
        if self.flood:
            if len(self.queue) < 2:
                for t in (PKT["SNAIL"], PKT["BASE"], PKT["ENV"], PKT["STATUS"]):
                    self.enqueue(t, now)
        else:
            for t, at in self.next_at.items():
                if now >= at:
                    self.enqueue(t, now)
                    self.next_at[t] = at + period[t]
        for b in self.port.read(256):
            if b == ACK and self.in_flight:
                ms = (now - self.in_flight[2]) * 1000
                self.lat.append(ms)
                self.acked += 1
                if self.verbose:
                    self.log(f"#{self.in_flight[0]:3d} ACK after {ms:.0f} ms")
                self.in_flight = None
            elif b != ACK and len(self.other) < 64:
                self.other.append(b)
        if self.in_flight and now - self.in_flight[2] > ACK_TIMEOUT_S:
            self.missed += 1
            if self.verbose:
                self.log(f"#{self.in_flight[0]:3d} no ACK within {ACK_TIMEOUT_S} s")
            self.in_flight = None
        if not self.in_flight and self.queue:
            ptype, frame, seq = self.queue.pop(0)
            self.port.write(frame)
            self.sent += 1
            self.in_flight = (seq, ptype, now)

    def report(self):
        s = f"sent {self.sent}  ACKed {self.acked}  no ACK {self.missed}  dropped {self.dropped}  queued {len(self.queue)}"
        if self.lat:
            s += f"  ACK time min/avg/max {min(self.lat):.0f}/{sum(self.lat) / len(self.lat):.0f}/{max(self.lat):.0f} ms"
        if self.other:
            s += "  other bytes from comm board: " + " ".join(f"{b:02X}" for b in self.other[:16])
        return s


# ------------------------------------------------------------------ in-memory link for selftest
class Pipe:
    def __init__(self):
        self.a_to_b, self.b_to_a = bytearray(), bytearray()

    def end(self, a):
        pipe = self

        class End:
            def read(self, n):
                buf = pipe.b_to_a if a else pipe.a_to_b
                d = bytes(buf[:n]); del buf[:n]
                return d

            def write(self, d):
                (pipe.a_to_b if a else pipe.b_to_a).extend(d)
        return End()


def selftest():
    ok = True
    for name, kw in (("normal", {}), ("comm board ignores 30 % of frames", {"drop_pct": 30})):
        random.seed(1)
        p = Pipe()
        fc, comm = FakeFc(p.end(True), verbose=False), FakeComm(p.end(False), ack_delay_ms=100, verbose=False, **kw)
        t = 0.0
        while t < 21.0:                     # simulated clock, 1 ms steps
            fc.step(t); comm.step(t); t += 0.001
        # 21 s of flight timing: 11 SNAIL + 11 BASE + 6 ENV + 3 STATUS = 31 frames
        good = comm.frames == fc.sent and comm.parser.crc_errors == 0 and comm.lost == 0
        if not kw:
            good = good and fc.sent == 31 and fc.missed == 0 and fc.acked == 31 and 99 <= min(fc.lat) <= 102
        else:
            good = good and comm.not_acked > 0 and fc.missed <= comm.not_acked <= fc.missed + 1 and fc.acked + fc.missed + (fc.in_flight is not None) == fc.sent
        print(f"{'PASS' if good else 'FAIL'}  {name}\n      fake FC:   {fc.report()}\n      fake comm: {comm.report()}")
        ok &= good
    f = build_frame(PKT["ENV"], 2, test_payload(PKT["ENV"], 6000, 0))
    frames = list(Parser().feed(f))
    good = len(f) == 43 and len(frames) == 1 and crc16_ccitt(b"123456789") == 0x29B1
    print(f"{'PASS' if good else 'FAIL'}  ENV test frame is 43 bytes and decodes: {summary(*frames[0][::2])}")
    ok &= good
    print("ALL PASSED" if ok else "SOMETHING FAILED")
    return 0 if ok else 1


# ------------------------------------------------------------------ main
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("mode", choices=["fake-comm", "fake-fc", "selftest"])
    ap.add_argument("--port", help="USB-UART adapter, e.g. COM5 or /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--parity", default="E", choices="NEO", help="E = even (the IntroSat link)")
    ap.add_argument("--ack-delay", type=int, default=150, metavar="MS", help="fake-comm: wait this long before 0x06")
    ap.add_argument("--no-ack", action="store_true", help="fake-comm: never answer (FC must keep going, count missed ACKs)")
    ap.add_argument("--drop", type=float, default=0, metavar="PCT", help="fake-comm: leave this %% of frames without ACK")
    ap.add_argument("--save", metavar="FILE", help="fake-comm: append every received byte (replay with the monitor's --file)")
    ap.add_argument("--flood", action="store_true", help="fake-fc: send as fast as the ACKs allow")
    ap.add_argument("--quiet", action="store_true", help="only the 5 s summaries")
    a = ap.parse_args()
    if a.mode == "selftest":
        return selftest()
    if not a.port:
        ap.error("give --port")
    try:
        import serial
    except ImportError:
        sys.exit("pyserial missing: pip install pyserial")
    port = serial.Serial(a.port, a.baud, parity=a.parity, timeout=0.01)
    if a.mode == "fake-comm":
        side = FakeComm(port, a.ack_delay, a.no_ack, a.drop, a.save, not a.quiet)
        what = f"pretending to be the comm board (ACK after {a.ack_delay} ms)"
    else:
        side = FakeFc(port, a.flood, not a.quiet)
        what = "pretending to be the FC (" + ("flood" if a.flood else "flight timing") + ")"
    print(f"{a.port} @ {a.baud} 8{a.parity}1: {what}. Ctrl+C to stop.")
    next_report = time.monotonic() + 5
    try:
        while True:
            now = time.monotonic()
            side.step(now)
            if now >= next_report:
                print("--", side.report())
                next_report += 5
    except KeyboardInterrupt:
        print("\nfinal:", side.report())
    return 0


if __name__ == "__main__":
    sys.exit(main())
