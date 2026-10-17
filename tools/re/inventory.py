#!/usr/bin/env python3
"""
inventory.py - PE inventory of one or more game install folders.

For every .dll/.exe found it reports bitness, size, link timestamp, PDB path
(CodeView record), RTTI type-name count, hits on the cross-version anchor
strings from CLAUDE.md, and which commonly proxied system DLLs it imports.
Output is a Markdown report. Standard library only (no pefile).

Usage:
    python tools/inventory.py <folder> [<folder> ...] --recursive -o docs/inventory.md

The output file is never overwritten blind: on collision the new report is
written to <name>_tmp, the old file is moved to <name>.bak, then the tmp file
is moved into place.
"""
from __future__ import print_function

import argparse
import datetime
import os
import re
import struct
import sys

ANCHOR_STRINGS = [
    # No spaces after the commas in the binary (CLAUDE.md quotes it with spaces).
    "expecting at most 4 arguments (location,angle,width,move_fast)",
    "Move Orientation Width",
    "Move Rotation Angle",
    "ENTITY_ACTION_ATTACK_CORRIDOR_FACTOR",
    "EntityActionAttack.cpp",
    "FIND_WAYPOINT",
    "MOVE_TO_WAYPOINT",
    "ARRIVED_AT_WAYPOINT",
    "SoftCollisionObject.cpp",
    "NAVAL_UNIT_RECORD",
    "rocket",
    "torpedo",
    "PATHFINDING_GRID",
]

# Anchors that only the battle module should contain. "rocket"/"torpedo" and
# the UI strings are too generic to count toward the battle-code verdict.
BATTLE_ANCHORS = [
    "expecting at most 4 arguments (location,angle,width,move_fast)",
    "ENTITY_ACTION_ATTACK_CORRIDOR_FACTOR",
    "EntityActionAttack.cpp",
    "FIND_WAYPOINT",
    "MOVE_TO_WAYPOINT",
    "ARRIVED_AT_WAYPOINT",
    "SoftCollisionObject.cpp",
    "NAVAL_UNIT_RECORD",
]

# The angle constant is the source literal 182.04443f (0x43360b60), which is
# one ulp below the correctly rounded 65536/360 (0x43360b61). Search for the
# literal, not the quotient.
FLOAT_ANCHORS = [
    ("182.04443f (deg -> uint16 angle)", struct.pack("<I", 0x43360B60)),
    ("1.1f corridor factor", struct.pack("<f", 1.1)),
]

# System DLLs that are practical to proxy (few exports, loaded early, found
# via the application directory before System32).
PROXYABLE = [
    "dinput8.dll", "dinput.dll", "version.dll", "winmm.dll", "dxgi.dll",
    "d3d9.dll", "d3d11.dll", "d3d10.dll", "dsound.dll", "xinput1_3.dll",
    "xinput1_4.dll", "xinput9_1_0.dll", "winhttp.dll", "wininet.dll",
    "dbghelp.dll", "msimg32.dll", "dwmapi.dll", "iphlpapi.dll",
    "wsock32.dll", "opengl32.dll", "ddraw.dll", "msacm32.dll",
    "avifil32.dll", "userenv.dll", "uxtheme.dll", "cryptbase.dll",
    "profapi.dll", "powrprof.dll", "hid.dll", "setupapi.dll",
]

RTTI_RE = re.compile(br"\.\?A[VU][A-Za-z_][A-Za-z0-9_@?$]{1,200}@@")
SOURCE_PATH_RE = re.compile(br"[A-Za-z]:\\[\x20-\x7e]{4,200}?\.(?:cpp|h|hpp|inl|c)\b")


class PEError(Exception):
    pass


class PE(object):
    """Minimal PE32 / PE32+ reader: sections, imports, debug directory."""

    def __init__(self, data):
        self.data = data
        if len(data) < 0x40 or data[:2] != b"MZ":
            raise PEError("no MZ header")
        e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
        if e_lfanew + 24 > len(data) or data[e_lfanew:e_lfanew + 4] != b"PE\0\0":
            raise PEError("no PE signature")
        (self.machine, nsec, self.timestamp, _symptr, _nsym, opt_size,
         self.characteristics) = struct.unpack_from("<HHIIIHH", data, e_lfanew + 4)
        opt = e_lfanew + 24
        magic = struct.unpack_from("<H", data, opt)[0]
        if magic == 0x10B:
            self.bits = 32
            self.image_base = struct.unpack_from("<I", data, opt + 28)[0]
            dd_off = opt + 96
            n_dd = struct.unpack_from("<I", data, opt + 92)[0]
        elif magic == 0x20B:
            self.bits = 64
            self.image_base = struct.unpack_from("<Q", data, opt + 24)[0]
            dd_off = opt + 112
            n_dd = struct.unpack_from("<I", data, opt + 108)[0]
        else:
            raise PEError("unknown optional header magic 0x%x" % magic)
        self.size_of_image = struct.unpack_from("<I", data, opt + 56)[0]
        self.dirs = []
        for i in range(min(n_dd, 16)):
            self.dirs.append(struct.unpack_from("<II", data, dd_off + i * 8))
        self.sections = []
        sec_off = opt + opt_size
        for i in range(nsec):
            o = sec_off + i * 40
            name = data[o:o + 8].rstrip(b"\0").decode("ascii", "replace")
            vsize, va, rsize, rptr = struct.unpack_from("<IIII", data, o + 8)
            chars = struct.unpack_from("<I", data, o + 36)[0]
            self.sections.append((name, va, vsize, rptr, rsize, chars))

    def rva_to_off(self, rva):
        for _name, va, vsize, rptr, rsize, _c in self.sections:
            span = max(vsize, rsize)
            if va <= rva < va + span:
                off = rva - va + rptr
                if off < len(self.data):
                    return off
        return None

    def cstr(self, rva, limit=512):
        off = self.rva_to_off(rva)
        if off is None:
            return None
        end = self.data.find(b"\0", off, off + limit)
        if end < 0:
            end = off + limit
        return self.data[off:end].decode("ascii", "replace")

    def _dir(self, idx):
        if idx < len(self.dirs):
            return self.dirs[idx]
        return (0, 0)

    def imports(self):
        """Return (static_import_dll_names, delay_import_dll_names)."""
        static, delay = [], []
        rva, _size = self._dir(1)
        if rva:
            off = self.rva_to_off(rva)
            while off is not None and off + 20 <= len(self.data):
                ilt, _ts, _fwd, name_rva, iat = struct.unpack_from("<IIIII", self.data, off)
                if not (ilt or name_rva or iat):
                    break
                name = self.cstr(name_rva)
                if name:
                    static.append(name)
                off += 20
        rva, _size = self._dir(13)
        if rva:
            off = self.rva_to_off(rva)
            while off is not None and off + 32 <= len(self.data):
                attrs, name_rva = struct.unpack_from("<II", self.data, off)
                if not name_rva:
                    break
                if not (attrs & 1) and self.bits == 32:
                    name_rva = (name_rva - self.image_base) & 0xFFFFFFFF
                name = self.cstr(name_rva)
                if name:
                    delay.append(name)
                off += 32
        return static, delay

    def export_name(self):
        rva, _size = self._dir(0)
        if not rva:
            return None, 0
        off = self.rva_to_off(rva)
        if off is None or off + 40 > len(self.data):
            return None, 0
        name_rva = struct.unpack_from("<I", self.data, off + 12)[0]
        n_names = struct.unpack_from("<I", self.data, off + 24)[0]
        return self.cstr(name_rva), n_names

    def pdb_path(self):
        rva, size = self._dir(6)
        if not rva:
            return None
        off = self.rva_to_off(rva)
        if off is None:
            return None
        for i in range(size // 28):
            o = off + i * 28
            if o + 28 > len(self.data):
                break
            dtype = struct.unpack_from("<I", self.data, o + 12)[0]
            dsize, _draw_rva, dptr = struct.unpack_from("<III", self.data, o + 16)
            if dtype == 2 and dptr and dptr + 24 < len(self.data):
                if self.data[dptr:dptr + 4] == b"RSDS":
                    end = self.data.find(b"\0", dptr + 24, dptr + max(dsize, 24) + 1)
                    if end < 0:
                        end = dptr + dsize
                    return self.data[dptr + 24:end].decode("utf-8", "replace")
                if self.data[dptr:dptr + 4] == b"NB10":
                    end = self.data.find(b"\0", dptr + 16, dptr + max(dsize, 16) + 1)
                    return self.data[dptr + 16:end].decode("utf-8", "replace")
        return None


def count_all(data, needle):
    n, pos = 0, 0
    while True:
        pos = data.find(needle, pos)
        if pos < 0:
            return n
        n += 1
        pos += 1


def find_files(folder, recursive):
    out = []
    if recursive:
        for root, _dirs, files in os.walk(folder):
            for f in files:
                if f.lower().endswith((".dll", ".exe")):
                    out.append(os.path.join(root, f))
    else:
        for f in os.listdir(folder):
            p = os.path.join(folder, f)
            if os.path.isfile(p) and f.lower().endswith((".dll", ".exe")):
                out.append(p)
    out.sort(key=lambda p: p.lower())
    return out


def inspect(path):
    info = {"path": path, "size": os.path.getsize(path), "error": None}
    try:
        with open(path, "rb") as fh:
            data = fh.read()
        pe = PE(data)
    except (PEError, IOError, OSError, struct.error) as exc:
        info["error"] = str(exc)
        return info
    info["bits"] = pe.bits
    info["machine"] = pe.machine
    info["image_base"] = pe.image_base
    info["size_of_image"] = pe.size_of_image
    try:
        info["timestamp"] = datetime.datetime.utcfromtimestamp(pe.timestamp).strftime("%Y-%m-%d %H:%M UTC")
    except (ValueError, OverflowError, OSError):
        info["timestamp"] = "invalid (0x%08x)" % pe.timestamp
    info["sections"] = pe.sections
    info["pdb"] = pe.pdb_path()
    info["export_name"], info["n_exports"] = pe.export_name()
    try:
        static, delay = pe.imports()
    except struct.error:
        static, delay = [], []
    info["imports"] = static
    info["delay_imports"] = delay
    low_static = set(n.lower() for n in static)
    low_delay = set(n.lower() for n in delay)
    info["proxyable"] = [n for n in PROXYABLE if n in low_static]
    info["proxyable_delay"] = [n for n in PROXYABLE if n in low_delay and n not in low_static]

    hits = {}
    for s in ANCHOR_STRINGS:
        a = count_all(data, s.encode("ascii"))
        w = count_all(data, s.encode("utf-16-le"))
        hits[s] = (a, w)
    info["anchors"] = hits
    info["battle_score"] = sum(1 for s in BATTLE_ANCHORS if sum(hits[s]) > 0)
    info["floats"] = [(label, count_all(data, pat)) for label, pat in FLOAT_ANCHORS]

    rtti = set(m.group(0) for m in RTTI_RE.finditer(data))
    info["rtti_count"] = len(rtti)
    info["rtti_sample"] = sorted(n.decode("ascii", "replace") for n in rtti)[:0]
    interesting = [n.decode("ascii", "replace") for n in rtti
                   if re.search(br"(?i)path|formation|waypoint|softcollision|moveorder|unit_?order", n)]
    info["rtti_interesting"] = sorted(interesting)[:40]
    info["rtti_interesting_total"] = len(interesting)
    src = set(m.group(0) for m in SOURCE_PATH_RE.finditer(data))
    info["source_path_count"] = len(src)
    info["source_path_sample"] = sorted(p.decode("ascii", "replace") for p in src)[:8]
    return info


def human_size(n):
    return "%.1f MB" % (n / 1048576.0) if n >= 1048576 else "%.1f KB" % (n / 1024.0)


def render(folders, results, recursive):
    L = []
    L.append("# PE inventory")
    L.append("")
    L.append("Generated %s by tools/inventory.py (recursive=%s)." % (
        datetime.datetime.now().strftime("%Y-%m-%d %H:%M"), recursive))
    L.append("")
    for folder in folders:
        infos = results[folder]
        ok = [i for i in infos if not i["error"]]
        L.append("## %s" % folder)
        L.append("")
        L.append("%d PE files scanned, %d unreadable." % (len(ok), len(infos) - len(ok)))
        L.append("")
        ranked = sorted(ok, key=lambda i: (-i["battle_score"], -i["size"]))
        if ranked and ranked[0]["battle_score"] > 0:
            top = ranked[0]
            L.append("**Battle-code module: `%s`** (%d of %d battle anchor strings present, %d-bit, %s)." % (
                os.path.relpath(top["path"], folder), top["battle_score"], len(BATTLE_ANCHORS),
                top["bits"], human_size(top["size"])))
            others = [i for i in ranked[1:] if i["battle_score"] > 0]
            if others:
                L.append("")
                L.append("Other modules with battle anchor hits: " + ", ".join(
                    "`%s` (%d)" % (os.path.relpath(i["path"], folder), i["battle_score"]) for i in others))
        else:
            L.append("**No module in this folder contains any battle anchor string.**")
        L.append("")
        L.append("### Summary")
        L.append("")
        L.append("| File | Bits | Size | Linked | Battle anchors | RTTI names | PDB path | Proxy-able imports |")
        L.append("|------|------|------|--------|----------------|------------|----------|--------------------|")
        for i in infos:
            rel = os.path.relpath(i["path"], folder)
            if i["error"]:
                L.append("| %s | - | %s | - | - | - | unreadable: %s | - |" % (rel, human_size(i["size"]), i["error"]))
                continue
            prox = ", ".join(i["proxyable"])
            if i["proxyable_delay"]:
                prox += (" ; " if prox else "") + "delay-load: " + ", ".join(i["proxyable_delay"])
            L.append("| %s | %d | %s | %s | %d/%d | %d | %s | %s |" % (
                rel, i["bits"], human_size(i["size"]), i["timestamp"], i["battle_score"],
                len(BATTLE_ANCHORS), i["rtti_count"], i["pdb"] or "none", prox or "none"))
        L.append("")
        for i in ranked:
            if i["battle_score"] == 0:
                continue
            rel = os.path.relpath(i["path"], folder)
            L.append("### Detail: %s" % rel)
            L.append("")
            L.append("- Bitness: %d-bit (machine 0x%04x), image base 0x%x, SizeOfImage %s" % (
                i["bits"], i["machine"], i["image_base"], human_size(i["size_of_image"])))
            L.append("- File size: %d bytes (%s), linked %s" % (i["size"], human_size(i["size"]), i["timestamp"]))
            L.append("- PDB path: %s" % (i["pdb"] or "none (no CodeView debug record)"))
            L.append("- Export table: name %s, %d named exports" % (i["export_name"] or "none", i["n_exports"]))
            L.append("- RTTI type names (.?AV / .?AU): %d distinct; %d match path/formation/waypoint/collision" % (
                i["rtti_count"], i["rtti_interesting_total"]))
            L.append("- Embedded source paths (*.cpp/*.h): %d distinct" % i["source_path_count"])
            for p in i["source_path_sample"]:
                L.append("    - `%s`" % p)
            L.append("- Sections: " + ", ".join("%s (%s)" % (s[0], human_size(s[2])) for s in i["sections"]))
            L.append("")
            L.append("Anchor strings (ASCII hits / UTF-16 hits):")
            L.append("")
            L.append("| Anchor | ASCII | UTF-16 |")
            L.append("|--------|-------|--------|")
            for s in ANCHOR_STRINGS:
                a, w = i["anchors"][s]
                L.append("| %s | %d | %d |" % (s, a, w))
            L.append("")
            L.append("Float constants (raw 4-byte pattern count, unaligned; noisy for common values):")
            L.append("")
            for label, n in i["floats"]:
                L.append("- %s: %d" % (label, n))
            L.append("")
            L.append("Proxy-able DLLs imported statically: %s" % (", ".join(i["proxyable"]) or "none"))
            L.append("")
            L.append("Proxy-able DLLs delay-loaded only: %s" % (", ".join(i["proxyable_delay"]) or "none"))
            L.append("")
            L.append("All static imports (%d): %s" % (len(i["imports"]), ", ".join(sorted(i["imports"], key=str.lower))))
            L.append("")
            if i["delay_imports"]:
                L.append("All delay imports (%d): %s" % (
                    len(i["delay_imports"]), ", ".join(sorted(i["delay_imports"], key=str.lower))))
                L.append("")
            if i["rtti_interesting"]:
                L.append("Sample of pathfinding-related RTTI names (first %d of %d):" % (
                    len(i["rtti_interesting"]), i["rtti_interesting_total"]))
                L.append("")
                for n in i["rtti_interesting"]:
                    L.append("- `%s`" % n)
                L.append("")
        # Who imports what, across the whole folder: needed to pick a proxy
        # DLL that is actually loaded by the process.
        L.append("### Proxy-able imports across all modules in this folder")
        L.append("")
        table = {}
        for i in ok:
            for n in i["proxyable"]:
                table.setdefault(n, []).append(os.path.basename(i["path"]))
            for n in i["proxyable_delay"]:
                table.setdefault(n, []).append(os.path.basename(i["path"]) + " (delay)")
        if table:
            L.append("| System DLL | Imported by |")
            L.append("|------------|-------------|")
            for n in sorted(table):
                L.append("| %s | %s |" % (n, ", ".join(sorted(set(table[n]), key=str.lower))))
        else:
            L.append("None.")
        L.append("")
    return "\n".join(L) + "\n"


def safe_write(target, text):
    """Write text to target without ever overwriting an existing file blind."""
    target = os.path.abspath(target)
    parent = os.path.dirname(target)
    if not os.path.isdir(parent):
        os.makedirs(parent)
    if not os.path.exists(target):
        with open(target, "w") as fh:
            fh.write(text)
        return "created %s" % target
    tmp = target + "_tmp"
    with open(tmp, "w") as fh:
        fh.write(text)
    bak = target + ".bak"
    n = 1
    while os.path.exists(bak):
        n += 1
        bak = "%s.bak%d" % (target, n)
    os.rename(target, bak)
    os.rename(tmp, target)
    return "replaced %s (previous version kept as %s)" % (target, bak)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folders", nargs="+", help="install folders to scan")
    ap.add_argument("--recursive", action="store_true", help="descend into subfolders")
    ap.add_argument("-o", "--output", default=None, help="Markdown report path (default: stdout)")
    args = ap.parse_args()

    results = {}
    for folder in args.folders:
        if not os.path.isdir(folder):
            print("ERROR: not a folder: %s" % folder, file=sys.stderr)
            return 2
        files = find_files(folder, args.recursive)
        print("scanning %d PE files under %s" % (len(files), folder), file=sys.stderr)
        results[folder] = [inspect(p) for p in files]

    text = render(args.folders, results, args.recursive)
    if args.output:
        print(safe_write(args.output, text))
    else:
        sys.stdout.write(text)
    return 0


if __name__ == "__main__":
    sys.exit(main())
