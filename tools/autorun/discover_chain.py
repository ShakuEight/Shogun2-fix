"""Find a static pointer path to a known live object in the running game (read-only).

Used once to derive the battle-clock chain the autorun proxy reads (tools/autorun/docs/runbook.md):
the pathfix dev log names the live tick object ("v36 game clock: object 0x........"); this script
searches forward from root globals of empire.retail.dll for a path [[root]+o1]+o2 ... == target.
Nothing is written to the game; it only uses OpenProcess(PROCESS_VM_READ) / ReadProcessMemory.

    python tools\\autorun\\discover_chain.py --target 0x6B8C0FD8 --root-rva 0x17c755c [--depth 4]
    python tools\\autorun\\discover_chain.py --target-from-log      (reads <game>\\pathfix_log.txt)
    python tools\\autorun\\discover_chain.py --verify "0x17c755c:0x84,0x10,0x8"   (print the chain values)

Roots: --root-rva may be repeated; --scan-data searches every pointer-sized slot of the module's
writable sections as a root (slower, finds paths from any global).
"""
import argparse
import ctypes
import ctypes.wintypes as wt
import os
import re
import struct
import sys

GAME_DIR = r"E:\SteamLibrary\steamapps\common\Total War SHOGUN 2"
PROCESS_VM_READ = 0x0010
PROCESS_QUERY_INFORMATION = 0x0400
k32 = ctypes.WinDLL("kernel32", use_last_error=True)
psapi = ctypes.WinDLL("psapi", use_last_error=True)


def find_pid(name="shogun2.exe"):
    arr = (wt.DWORD * 4096)()
    needed = wt.DWORD()
    psapi.EnumProcesses(arr, ctypes.sizeof(arr), ctypes.byref(needed))
    for pid in arr[: needed.value // 4]:
        h = k32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
        if not h:
            continue
        buf = ctypes.create_unicode_buffer(260)
        psapi.GetModuleBaseNameW(h, None, buf, 260)
        k32.CloseHandle(h)
        if buf.value.lower() == name:
            return pid
    return None


class Proc:
    def __init__(self, pid):
        self.h = k32.OpenProcess(PROCESS_VM_READ | PROCESS_QUERY_INFORMATION, False, pid)
        if not self.h:
            raise OSError("OpenProcess failed: %d" % ctypes.get_last_error())

    def read(self, addr, size):
        buf = ctypes.create_string_buffer(size)
        got = ctypes.c_size_t()
        if not k32.ReadProcessMemory(self.h, ctypes.c_void_p(addr), buf, size, ctypes.byref(got)):
            return None
        return buf.raw[: got.value]

    def u32(self, addr):
        b = self.read(addr, 4)
        return struct.unpack("<I", b)[0] if b and len(b) == 4 else None

    def module(self, name="empire.retail.dll"):
        mods = (ctypes.c_void_p * 1024)()
        needed = wt.DWORD()
        # LIST_MODULES_32BIT = 1 (the game is a 32-bit process; this script runs as 64-bit Python)
        psapi.EnumProcessModulesEx(self.h, mods, ctypes.sizeof(mods), ctypes.byref(needed), 1)
        for m in mods[: needed.value // ctypes.sizeof(ctypes.c_void_p)]:
            buf = ctypes.create_unicode_buffer(260)
            psapi.GetModuleBaseNameW(self.h, ctypes.c_void_p(m), buf, 260)
            if buf.value.lower() == name:
                return m
        return None


def writable_sections(proc, base):
    pe = proc.u32(base + 0x3C)
    nsec = struct.unpack("<H", proc.read(base + pe + 6, 2))[0]
    optsize = struct.unpack("<H", proc.read(base + pe + 20, 2))[0]
    so = base + pe + 24 + optsize
    out = []
    for i in range(nsec):
        s = proc.read(so + 40 * i, 40)
        name = s[:8].rstrip(b"\0").decode(errors="replace")
        vsize, va = struct.unpack("<II", s[8:16])
        chars = struct.unpack("<I", s[36:40])[0]
        if chars & 0x80000000:  # IMAGE_SCN_MEM_WRITE
            out.append((name, base + va, vsize))
    return out


def plausible(p):
    return 0x10000 <= p < 0xFFFF0000 and p % 4 == 0


def search(proc, roots, target, depth, span):
    """Breadth-first: each frontier entry is (address of a pointer slot, path). Reads the pointer,
    then scans span bytes of the pointed object for the next slot."""
    found = []
    frontier = [(slot, [label]) for (label, slot) in roots]
    visited = set()
    for level in range(depth):
        nxt = []
        for slot, path in frontier:
            p = proc.u32(slot)
            if p is None or not plausible(p) or p in visited:
                continue
            visited.add(p)
            if p == target:
                found.append(path)
                continue
            blob = proc.read(p, span)
            if not blob:
                continue
            for off in range(0, len(blob) - 3, 4):
                v = struct.unpack_from("<I", blob, off)[0]
                if v == target:
                    found.append(path + ["0x%x" % off])
                elif plausible(v) and level + 1 < depth:
                    nxt.append((p + off, path + ["0x%x" % off]))
        frontier = nxt
        print("  level %d: %d slots to follow, %d paths found" % (level + 1, len(frontier), len(found)))
        if len(frontier) > 400000:
            print("  frontier too large, stopping at this depth")
            break
    return found


def target_from_log():
    text = open(os.path.join(GAME_DIR, "pathfix_log.txt"), "r", errors="replace").read()
    m = re.findall(r"game clock: object 0x([0-9A-Fa-f]{8}) from controller", text)
    return int(m[-1], 16) if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", type=lambda s: int(s, 16))
    ap.add_argument("--target-from-log", action="store_true")
    ap.add_argument("--root-rva", action="append", default=[], type=lambda s: int(s, 16))
    ap.add_argument("--scan-data", action="store_true")
    ap.add_argument("--depth", type=int, default=4)
    ap.add_argument("--span", type=lambda s: int(s, 0), default=0x400)
    ap.add_argument("--verify")
    args = ap.parse_args()

    pid = find_pid()
    if not pid:
        sys.exit("shogun2.exe is not running")
    proc = Proc(pid)
    base = proc.module()
    print("pid %d, empire.retail.dll at 0x%08X (rebase delta 0x%08X)" % (pid, base, base - 0x10000000))

    if args.verify:
        rva_text, offs_text = args.verify.split(":")
        addr = base + int(rva_text, 16)
        p = proc.u32(addr)
        print("  [base+%s] = 0x%08X" % (rva_text, p or 0))
        for o in [int(x, 16) for x in offs_text.split(",") if x]:
            q = proc.u32(p + o) if p else None
            print("  [0x%08X+0x%x] = 0x%08X" % (p or 0, o, q or 0))
            p = q
        return

    target = args.target
    if args.target_from_log:
        target = target_from_log()
    if not target:
        sys.exit("no target (give --target or run with pathfix capture so the log names the clock object)")
    print("target 0x%08X: tick [t+0x50] = %s, over flag [t+0x58] = %s" % (
        target, proc.u32(target + 0x50), (proc.read(target + 0x58, 1) or b"?")[0]))

    roots = [("rva_0x%x" % r, base + r) for r in args.root_rva]
    if args.scan_data:
        for name, va, size in writable_sections(proc, base):
            blob = proc.read(va, size) or b""
            for off in range(0, len(blob) - 3, 4):
                v = struct.unpack_from("<I", blob, off)[0]
                if plausible(v):
                    roots.append(("rva_0x%x" % (va + off - base), va + off))
            print("  section %s: %d bytes scanned" % (name, len(blob)))
    print("%d roots, depth %d, span 0x%x" % (len(roots), args.depth, args.span))
    paths = search(proc, roots, target, args.depth, args.span)
    paths.sort(key=len)
    for p in paths[:40]:
        print("PATH  %s" % " -> ".join(p))
    if not paths:
        print("no path found")


main()
