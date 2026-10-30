#!/usr/bin/env python3
"""SnailPulse ground monitor: every sensor's value and OK / WARN / FAIL state in your browser.

    python3 ground/snailpulse_monitor.py                                  # finds the USB serial port itself
    python3 ground/snailpulse_monitor.py --port /dev/cu.usbserial-1410    # FC USB-C on the bench (115200)
    python3 ground/snailpulse_monitor.py --port /dev/cu.usbserial-0001 --baud 9600   # ground LoRa module
    python3 ground/snailpulse_monitor.py --demo                           # no hardware: simulated satellite
    python3 ground/snailpulse_monitor.py --file logs/raw_20261030_101500.bin          # replay a capture
    python3 ground/snailpulse_monitor.py --list                           # list serial ports

Then open http://localhost:8765 (it opens by itself). Ctrl+C stops it.

Two ways to get data in, same screen:
  * Bench, no radio: flight firmware 1.2 copies every frame to the FC's USB-C (SP_USB_MIRROR 1), 115200.
  * Over the air: the ground E32 LoRa-UART module on a USB-UART adapter, at ITS UART rate (often 9600 8N1).
Text from the FC (SensorTest, HB_Tune, debug prints) shows in the Console panel.

Needs pyserial (not for --demo / --file). Everything received is also saved like snailpulse_ground.py:
logs/raw_<time>.bin (every byte) and one CSV per packet type.
"""
import argparse, json, math, os, random, struct, sys, threading, time, webbrowser
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from snailpulse_ground import PACKETS, SYNC, CsvSink, Parser, crc16_ccitt, decode  # noqa: E402

PERIOD_S = {"SNAIL": 2, "BASE": 2, "ENV": 4, "STATUS": 10}    # config.h PERIOD_*_MS
INV16, NO_RH, NO_MQ7 = -32768, 0xFFFF, 0xFFFF
HISTORY_S = 600                                              # charts show the last 10 minutes


# ----------------------------------------------------------------------------- state
class Monitor:
    def __init__(self, out_dir, source, replay=False):
        self.lock = threading.Lock()
        self.parser = Parser()
        self.sink = CsvSink(out_dir) if out_dir else None
        self.raw = None
        self.source, self.replay = source, replay
        self.error = ""
        self.t0 = time.time()
        self.last_rx = None             # time of the last byte
        self.bytes = 0
        self.latest = {}                # name -> (rx_time, seq, dict)
        self.prev_status = None
        self.counts = {}
        self.lost = 0
        self.last_seq = None
        self.series = {}                # key -> deque[(t, v)]
        self.wave = deque(maxlen=HISTORY_S * 10)
        self.console = deque(maxlen=300)
        self.frames = deque(maxlen=40)
        self._line = bytearray()
        self._line_ok = True

    def open_raw(self, path):
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        self.raw = open(path, "ab")

    def clock(self):
        return (self.last_rx or time.time()) if self.replay else time.time()

    def add(self, key, t, v):
        if v is None or (isinstance(v, float) and math.isnan(v)):
            return
        self.series.setdefault(key, deque(maxlen=2000)).append((t, v))

    # Bytes from the port. Frames go to the parser; clean text lines go to the console.
    def feed(self, data, t=None):
        t = t or time.time()
        with self.lock:
            self.bytes += len(data)
            self.last_rx = t
            if self.raw:
                self.raw.write(data); self.raw.flush()
            for b in data:
                if b == 0x0A:
                    if self._line_ok and self._line.strip():
                        self.console.append((t, self._line.decode("ascii", "replace").rstrip("\r")))
                    self._line.clear(); self._line_ok = True
                elif 32 <= b < 127 or b in (9, 13):
                    if len(self._line) < 400:
                        self._line.append(b)
                else:
                    self._line_ok = False       # binary frame bytes: not a text line
            for ptype, seq, payload in self.parser.feed(data):
                self._frame(ptype, seq, payload, t)

    def _frame(self, ptype, seq, payload, t):
        if self.last_seq is not None:
            self.lost += (seq - self.last_seq - 1) & 0xFF
        self.last_seq = seq
        name, d = decode(ptype, payload)
        if d is None:
            self.frames.appendleft((t, seq, f"0x{ptype:02X}", f"unknown or short packet ({len(payload)} B)"))
            return
        self.counts[name] = self.counts.get(name, 0) + 1
        if name == "STATUS" and "STATUS" in self.latest:
            self.prev_status = self.latest["STATUS"][2]
        self.latest[name] = (t, seq, d)
        self.frames.appendleft((t, seq, name, short(name, d)))
        if self.sink:
            self.sink.write(name, d, seq)
        if name == "SNAIL":
            n = len(d["wave"])
            for i, v in enumerate(d["wave"]):
                self.wave.append((t - (n - 1 - i) * 0.1, v))
            self.add("bpm", t, d["bpm"] if d["bpm_x10"] else None)
            self.add("bpm2", t, d["max_bpm"] if d["max_bpm_x10"] else None)
        elif name == "ENV":
            self.add("t_sht", t, c100(d["box_t_c100"]))
            self.add("t_dht", t, c100(d["dht_t_c100"]))
            self.add("t_bmp1", t, c100(d["bmp_t_c100"]))
            self.add("rh_sht", t, rh(d["box_rh_c100"]))
            self.add("rh_dht", t, rh(d["dht_rh_c100"]))
            self.add("p_bmp1", t, hpa(d["box_press_pa"]))
            self.add("mq7", t, None if d["mq7_mv"] == NO_MQ7 else d["mq7_mv"])
        elif name == "BASE":
            self.add("p_ms5611", t, hpa(d["press_pa"]))
            self.add("t_ms5611", t, c100(d["baro_t_c100"]))
            self.add("t_lm75", t, None if d["fc_t_x8"] == INV16 else d["fc_t_x8"] / 8)

    def snapshot(self):
        with self.lock:
            now = self.clock()
            groups = evaluate(self, now)
            cut = now - HISTORY_S
            ser = {k: [(round(t - now, 1), v) for t, v in s if t >= cut] for k, s in self.series.items()}
            wave = [(round(t - now, 2), v) for t, v in self.wave if t >= now - 60]
            tally = {}
            for g in groups:
                for r in g["rows"]:
                    tally[r["state"]] = tally.get(r["state"], 0) + 1
            return {
                "source": self.source, "error": self.error, "replay": self.replay,
                "age": None if self.last_rx is None else round(now - self.last_rx, 1),
                "bytes": self.bytes, "counts": self.counts, "crc_errors": self.parser.crc_errors, "lost": self.lost,
                "groups": groups, "tally": tally, "series": ser, "wave": wave,
                "console": [(time.strftime("%H:%M:%S", time.localtime(t)), s) for t, s in list(self.console)[-150:]],
                "frames": [(time.strftime("%H:%M:%S", time.localtime(t)), seq, n, s) for t, seq, n, s in self.frames],
            }


# ----------------------------------------------------------------------------- helpers
def c100(v):
    return None if v == INV16 else v / 100


def rh(v):
    return None if v == NO_RH else v / 100


def hpa(pa):
    return None if not pa else pa / 100


def fmt(v, unit="", nd=1):
    return "—" if v is None else f"{v:.{nd}f}{unit}"


def hms(ms):
    s = int(ms // 1000)
    return f"{s // 3600}:{s // 60 % 60:02d}:{s % 60:02d}"


def short(name, d):
    if name == "SNAIL":
        return f"snail 1 {d['bpm']:.1f} bpm q{d['quality']}% · snail 2 {d['max_bpm']:.1f} bpm q{d['max_quality']}%"
    if name == "ENV":
        return f"box {fmt(c100(d['bmp_t_c100']), ' °C')}, {fmt(rh(d['dht_rh_c100']), ' %RH', 0)}, ok=0x{d['sensors_ok']:02X}"
    if name == "BASE":
        return f"{fmt(hpa(d['press_pa']), ' hPa')}, |a| {math.sqrt(d['ax_mg']**2 + d['ay_mg']**2 + d['az_mg']**2):.0f} mg"
    if name == "STATUS":
        return f"fw {d['fw_version'] >> 8}.{d['fw_version'] & 0xFF}, up {hms(d['t_ms'])}"
    return ""


def row(name, state, value="", detail=""):
    return {"name": name, "state": state, "value": value, "detail": detail}


def in_range(v, lo, hi):
    return v is not None and lo <= v <= hi


# ----------------------------------------------------------------------------- checks
def evaluate(m, now):
    """Turns the latest packets into rows with a state: ok, warn, fail, wait (no data yet),
    stale (data stopped) or na (not fitted). Limits are for a bench test at room conditions,
    with a little room for the stratosphere where it matters."""
    L = m.latest

    def fresh(name):
        if name not in L:
            return None
        t, _, d = L[name]
        return d if now - t <= 3 * PERIOD_S[name] + 3 else False

    G = []

    # --- link
    rows = []
    if m.last_rx is None:
        rows.append(row("Data in", "wait", "nothing yet", m.error or "check the port, baud rate and that the FC is powered"))
    else:
        age = now - m.last_rx
        rows.append(row("Data in", "ok" if age < 5 else ("warn" if age < 30 else "fail"),
                        f"last byte {age:.0f} s ago", m.error))
    total = sum(m.counts.values())
    crc = m.parser.crc_errors
    rows.append(row("Frames OK / CRC errors", "wait" if total == 0 else ("ok" if crc <= max(1, total * 0.02) else "warn"),
                    f"{total} / {crc}", "CRC errors = damaged frames (normal now and then over radio)"))
    rows.append(row("Lost frames (sequence gaps)", "wait" if total == 0 else ("ok" if m.lost <= max(1, total * 0.05) else "warn"),
                    str(m.lost), "over LoRa a few % is normal; on USB it should stay 0"))
    for n in ("SNAIL", "BASE", "ENV", "STATUS"):
        if n not in L:
            rows.append(row(f"{n} packets", "wait", "none yet", f"expected every {PERIOD_S[n]} s"))
        else:
            age = now - L[n][0]
            rows.append(row(f"{n} packets", "ok" if fresh(n) else "stale", f"{m.counts.get(n, 0)} received, last {age:.0f} s ago",
                            "" if fresh(n) else f"expected every {PERIOD_S[n]} s"))
    G.append({"title": "Link (ground side)", "rows": rows})

    # --- flight controller status
    st, rows = fresh("STATUS"), []
    if not st:
        rows.append(row("STATUS packet", "stale" if st is False else "wait", "", "sent every 10 s and once at boot"))
    else:
        fw = st["fw_version"]
        rows.append(row("Firmware", "ok" if fw >= 0x0104 else "warn", f"{fw >> 8}.{fw & 0xFF}",
                        "" if fw >= 0x0104 else ("older than 1.3: no snail 2 (MAX30100) heart rate" if fw < 0x0103 else
                        "older than 1.4: built for a 3.3 V DHT-11/MQ-7 (MQ-7 mV reads 1.5× too low)") + "; upload SnailPulse_Flight again"))
        rows.append(row("Uptime / boot count", "ok", f"{hms(st['t_ms'])} / {st['boot_count']}",
                        "boot count comes from the SD card (0 = no SD)"))
        rows.append(row("Last reset", "warn" if st["reset_cause"] == 1 else "ok",
                        "WATCHDOG" if st["reset_cause"] == 1 else "normal",
                        "the code hung for 4 s (stuck I²C bus or SD card)" if st["reset_cause"] == 1 else ""))
        lm = st["loop_max_ms"]
        rows.append(row("Worst loop time", "ok" if lm < 50 else ("warn" if lm < 1000 else "fail"), f"{lm} ms",
                        "should stay under ~20 ms; the watchdog fires at 4000"))
        prev = m.prev_status
        rising = prev is not None and st["i2c_errors"] > prev["i2c_errors"] and st["t_ms"] > prev["t_ms"]
        rows.append(row("I²C errors", "warn" if rising else "ok", str(st["i2c_errors"]),
                        "rising: a sensor or wire on I²C4/I²C1 is failing" if rising else "a few right after boot are normal"))
        rows.append(row("SD errors", "ok" if st["sd_errors"] == 0 else "warn", str(st["sd_errors"])))
        sent, miss = st["frames_sent"], st["acks_missed"]
        noack = sent > 3 and miss > sent * 0.5
        rows.append(row("Comm board ACKs", "warn" if noack else "ok", f"sent {sent}, no ACK {miss}, dropped {st['frames_dropped']}",
                        "comm board not answering 0x06: expected if no comm board is connected (USB bench test)"
                        if noack else "frames handed to the comm board / not acknowledged / dropped from the queue"))
    G.append({"title": "Flight controller", "rows": rows})

    # --- FC service sensors (BASE + STATUS.fc_ok)
    ba, rows = fresh("BASE"), []
    fc_ok = st["fc_ok"] if st and st.get("fc_ok") else None      # 0 = older firmware, not reported

    def fcbit(bit, state, why):
        if fc_ok is not None and not fc_ok & bit:
            return "fail", "not found at boot (STATUS)"
        return state, why

    if not ba:
        for n in ("MS5611 barometer", "LSM6DS3 accel/gyro", "LIS3MDL magnetometer", "LM75A board temperature"):
            rows.append(row(n, "stale" if ba is False else "wait", "", "BASE packet every 2 s"))
    else:
        p, t = hpa(ba["press_pa"]), c100(ba["baro_t_c100"])
        s, why = ("fail", "no reading (0)") if p is None else (("ok", "") if in_range(p, 5, 1200) and in_range(t, -60, 85) else ("warn", "out of range"))
        s, why = fcbit(8, s, why)
        rows.append(row("MS5611 barometer", s, f"{fmt(p, ' hPa')}, {fmt(t, ' °C')}, alt {ba['alt_dm'] / 10:.0f} m", why))
        a = math.sqrt(ba["ax_mg"] ** 2 + ba["ay_mg"] ** 2 + ba["az_mg"] ** 2)
        s, why = ("fail", "all zero: not answering") if a == 0 else (("ok", "≈1 g at rest") if 850 <= a <= 1150 else ("warn", "not ≈1 g: moving, or check it"))
        s, why = fcbit(1, s, why)
        rows.append(row("LSM6DS3 accel/gyro", s, f"a {ba['ax_mg']}, {ba['ay_mg']}, {ba['az_mg']} mg (|a| {a:.0f}); "
                        f"ω {ba['gx_dps10'] / 10:.1f}, {ba['gy_dps10'] / 10:.1f}, {ba['gz_dps10'] / 10:.1f} °/s", why))
        b = math.sqrt(ba["mx_mg"] ** 2 + ba["my_mg"] ** 2 + ba["mz_mg"] ** 2)
        s, why = ("fail", "all zero: not answering") if b == 0 else (("ok", "Earth's field is 250–650 mG") if 100 <= b <= 1500 else ("warn", "unusual: magnet or steel nearby?"))
        s, why = fcbit(2, s, why)
        rows.append(row("LIS3MDL magnetometer", s, f"{ba['mx_mg']}, {ba['my_mg']}, {ba['mz_mg']} mG (|B| {b:.0f})", why))
        ft = None if ba["fc_t_x8"] == INV16 else ba["fc_t_x8"] / 8
        s, why = ("fail", "no reading") if ft is None else (("ok", "") if in_range(ft, -50, 90) else ("warn", "out of range"))
        s, why = fcbit(4, s, why)
        rows.append(row("LM75A board temperature", s, fmt(ft, " °C"), why))
    G.append({"title": "FC sensors (I²C1)", "rows": rows})

    # --- payload sensors (ENV)
    en, rows = fresh("ENV"), []
    if not en:
        rows.append(row("ENV packet", "stale" if en is False else "wait", "", "sent every 4 s"))
    else:
        ok = en["sensors_ok"]

        def sensor(name, bit, value, valid, why_bad, backup=False):
            if ok & bit and valid:
                rows.append(row(name, "ok", value))
            elif ok & bit:
                rows.append(row(name, "warn", value, "answers, but the value is out of range"))
            else:
                rows.append(row(name, "warn" if backup else "fail", value, why_bad))

        t, h = c100(en["box_t_c100"]), rh(en["box_rh_c100"])
        fwv = L["STATUS"][2]["fw_version"] if "STATUS" in L else 0
        sht_fitted = not (fwv >= 0x0105 and t is None and not ok & 1)     # fw 1.5+: SHT21 switched off in config.h
        if sht_fitted:
            sensor("SHT21 temp/humidity (0x40)", 1, f"{fmt(t, ' °C')}, {fmt(h, ' %RH')}",
                   in_range(t, -40, 60) and in_range(h, 0, 100), "not answering on I²C4 (PD13/PD12)")
        else:
            rows.append(row("SHT21 (GY-21)", "na", "not fitted", "removed 30 Oct; box air comes from the BMP280 and the DHT-11"))
            bt, dh = c100(en["bmp_t_c100"]), rh(en["dht_rh_c100"])
            rows.append(row("Box air (BMP280 T, DHT-11 RH)", "ok" if in_range(bt, -40, 60) and in_range(dh, 0, 100) else "warn",
                            f"{fmt(bt, ' °C')}, {fmt(dh, ' %RH', 0)}", "" if in_range(bt, -40, 60) and in_range(dh, 0, 100)
                            else "one of the two is missing: see their rows"))
        p, t1 = hpa(en["box_press_pa"]), c100(en["bmp_t_c100"])
        sensor("BMP280 (0x76, SDO→GND)", 2, f"{fmt(p, ' hPa')}, {fmt(t1, ' °C')}",
               in_range(p, 5, 1200) and in_range(t1, -40, 60), "not answering at 0x76: check SDO→GND and I²C4")
        td, hd = c100(en["dht_t_c100"]), rh(en["dht_rh_c100"])
        sensor("DHT-11 (PC6)", 32, f"{fmt(td, ' °C', 0)}, {fmt(hd, ' %RH', 0)}",
               in_range(td, 0, 50) and in_range(hd, 5, 95), "no valid reading: check DATA→PC6 and its 5 V supply")
        mv = en["mq7_mv"]
        if mv == NO_MQ7:
            rows.append(row("MQ-7 CO (PB1)", "na", "—", "disabled in config.h or older firmware"))
        else:
            sensor("MQ-7 CO (PB1)", 64, f"{mv} mV", True,
                   "voltage at the module (5 V supply, 10k/20k divider before PB1). 0 V = wire off / no power; ≈4.95 V = off the scale "
                   "or divider missing (check PB1 < 3.3 V!). Needs ~3 min to warm up")
        sensor("MAX30100 (0x57, snail 2)", 4, f"IR {en['max30100_ir']}", True,
               "not answering on I²C4: no heart rate for snail 2 (retried every 5 s)")
        rows.append(row("microSD card", "ok" if ok & 16 else "warn", "logging" if ok & 16 else "no card",
                        "" if ok & 16 else "no SD: radio still works, but the 50 Hz log is lost"))
        if ok & 8 or en["heater_t_c100"] != INV16:
            rows.append(row("Heater NTC (rev B)", "ok" if ok & 8 else "fail", fmt(c100(en["heater_t_c100"]), " °C")))
        else:
            rows.append(row("Heater NTC", "na", "not fitted", "rev C has no heater"))
        need = 0x77 if sht_fitted else 0x76
        rows.append(row("Sensor-OK bits", "ok" if ok & need == need else "warn", f"0x{ok:02X}",
                        f"healthy board = 0x{need:02X} (not fitted: bit 3 heater, bit 7 second BMP280"
                        + ("" if sht_fitted else ", bit 0 SHT21") + ")"))
    G.append({"title": "Payload sensors (I²C4, PC6, PB1)", "rows": rows})

    # --- heartbeats (SNAIL): snail 1 on the TCRT5000, snail 2 on the MAX30100
    sn, rows, rows2 = fresh("SNAIL"), [], []
    if not sn:
        rows.append(row("SNAIL packet", "stale" if sn is False else "wait", "", "sent every 2 s"))
        rows2.append(row("SNAIL packet", "stale" if sn is False else "wait", "", "sent every 2 s"))
    else:
        f = sn["flags"]
        if f & 4:
            s, why = "warn", "no pulse: TCRT5000 off the shell, or turn RV3"
        elif f & 2:
            s, why = "warn", "amplifier clipping: turn RV3 / less light"
        else:
            s, why = "ok", "beat seen in this window" if f & 1 else ""
        rows.append(row("Signal", s, " ".join(n for b, n in ((1, "beat"), (2, "SATURATED"), (4, "NO-SIGNAL")) if f & b) or "clean", why))
        rows.append(row("Heart rate", "ok" if sn["bpm_x10"] else "warn",
                        f"{sn['bpm']:.1f} bpm, last interval {sn['last_ibi_ms']} ms" if sn["bpm_x10"] else "no valid rate yet",
                        f"quality {sn['quality']} %, {sn['beats_total']} beats since boot"))
        v = sn["dc_counts"] * 3.3 / 4095
        rows.append(row("Amplifier level (PB0)", "ok" if 0.3 <= v <= 3.0 else "warn", f"{v:.2f} V (pulse {sn['amp_counts']} counts)",
                        "resting level near 1.65–2.0 V" if 0.3 <= v <= 3.0 else "stuck near a rail: amplifier or wiring"))

        mf = sn["max_flags"]
        if mf & 8:
            s, why = "fail", "MAX30100 not answering on I²C4 (0x57); retried every 5 s"
        elif mf & 16:
            s, why = "warn", "no contact: nothing in front of the MAX30100 (IR below 1000)"
        elif mf & 2:
            s, why = "warn", "too much light: IR at full scale"
        elif mf & 4:
            s, why = "warn", "contact but no pulse yet: hold still, it needs ~5 s after being put on"
        else:
            s, why = "ok", "beat seen in this window" if mf & 1 else ""
        names = ((1, "beat"), (2, "SATURATED"), (4, "NO-SIGNAL"), (8, "NOT-ANSWERING"), (16, "NO-CONTACT"))
        rows2.append(row("Signal", s, " ".join(n for b, n in names if mf & b) or "clean", why))
        rows2.append(row("Heart rate", "ok" if sn["max_bpm_x10"] else "warn",
                         f"{sn['max_bpm']:.1f} bpm" if sn["max_bpm_x10"] else "no valid rate yet",
                         f"quality {sn['max_quality']} %, {sn['max_beats_total']} beats since boot"))
        rows2.append(row("IR level", "ok" if sn["max_ir"] >= 1000 else "warn", f"{sn['max_ir']} (pulse {sn['max_amp']} counts)",
                         "~200 = nothing in front; thousands = on the shell"))
    G.append({"title": "Snail 1 · TCRT5000 → amplifier → PB0", "rows": rows})
    G.append({"title": "Snail 2 · MAX30100 (I²C4, 0x57)", "rows": rows2})

    # --- cross-checks
    rows = []

    def cross(name, a, b, lim, unit, why):
        if a is None or b is None:
            rows.append(row(name, "wait", "", "needs both readings"))
        else:
            dlt = abs(a - b)
            rows.append(row(name, "ok" if dlt <= lim else "warn", f"Δ {dlt:.1f}{unit} (limit {lim}{unit})", why))

    e = en or {}
    b = ba or {}
    cross("BMP280 vs MS5611 pressure", hpa(e.get("box_press_pa")), hpa(b.get("press_pa")), 3, " hPa", "bench only")
    cross("DHT-11 vs BMP280 temperature", c100(e.get("dht_t_c100", INV16)), c100(e.get("bmp_t_c100", INV16)), 3, " °C", "DHT-11 is ±2 °C")
    if c100(e.get("box_t_c100", INV16)) is not None:                      # only while an SHT21 is fitted
        cross("SHT21 vs DHT-11 temperature", c100(e.get("box_t_c100", INV16)), c100(e.get("dht_t_c100", INV16)), 3, " °C", "DHT-11 is ±2 °C")
        cross("SHT21 vs DHT-11 humidity", rh(e.get("box_rh_c100", NO_RH)), rh(e.get("dht_rh_c100", NO_RH)), 15, " %", "DHT-11 is ±5 %RH")
    G.append({"title": "Cross-checks", "rows": rows})
    return G


# ----------------------------------------------------------------------------- demo satellite
def demo_frames(stop):
    """Plausible frames at the real rates, with a DHT-11 that misses now and then."""
    fmt_of = {n: f for _, (n, f, _) in PACKETS.items()}
    seq, t0, beats = 0, time.time(), 0
    tick = {"SNAIL": 0, "BASE": 0, "ENV": 0, "STATUS": 0}
    code = {n: c for c, (n, _, _) in PACKETS.items()}

    def frame(name, *vals):
        nonlocal seq
        body = struct.pack(fmt_of[name], *vals)
        head = bytes([code[name], seq & 0xFF, len(body)])
        seq += 1
        return SYNC + head + body + struct.pack("<H", crc16_ccitt(head + body))

    yield b"SnailPulse demo: simulated flight controller (fw 1.5)\r\n"
    while not stop.is_set():
        el = time.time() - t0
        ms = int(el * 1000)
        out = b""
        if el >= tick["STATUS"]:
            tick["STATUS"] += 10
            out += frame("STATUS", ms, 0x0105, 7, seq, seq // 40, 0, 2, 0, random.randint(3, 9), 0, 0x0F, b"\0\0")
        if el >= tick["SNAIL"]:
            tick["SNAIL"] += 2
            bpm = 28 + 3 * math.sin(el / 60)
            wave = bytes(int(128 + 45 * math.exp(-((((el - 2 + i * 0.1) * bpm / 60) % 1) - 0.2) ** 2 / 0.01)
                             + random.uniform(-3, 3)) for i in range(20))
            beats = int(el * bpm / 60)
            bpm2 = 41 + 4 * math.sin(el / 45)                     # snail 2 (MAX30100): a different snail
            out += frame("SNAIL", ms, int(bpm * 10), int(60000 / bpm), beats, 82, 1, 610, 2030, wave, 20, b"\0\0",
                         int(bpm2 * 10), int(el * bpm2 / 60), 71, 1, 140 + random.randint(-10, 10), 9100 + random.randint(-40, 40))
        if el >= tick["BASE"]:
            tick["BASE"] += 2
            out += frame("BASE", ms, 101180 + random.randint(-8, 8), 2410, 1020, 12, -8, 1003, 3, -2, 1, 210, -95, 380, 196)
        if el >= tick["ENV"]:
            tick["ENV"] += 4
            dht_ok = random.random() > 0.15
            t = 2290 + int(30 * math.sin(el / 90))
            out += frame("ENV", ms, INV16, NO_RH, 101150, t + 15, INV16, 0, 0, 9100, 0x76 if dht_ok else 0x56, b"\0\0\0",
                         t - 60 if dht_ok else INV16, 4700 if dht_ok else NO_RH, 960 + int(30 * math.sin(el / 30)),
                         0, INV16)                               # second BMP280 removed in fw 1.3
        if out:
            yield out
        time.sleep(0.1)


# ----------------------------------------------------------------------------- serial
def list_ports():
    try:
        from serial.tools import list_ports as lp
    except ImportError:
        sys.exit("pyserial is missing: run  python3 -m pip install pyserial  (or use the project's .venv)")
    return [(p.device, p.description) for p in lp.comports()]


def pick_port():
    ports = [p for p in list_ports() if not p[0].startswith("/dev/tty.")] or list_ports()
    usb = [p for p in ports if any(k in (p[0] + p[1]).lower() for k in ("usbserial", "wchusb", "usbmodem", "ch34", "cp210", "ftdi", "uart", "com"))]
    usb = [p for p in usb if "bluetooth" not in (p[0] + p[1]).lower()]
    if len(usb) == 1:
        return usb[0][0]
    lines = "\n".join(f"  {d}   {desc}" for d, desc in ports) or "  (none: is the cable plugged in?)"
    sys.exit(f"Pick a port with --port. Serial ports found:\n{lines}")


def serial_loop(m, port, baud, parity, stop):
    import serial
    ser = None
    while not stop.is_set():
        try:
            if ser is None:
                ser = serial.Serial(port, baud, parity=parity, timeout=0.2)
                with m.lock:
                    m.error = ""
            data = ser.read(512)
            if data:
                m.feed(data)
        except Exception as e:                     # unplugged, busy, ... keep retrying
            with m.lock:
                m.error = f"{port}: {e.__class__.__name__}: {e}"
            try:
                ser and ser.close()
            except Exception:
                pass
            ser = None
            stop.wait(2)


# ----------------------------------------------------------------------------- web page
PAGE = r"""<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>SnailPulse Monitor</title>
<style>
:root{--bg:#f6f7f9;--card:#fff;--ink:#1d2330;--mut:#6b7385;--line:#e3e6ec;--ok:#1f9d55;--warn:#c98a00;--fail:#d64545;--wait:#8a93a6;--na:#b0b6c3;--acc:#3867d6}
@media (prefers-color-scheme:dark){:root{--bg:#12151b;--card:#1b1f27;--ink:#e6e9ef;--mut:#98a1b3;--line:#2b313d;--ok:#3ccf7a;--warn:#f0b429;--fail:#ff6b6b;--wait:#7d869a;--na:#5b6375;--acc:#7aa2ff}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:14px/1.4 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif}
header{position:sticky;top:0;z-index:2;background:var(--card);border-bottom:1px solid var(--line);padding:10px 16px;display:flex;flex-wrap:wrap;gap:10px;align-items:center}
h1{font-size:17px;margin:0 12px 0 0}.pill{padding:3px 10px;border-radius:99px;font-weight:600;font-size:13px;border:1px solid var(--line)}
.ok{color:var(--ok)}.warn{color:var(--warn)}.fail{color:var(--fail)}.wait,.stale{color:var(--wait)}.na{color:var(--na)}
.src{color:var(--mut);font-size:13px}.err{color:var(--fail);font-size:13px;flex-basis:100%}
main{padding:14px 16px;display:grid;gap:14px;grid-template-columns:repeat(auto-fill,minmax(min(100%,460px),1fr))}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:12px 14px;min-width:0}
.card h2{font-size:14px;margin:0 0 8px;color:var(--mut);text-transform:uppercase;letter-spacing:.04em}
table{width:100%;border-collapse:collapse}td{padding:5px 4px;border-top:1px solid var(--line);vertical-align:top}
tr:first-child td{border-top:0}td.st{width:74px;font-weight:700;font-size:12px;white-space:nowrap}
td.nm{width:38%}td.v{font-variant-numeric:tabular-nums}.d{color:var(--mut);font-size:12px}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;background:currentColor;margin-right:5px}
.wide{grid-column:1/-1}canvas{width:100%;height:170px;display:block}
.charts{display:grid;gap:14px;grid-template-columns:repeat(auto-fill,minmax(min(100%,420px),1fr))}
.ct{font-size:13px;color:var(--mut);margin:0 0 4px}.lg{font-size:12px;color:var(--mut)}.lg span{margin-right:10px;white-space:nowrap}
pre{margin:0;max-height:260px;overflow:auto;font:12px/1.35 ui-monospace,Menlo,Consolas,monospace;white-space:pre-wrap;word-break:break-word}
</style></head><body>
<header><h1>🐌 SnailPulse monitor</h1><span id="sum"></span><span class="src" id="src"></span><span class="err" id="err"></span></header>
<main id="groups"></main>
<main><section class="card wide"><h2>Charts (last 10 min; heartbeat: last 60 s)</h2><div class="charts" id="charts"></div></section>
<section class="card"><h2>Console (text from the FC)</h2><pre id="con">nothing yet</pre></section>
<section class="card"><h2>Last frames</h2><pre id="frm">nothing yet</pre></section></main>
<script>
const CH=[["Snail 1 waveform (TCRT5000, 8-bit ADC, 10 Hz)","wave",[["TCRT5000","wave"]],""],["Heart rate","bpm",[["snail 1 · TCRT5000","bpm"],["snail 2 · MAX30100","bpm2"]]," bpm"],
["Temperature","t",[["SHT21","t_sht"],["DHT-11","t_dht"],["BMP280","t_bmp1"],["MS5611","t_ms5611"],["LM75A","t_lm75"]]," °C"],
["Humidity","rh",[["SHT21","rh_sht"],["DHT-11","rh_dht"]]," %"],["Pressure","p",[["BMP280","p_bmp1"],["MS5611","p_ms5611"]]," hPa"],
["MQ-7 output","mq",[["MQ-7","mq7"]]," mV"]];
const COL=["#3867d6","#e8590c","#2f9e44","#ae3ec9","#0c8599","#c2255c"];
const LBL={ok:"OK",warn:"WARN",fail:"FAIL",wait:"WAITING",stale:"STALE",na:"N/A"};
const esc=s=>String(s).replace(/[&<>]/g,c=>({"&":"&amp;","<":"&lt;",">":"&gt;"}[c]));
const cv={};
document.getElementById("charts").innerHTML=CH.map(c=>`<div><p class="ct">${c[0]}</p><canvas id="c_${c[1]}"></canvas><div class="lg">${c[2].map((s,i)=>`<span style="color:${COL[i]}">■ ${s[0]}</span>`).join("")}</div></div>`).join("");
function draw(id,lines,unit){const c=document.getElementById(id),r=devicePixelRatio||1,W=c.clientWidth,H=c.clientHeight;
 if(c.width!==W*r){c.width=W*r;c.height=H*r}const g=c.getContext("2d");g.setTransform(r,0,0,r,0,0);g.clearRect(0,0,W,H);
 const cs=getComputedStyle(document.body),mut=cs.getPropertyValue("--mut"),ln=cs.getPropertyValue("--line");
 let pts=lines.flat();g.font="11px sans-serif";g.fillStyle=mut;
 if(!pts.length){g.fillText("no data yet",8,20);return}
 let x0=Math.min(...pts.map(p=>p[0])),x1=0,y0=Math.min(...pts.map(p=>p[1])),y1=Math.max(...pts.map(p=>p[1]));
 if(x0>x1-5)x0=x1-5;if(y1-y0<1e-6){y0-=1;y1+=1}const pad=(y1-y0)*.1;y0-=pad;y1+=pad;
 const L=46,R=8,T=6,B=18,X=x=>L+(x-x0)/(x1-x0)*(W-L-R),Y=y=>T+(y1-y)/(y1-y0)*(H-T-B);
 g.strokeStyle=ln;g.lineWidth=1;for(let i=0;i<=4;i++){const y=y0+(y1-y0)*i/4,py=Y(y);g.beginPath();g.moveTo(L,py);g.lineTo(W-R,py);g.stroke();
  g.fillText((Math.abs(y1-y0)<10?y.toFixed(1):y.toFixed(0)),2,py+4)}
 g.fillText(Math.round(-x0)+" s ago",L,H-4);g.fillText("now",W-R-22,H-4);
 lines.forEach((p,i)=>{if(!p.length)return;g.strokeStyle=COL[i];g.fillStyle=COL[i];g.lineWidth=1.6;g.beginPath();
  p.forEach((q,j)=>{const px=X(q[0]),py=Y(q[1]);j?g.lineTo(px,py):g.moveTo(px,py)});g.stroke();
  if(p.length<3)p.forEach(q=>{g.beginPath();g.arc(X(q[0]),Y(q[1]),2.5,0,7);g.fill()})});}
async function tick(){let s;try{s=await (await fetch("api/state",{cache:"no-store"})).json()}catch(e){document.getElementById("err").textContent="monitor program stopped";return}
 const t=s.tally,ord=["fail","warn","stale","wait","ok","na"];
 document.getElementById("sum").innerHTML=ord.filter(k=>t[k]).map(k=>`<span class="pill ${k}">${t[k]} ${LBL[k]}</span>`).join(" ");
 document.getElementById("src").textContent=`${s.source} · ${s.bytes} bytes · `+(s.age==null?"no data yet":`last data ${s.age}s ago`)+(s.replay?" · replay":"");
 document.getElementById("err").textContent=s.error||"";
 document.getElementById("groups").innerHTML=s.groups.map(g=>`<section class="card"><h2>${esc(g.title)}</h2><table>${g.rows.map(r=>
  `<tr><td class="st ${r.state}"><span class="dot"></span>${LBL[r.state]}</td><td class="nm">${esc(r.name)}</td><td class="v">${esc(r.value)}${r.detail?`<div class="d">${esc(r.detail)}</div>`:""}</td></tr>`).join("")}</table></section>`).join("");
 for(const c of CH){const L=c[2].map(x=>x[1]==="wave"?s.wave:(s.series[x[1]]||[]));draw("c_"+c[1],L,c[3])}
 document.getElementById("con").textContent=s.console.map(l=>l[0]+"  "+l[1]).join("\n")||"nothing yet (SensorTest / HB_Tune text appears here)";
 const con=document.getElementById("con");con.scrollTop=con.scrollHeight;
 document.getElementById("frm").textContent=s.frames.map(f=>`${f[0]}  #${String(f[1]).padStart(3)}  ${f[2].padEnd(6)} ${f[3]}`).join("\n")||"nothing yet";}
tick();setInterval(tick,1000);
</script></body></html>"""


def serve(m, port, open_browser):
    class H(BaseHTTPRequestHandler):
        def do_GET(self):
            if self.path.startswith("/api/state"):
                body, ctype = json.dumps(m.snapshot()).encode(), "application/json"
            elif self.path in ("/", "/index.html"):
                body, ctype = PAGE.encode(), "text/html; charset=utf-8"
            else:
                self.send_error(404); return
            self.send_response(200)
            self.send_header("Content-Type", ctype)
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, *a):
            pass

    httpd = ThreadingHTTPServer(("127.0.0.1", port), H)
    url = f"http://localhost:{port}"
    print(f"monitor running: {url}   (Ctrl+C to stop)")
    if open_browser:
        threading.Timer(0.8, lambda: webbrowser.open(url)).start()
    return httpd


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port", help="serial port, e.g. /dev/cu.usbserial-1410 or COM5 (default: the only USB one)")
    ap.add_argument("--baud", type=int, default=115200, help="115200 for the FC USB-C; the ground LoRa module's rate otherwise")
    ap.add_argument("--parity", default="N", choices="NEO")
    ap.add_argument("--demo", action="store_true", help="simulated satellite, no hardware")
    ap.add_argument("--file", help="replay a raw capture (logs/raw_*.bin)")
    ap.add_argument("--list", action="store_true", help="list serial ports and exit")
    ap.add_argument("--http", type=int, default=8765, help="web page port (default 8765)")
    ap.add_argument("--out", default="logs", help="folder for raw + CSV logs ('' = don't log)")
    ap.add_argument("--no-browser", action="store_true")
    a = ap.parse_args()

    if a.list:
        for d, desc in list_ports():
            print(f"{d}   {desc}")
        return
    stop = threading.Event()
    if a.demo:
        m = Monitor(None, "demo (simulated satellite)")
        threading.Thread(target=lambda: [m.feed(b) for b in demo_frames(stop)], daemon=True).start()
    elif a.file:
        m = Monitor(None, f"file {os.path.basename(a.file)}", replay=True)
        with open(a.file, "rb") as fh:
            m.feed(fh.read())
    else:
        port = a.port or pick_port()
        m = Monitor(a.out or None, f"{port} @ {a.baud} 8{a.parity}1")
        if a.out:
            m.open_raw(os.path.join(a.out, time.strftime("raw_%Y%m%d_%H%M%S.bin")))
        threading.Thread(target=serial_loop, args=(m, port, a.baud, a.parity, stop), daemon=True).start()
    httpd = serve(m, a.http, not a.no_browser)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        stop.set()
        httpd.server_close()


if __name__ == "__main__":
    main()
