#!/usr/bin/env python3
"""Builds an offline Arduino bundle for a Mac that can't reach arduino.cc (e.g. in Russia).

Run it on any computer that CAN reach downloads.arduino.cc and github.com (Windows, Linux or Mac):

    python3 scripts/build_mac_arduino_bundle.py OUT_FOLDER [--arch arm64|x86_64]

It writes OUT_FOLDER/arduino-offline-mac-<arch>.tgz with, laid out exactly as Arduino IDE 2.3.10 /
arduino-cli 1.5.1 would install them:
  Library/Arduino15/   package + library indexes, Arduino's helper tools (ctags, discovery, monitor),
                       STM32 board package 2.12.0 + its compiler/OpenOCD, Arduino AVR Boards
                       (the IDE installs that on first start otherwise)
  Documents/Arduino/libraries/   STM32SD 1.5.0, FatFs 4.0.4, IntroStratLib 0.1.2, Arduino_BuiltIn + its libraries
On the Mac:  bash scripts/mac_setup.sh --arduino15 arduino-offline-mac-arm64.tgz   (or: tar -xzf <file> -C ~)

Nothing is unpacked on this computer: archive members are copied into the bundle with their Unix
permissions and symlinks intact (Windows can't create those), so the Mac gets working executables.
"""
import argparse, hashlib, io, json, os, re, stat, sys, tarfile, time, urllib.request, zipfile

PKG_INDEX = "https://downloads.arduino.cc/packages/package_index.json"
LIB_INDEX = "https://downloads.arduino.cc/libraries/library_index.json"
STM_INDEX = "https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json"
STM_VERSION = "2.12.0"
BUILTIN = {"ctags": "5.8-arduino11", "dfu-discovery": "0.1.2", "mdns-discovery": "1.1.0",
           "serial-discovery": "1.5.2", "serial-monitor": "0.15.0"}        # what arduino-cli 1.5.1 installs
GIT_LIBS = [("STM32SD", "https://github.com/stm32duino/STM32SD/archive/refs/tags/1.5.0.zip"),
            ("FatFs", "https://github.com/stm32duino/FatFs/archive/refs/tags/4.0.4.zip"),
            ("IntroStratLib-Public", "https://github.com/Obu-IntroSat/IntroStratLib-Public/archive/refs/tags/0.1.2.zip")]


def log(*a):
    print(time.strftime("%H:%M:%S"), *a, flush=True)


def fetch(url, cache, name=None, checksum=None, size=None):
    os.makedirs(cache, exist_ok=True)
    path = os.path.join(cache, name or url.rstrip("/").split("/")[-1].split("?")[0])
    if os.path.exists(path) and (not size or os.path.getsize(path) == int(size)) and (not checksum or sha(path) == checksum):
        return path
    for attempt in range(4):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "arduino-cli/1.5.1"})
            with urllib.request.urlopen(req, timeout=60) as r, open(path + ".part", "wb") as f:
                while True:
                    b = r.read(1 << 20)
                    if not b:
                        break
                    f.write(b)
            os.replace(path + ".part", path)
            break
        except Exception as e:
            log(f"  retry {attempt + 1} {url}: {e}")
            time.sleep(3)
    else:
        sys.exit(f"download failed: {url}")
    if checksum and sha(path) != checksum:
        sys.exit(f"checksum mismatch: {url}")
    log(f"  got {os.path.basename(path)} ({os.path.getsize(path) / 1e6:.1f} MB)")
    return path


def sha(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return "SHA-256:" + h.hexdigest()


def flavour(tool, arch):
    """Pick the download arduino-cli would pick on this Mac (arm64 falls back to Intel builds)."""
    pats = ([r"^(arm64|aarch64)-apple-darwin"] if arch == "arm64" else []) + [r"^x86_64-apple-darwin", r"^i[3-6]86-apple-darwin"]
    for p in pats:
        for s in tool["systems"]:
            if re.match(p, s["host"]):
                return s
    sys.exit(f"no macOS build of {tool['name']} {tool['version']}")


class Bundle:
    def __init__(self, path):
        self.tar = tarfile.open(path, "w:gz", compresslevel=6, format=tarfile.PAX_FORMAT)
        self.dirs = set()

    def _dir(self, name):
        parts = name.strip("/").split("/")
        for i in range(1, len(parts) + 1):
            d = "/".join(parts[:i])
            if d not in self.dirs:
                self.dirs.add(d)
                ti = tarfile.TarInfo(d); ti.type = tarfile.DIRTYPE; ti.mode = 0o755; ti.mtime = time.time()
                self.tar.addfile(ti)

    def add_bytes(self, name, data, mode=0o644):
        self._dir(os.path.dirname(name))
        ti = tarfile.TarInfo(name); ti.size = len(data); ti.mode = mode; ti.mtime = time.time()
        self.tar.addfile(ti, io.BytesIO(data))

    def add_archive(self, archive, dest):
        """Copy an archive's contents under dest/, dropping a single top folder like arduino-cli does."""
        if archive.endswith(".zip"):
            z = zipfile.ZipFile(archive)
            infos = [i for i in z.infolist() if i.filename.strip("/")]
            names = [i.filename for i in infos]
        else:
            t = tarfile.open(archive, "r:*")
            infos = [m for m in t.getmembers() if m.name.strip("./")]
            names = [m.name for m in infos]
        norm = [re.sub(r"^\./", "", n).strip("/") for n in names]
        tops = {n.split("/")[0] for n in norm}
        strip = len(tops) == 1 and any("/" in n for n in norm)

        def target(n):
            n = re.sub(r"^\./", "", n).strip("/")
            if strip:
                n = n.split("/", 1)[1] if "/" in n else ""
            return f"{dest}/{n}" if n else dest

        self._dir(dest)
        for info, n in zip(infos, names):
            tn = target(n)
            if tn == dest:
                continue
            self._dir(os.path.dirname(tn))
            if archive.endswith(".zip"):
                mode = (info.external_attr >> 16) & 0xFFFF
                ti = tarfile.TarInfo(tn); ti.mtime = time.mktime(info.date_time + (0, 0, -1))
                if info.is_dir():
                    if tn not in self.dirs:
                        self.dirs.add(tn); ti.type = tarfile.DIRTYPE; ti.mode = 0o755; self.tar.addfile(ti)
                elif stat.S_ISLNK(mode):
                    ti.type = tarfile.SYMTYPE; ti.linkname = z.read(info).decode(); ti.mode = 0o777; self.tar.addfile(ti)
                else:
                    data = z.read(info)
                    ti.size = len(data); ti.mode = (mode & 0o7777) or 0o644
                    self.tar.addfile(ti, io.BytesIO(data))
            else:
                m = info
                ti = tarfile.TarInfo(tn); ti.mtime = m.mtime; ti.mode = m.mode; ti.type = m.type
                if m.isdir():
                    if tn in self.dirs:
                        continue
                    self.dirs.add(tn); self.tar.addfile(ti)
                elif m.issym():
                    ti.linkname = m.linkname; self.tar.addfile(ti)
                elif m.islnk():
                    ti.linkname = target(m.linkname); self.tar.addfile(ti)
                elif m.isfile():
                    ti.size = m.size; self.tar.addfile(ti, t.extractfile(m))

    def close(self):
        self.tar.close()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out")
    ap.add_argument("--arch", default="arm64", choices=["arm64", "x86_64"])
    a = ap.parse_args()
    cache = os.path.join(a.out, "_cache")
    os.makedirs(a.out, exist_ok=True)
    A15, LIBS = "Library/Arduino15", "Documents/Arduino/libraries"
    out = os.path.join(a.out, f"arduino-offline-mac-{a.arch}.tgz")
    b = Bundle(out + ".part")

    log("indexes")
    files = {}
    for url in (PKG_INDEX, PKG_INDEX + ".sig", LIB_INDEX, LIB_INDEX + ".sig", STM_INDEX):
        files[url] = fetch(url, cache)
        with open(files[url], "rb") as f:
            b.add_bytes(f"{A15}/{os.path.basename(files[url])}", f.read())
    pkg = json.load(open(files[PKG_INDEX], encoding="utf-8"))
    stm = json.load(open(files[STM_INDEX], encoding="utf-8"))
    libidx = json.load(open(files[LIB_INDEX], encoding="utf-8"))

    packages = {p["name"]: p for p in pkg["packages"] + stm["packages"]}

    def tool(packager, name, version):
        for t in packages[packager]["tools"]:
            if t["name"] == name and t["version"] == version:
                return t
        sys.exit(f"tool {packager}:{name}@{version} not in index")

    log("Arduino helper tools")
    for name, ver in BUILTIN.items():
        s = flavour(tool("builtin", name, ver), a.arch)
        b.add_archive(fetch(s["url"], cache, s["archiveFileName"], s["checksum"], s["size"]),
                      f"{A15}/packages/builtin/tools/{name}/{ver}")

    def platform(packager, arch, version=None):
        rel = [p for p in packages[packager]["platforms"] if p["architecture"] == arch]
        if version:
            rel = [p for p in rel if p["version"] == version]
        else:   # newest
            rel.sort(key=lambda p: [int(x) if x.isdigit() else 0 for x in re.split(r"[.-]", p["version"])])
            rel = rel[-1:]
        p = rel[0]
        log(f"platform {packager}:{arch}@{p['version']}")
        b.add_archive(fetch(p["url"], cache, p["archiveFileName"], p["checksum"], p["size"]),
                      f"{A15}/packages/{packager}/hardware/{arch}/{p['version']}")
        deps = []
        for d in p.get("toolsDependencies", []):
            t = tool(d["packager"], d["name"], d["version"])
            deps.append(t)
            s = flavour(t, a.arch)
            b.add_archive(fetch(s["url"], cache, s["archiveFileName"], s["checksum"], s["size"]),
                          f"{A15}/packages/{d['packager']}/tools/{d['name']}/{d['version']}")
        meta = {k: v for k, v in packages[packager].items() if k not in ("platforms", "tools")}
        installed = {"packages": [{**meta, "platforms": [p], "tools": deps}]}
        b.add_bytes(f"{A15}/packages/{packager}/hardware/{arch}/{p['version']}/installed.json",
                    json.dumps(installed, indent=2).encode())

    platform("STMicroelectronics", "stm32", STM_VERSION)
    platform("arduino", "avr")      # the IDE installs this on its first start if it's missing

    log("libraries")
    for folder, url in GIT_LIBS:
        b.add_archive(fetch(url, cache, f"{folder}.zip"), f"{LIBS}/{folder}")
    newest = {}
    for lib in libidx["libraries"]:
        key = lib["name"]
        v = [int(x) if x.isdigit() else 0 for x in re.split(r"[.-]", lib["version"])]
        if key not in newest or v > newest[key][0]:
            newest[key] = (v, lib)
    todo, done = ["Arduino_BuiltIn"], set()
    while todo:
        name = todo.pop()
        if name in done or name not in newest:
            continue
        done.add(name)
        lib = newest[name][1]
        b.add_archive(fetch(lib["url"], cache, lib["archiveFileName"], lib["checksum"], lib["size"]),
                      f"{LIBS}/{re.sub(r'[^A-Za-z0-9_.-]', '_', name)}")
        todo += [d["name"] for d in lib.get("dependencies", [])]
    log(f"  index libraries: {', '.join(sorted(done))}")

    b.close()
    os.replace(out + ".part", out)
    log(f"bundle: {out} ({os.path.getsize(out) / 1e6:.0f} MB)")


if __name__ == "__main__":
    main()
