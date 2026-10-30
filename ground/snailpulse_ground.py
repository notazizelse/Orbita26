#!/usr/bin/env python3
"""SnailPulse ground station - decodes SnailPulse frames received by the LoRa-UART module.

    pip install pyserial            (matplotlib only needed for --plot)
    python snailpulse_ground.py --port /dev/tty.usbserial-0001 --baud 9600
    python snailpulse_ground.py --file capture.bin           # replay a raw capture
    python snailpulse_ground.py --plot logs/                  # plot CSVs after the flight

The --baud/--parity must match the "UartRate"/"Parity" set in RF Setting for the
ground LoRa-UART module (not the 115200 8E1 used between FCM and the comm board).
Every raw byte is also saved to logs/raw_<time>.bin so nothing is lost if decoding fails.
"""
import argparse, csv, os, struct, sys, time

SYNC = b"\xAA\x55"
MAX_PAYLOAD = 51

# type: (name, struct format, field names)   - must match firmware/SnailPulse_Flight/protocol.h
PACKETS = {
    0x01: ("BASE", "<IIhi3h3h3hh",
           ["t_ms", "press_pa", "baro_t_c100", "alt_dm", "ax_mg", "ay_mg", "az_mg",
            "gx_dps10", "gy_dps10", "gz_dps10", "mx_mg", "my_mg", "mz_mg", "fc_t_x8"]),
    # SNAIL: snail 1 = TCRT5000 (PB0), snail 2 = MAX30100 (max_* fields, appended in fw 1.3: 49 bytes)
    0x10: ("SNAIL", "<IHHHBBHH20sB2sHHBBHH",
           ["t_ms", "bpm_x10", "last_ibi_ms", "beats_total", "quality", "flags", "amp_counts",
            "dc_counts", "wave", "wave_n", "rsv",
            "max_bpm_x10", "max_beats_total", "max_quality", "max_flags", "max_amp", "max_ir"]),
    0x11: ("ENV", "<IhHIhhBBHB3shHHIh",         # fw 1.2+: 36 bytes (DHT-11, MQ-7, second BMP280 appended; unused since 1.3)
           ["t_ms", "box_t_c100", "box_rh_c100", "box_press_pa", "bmp_t_c100", "heater_t_c100",
            "heater_duty", "heater_mode", "max30100_ir", "sensors_ok", "rsv",
            "dht_t_c100", "dht_rh_c100", "mq7_mv", "box2_press_pa", "bmp2_t_c100"]),
    0x7F: ("STATUS", "<IHHHHHHHHBB2s",         # fc_ok was a reserved 0 byte in older builds
           ["t_ms", "fw_version", "boot_count", "frames_sent", "acks_missed", "frames_dropped",
            "i2c_errors", "sd_errors", "loop_max_ms", "reset_cause", "fc_ok", "rsv"]),
}
# Shorter ENV packets from older builds (24 bytes: fw 1.0-1.1; 30 bytes: an early 1.2 test build).
# The fields they lack decode as "missing".
ENV_OLD_FMTS = ["<IhHIhhBBHB3shHH", "<IhHIhhBBHB3s"]
ENV_MISSING = {"dht_t_c100": -32768, "dht_rh_c100": 0xFFFF, "mq7_mv": 0xFFFF, "box2_press_pa": 0, "bmp2_t_c100": -32768}
# 39-byte SNAIL packets from fw 1.0-1.2 have no snail 2 (MAX30100): flag 8 = "sensor not answering".
SNAIL_OLD_FMTS = ["<IHHHBBHH20sB2s"]
SNAIL_MISSING = {"max_bpm_x10": 0, "max_beats_total": 0, "max_quality": 0, "max_flags": 8 | 16, "max_amp": 0, "max_ir": 0}
for t, (n, fmt, names) in PACKETS.items():
    assert struct.calcsize(fmt) <= MAX_PAYLOAD, n
assert struct.calcsize(PACKETS[0x11][1]) == 36 and [struct.calcsize(f) for f in ENV_OLD_FMTS] == [30, 24]
assert struct.calcsize(PACKETS[0x10][1]) == 49 and struct.calcsize(SNAIL_OLD_FMTS[0]) == 39


def crc16_ccitt(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


class Parser:
    """Byte-stream parser: finds AA 55, checks length and CRC, yields (type, seq, payload)."""
    def __init__(self):
        self.buf = bytearray()
        self.crc_errors = 0

    def feed(self, data: bytes):
        self.buf += data
        while True:
            i = self.buf.find(SYNC)
            if i < 0:
                del self.buf[:-1]
                return
            del self.buf[:i]
            if len(self.buf) < 5:
                return
            ln = self.buf[4]
            if ln > MAX_PAYLOAD:
                del self.buf[:2]
                continue
            if len(self.buf) < 7 + ln:
                return
            frame = bytes(self.buf[:7 + ln])
            if crc16_ccitt(frame[2:5 + ln]) == struct.unpack_from("<H", frame, 5 + ln)[0]:
                del self.buf[:7 + ln]
                yield frame[2], frame[3], frame[5:5 + ln]
            else:
                self.crc_errors += 1
                del self.buf[:2]


def decode(ptype, payload):
    if ptype not in PACKETS:
        return None, None
    name, fmt, names = PACKETS[ptype]
    size = struct.calcsize(fmt)
    d = None
    if len(payload) >= size:
        d = dict(zip(names, struct.unpack_from(fmt, payload)))
    else:
        olds, missing = {"ENV": (ENV_OLD_FMTS, ENV_MISSING), "SNAIL": (SNAIL_OLD_FMTS, SNAIL_MISSING)}.get(name, ([], {}))
        for old in olds:
            if len(payload) >= struct.calcsize(old):
                d = {**missing, **dict(zip(names, struct.unpack_from(old, payload)))}
                break
    if d is None:
        return name, None
    d.pop("rsv", None)
    if name == "SNAIL":
        d["wave"] = list(d["wave"][:d["wave_n"]])
        d["bpm"] = d["bpm_x10"] / 10
        d["max_bpm"] = d["max_bpm_x10"] / 10
    return name, d


def scaled(name, d):
    """Human-readable one-liner."""
    inv = -32768
    f = lambda v, k=100: "n/a" if v == inv else f"{v / k:.2f}"
    if name == "SNAIL":
        flags = [s for b, s in ((1, "beat"), (2, "SATURATED"), (4, "NO-SIGNAL")) if d["flags"] & b]
        mflags = [s for b, s in ((1, "beat"), (2, "SATURATED"), (4, "NO-SIGNAL"), (8, "NOT-ANSWERING"), (16, "NO-CONTACT"))
                  if d["max_flags"] & b]
        return (f"snail1/TCRT5000 {d['bpm']:5.1f} bpm IBI {d['last_ibi_ms']} ms q {d['quality']:3d}% "
                f"beats {d['beats_total']} amp {d['amp_counts']} dc {d['dc_counts']} {' '.join(flags)} | "
                f"snail2/MAX30100 {d['max_bpm']:5.1f} bpm q {d['max_quality']:3d}% beats {d['max_beats_total']} "
                f"amp {d['max_amp']} IR {d['max_ir']} {' '.join(mflags)}")
    if name == "ENV":
        rh = lambda v: "n/a" if v == 0xFFFF else f"{v / 100:.0f}"
        mbar = lambda v: "n/a" if v == 0 else f"{v / 100:.1f}"
        line = ((f"SHT21 {f(d['box_t_c100'])} C {rh(d['box_rh_c100'])} %RH | " if d["box_t_c100"] != inv else "") +
                f"BMP280 {mbar(d['box_press_pa'])} mbar {f(d['bmp_t_c100'])} C | "
                + (f"BMP#2 {mbar(d['box2_press_pa'])} mbar {f(d['bmp2_t_c100'])} C | " if d["box2_press_pa"] else "") +
                f"DHT {f(d['dht_t_c100'], 100)} C {rh(d['dht_rh_c100'])} %RH | "
                f"MQ-7 {'n/a' if d['mq7_mv'] == 0xFFFF else str(d['mq7_mv']) + ' mV'} | ")
        if d["heater_t_c100"] != inv or d["heater_duty"]:          # only rev B has a heater
            line += f"heater {f(d['heater_t_c100'])} C duty {d['heater_duty'] * 100 // 255}% mode {d['heater_mode']} | "
        return line + f"ok=0x{d['sensors_ok']:02X}"
    if name == "BASE":
        return (f"{d['press_pa'] / 100:.2f} mbar  alt {d['alt_dm'] / 10:.0f} m  baroT {f(d['baro_t_c100'])} C  "
                f"acc {d['ax_mg']},{d['ay_mg']},{d['az_mg']} mg  fcT {f(d['fc_t_x8'], 8)} C")
    if name == "STATUS":
        return (f"fw {d['fw_version'] >> 8}.{d['fw_version'] & 0xFF} boot {d['boot_count']} sent {d['frames_sent']} "
                f"noACK {d['acks_missed']} drop {d['frames_dropped']} i2cErr {d['i2c_errors']} "
                f"sdErr {d['sd_errors']} loopMax {d['loop_max_ms']} ms reset {d['reset_cause']} fcOk 0x{d['fc_ok']:X}")
    return str(d)


class CsvSink:
    def __init__(self, folder):
        os.makedirs(folder, exist_ok=True)
        self.folder, self.files = folder, {}

    def write(self, name, d, seq):
        if name not in self.files:
            path = os.path.join(self.folder, f"{name.lower()}.csv")
            cols = ["rx_time", "seq"] + [k for k in d if k != "wave"]
            new = not os.path.exists(path)
            if not new:   # columns changed (e.g. ENV grew in fw 1.2): start a new file, don't mix layouts
                with open(path, newline="") as r:
                    if next(csv.reader(r), None) != cols:
                        path = path[:-4] + time.strftime("_%Y%m%d_%H%M%S") + ".csv"
                        new = True
            fh = open(path, "a", newline="")
            w = csv.writer(fh)
            if new:
                w.writerow(cols)
            self.files[name] = (fh, w, cols)
            if name == "SNAIL":
                wpath = os.path.join(self.folder, "waveform.csv")
                wnew = not os.path.exists(wpath)
                self.wfh = open(wpath, "a", newline="")
                self.ww = csv.writer(self.wfh)
                if wnew:
                    self.ww.writerow(["t_ms", "hb_adc_8bit"])
        fh, w, cols = self.files[name]
        row = {"rx_time": f"{time.time():.3f}", "seq": seq, **d}
        w.writerow([row.get(c) for c in cols])
        fh.flush()
        if name == "SNAIL":   # 10 Hz samples, last one at t_ms
            n = len(d["wave"])
            for i, v in enumerate(d["wave"]):
                self.ww.writerow([d["t_ms"] - (n - 1 - i) * 100, v])
            self.wfh.flush()


def run(stream_read, out_dir, raw_path=None, quiet=False):
    p, sink = Parser(), CsvSink(out_dir)
    raw = open(raw_path, "ab") if raw_path else None
    counts = {}
    last_seq = None
    lost = 0
    while True:
        data = stream_read()
        if data is None:
            break
        if not data:
            continue
        if raw:
            raw.write(data); raw.flush()
        for ptype, seq, payload in p.feed(data):
            if last_seq is not None:
                lost += (seq - last_seq - 1) & 0xFF
            last_seq = seq
            name, d = decode(ptype, payload)
            if d is None:
                print(f"? unknown/short packet type 0x{ptype:02X} len {len(payload)}")
                continue
            counts[name] = counts.get(name, 0) + 1
            sink.write(name, d, seq)
            if not quiet:
                print(f"[{d['t_ms'] / 1000:8.1f}s #{seq:3d}] {name:6s} {scaled(name, d)}")
    print(f"frames: {counts}  crc errors: {p.crc_errors}  lost (seq gaps): {lost}")
    return counts, p.crc_errors


def plot(folder):
    import matplotlib.pyplot as plt
    def load(name):   # newest of name.csv / name_<date>.csv (a new one starts when the columns change)
        base = name[:-4]
        cands = [os.path.join(folder, f) for f in os.listdir(folder)
                 if f.endswith(".csv") and (f == name or f.startswith(base + "_2"))]
        if not cands:
            return []
        path = max(cands, key=os.path.getmtime)
        with open(path) as fh:
            return list(csv.DictReader(fh))
    sn, wv, env = load("snail.csv"), load("waveform.csv"), load("env.csv")
    fig, ax = plt.subplots(5, 1, figsize=(11, 12), sharex=True)
    if wv:
        ax[0].plot([int(r["t_ms"]) / 60000 for r in wv], [int(r["hb_adc_8bit"]) for r in wv], lw=0.6)
    ax[0].set_ylabel("snail 1 TCRT5000\nHB_ADC (8-bit)")
    if sn:
        t = [int(r["t_ms"]) / 60000 for r in sn]
        ax[1].plot(t, [int(r["bpm_x10"]) / 10 for r in sn], ".-", ms=3, label="snail 1 (TCRT5000)")
        if "max_bpm_x10" in sn[0]:
            ax[1].plot(t, [int(r["max_bpm_x10"]) / 10 for r in sn], ".-", ms=3, label="snail 2 (MAX30100)")
        ax[1].legend()
    ax[1].set_ylabel("heart rate, bpm")
    if env:
        t = [int(r["t_ms"]) / 60000 for r in env]
        def temp(col):   # -32768 = sensor missing (rev C has no heater NTC) -> leave a gap
            return [float("nan") if int(r[col]) == -32768 else int(r[col]) / 100 for r in env]
        if any(int(r["box_t_c100"]) != -32768 for r in env):       # SHT21 removed in fw 1.5
            ax[2].plot(t, temp("box_t_c100"), label="box (SHT21)")
        ax[2].plot(t, temp("bmp_t_c100"), label="box (BMP280)")
        if "dht_t_c100" in env[0] and any(int(r["dht_t_c100"]) != -32768 for r in env):
            ax[2].plot(t, temp("dht_t_c100"), label="box (DHT-11)")
        if any(int(r["heater_t_c100"]) != -32768 for r in env):
            ax[2].plot(t, temp("heater_t_c100"), label="heater (NTC1)")
        ax[2].legend()
        if "mq7_mv" in env[0]:   # 0xFFFF = MQ-7 off or older firmware -> gap
            ax[3].plot(t, [float("nan") if int(r["mq7_mv"]) == 0xFFFF else int(r["mq7_mv"]) for r in env])
        def mbar(col):   # 0 = sensor missing -> gap
            return [float("nan") if int(r.get(col) or 0) == 0 else int(r[col]) / 100 for r in env]
        ax[4].plot(t, mbar("box_press_pa"), label="BMP280 (0x76)")
        if "box2_press_pa" in env[0] and any(int(r["box2_press_pa"] or 0) for r in env):   # fw 1.2 logs only
            ax[4].plot(t, mbar("box2_press_pa"), label="second BMP280 (0x77, fw 1.2)")
        ax[4].legend()
    ax[2].set_ylabel("C")
    ax[3].set_ylabel("MQ-7 AO, mV")
    ax[4].set_ylabel("box pressure, mbar"); ax[4].set_xlabel("minutes since FC boot")
    fig.suptitle("SnailPulse flight data")
    fig.tight_layout()
    out = os.path.join(folder, "snailpulse_flight.png")
    fig.savefig(out, dpi=150)
    print("saved", out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port"); ap.add_argument("--baud", type=int, default=9600)
    ap.add_argument("--parity", default="N", choices="NEO")
    ap.add_argument("--file"); ap.add_argument("--out", default="logs")
    ap.add_argument("--plot", metavar="FOLDER"); ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()
    if a.plot:
        return plot(a.plot)
    if a.file:
        with open(a.file, "rb") as fh:
            data = fh.read()
        chunks = iter([data[i:i + 13] for i in range(0, len(data), 13)])
        run(lambda: next(chunks, None), a.out, quiet=a.quiet)
        return
    if not a.port:
        ap.error("give --port, --file or --plot")
    import serial
    ser = serial.Serial(a.port, a.baud, parity=a.parity, timeout=0.2)
    raw = os.path.join(a.out, time.strftime("raw_%Y%m%d_%H%M%S.bin"))
    os.makedirs(a.out, exist_ok=True)
    print(f"listening on {a.port} @ {a.baud} 8{a.parity}1 - Ctrl+C to stop; raw -> {raw}")
    try:
        run(lambda: ser.read(256), a.out, raw)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    sys.exit(main())
