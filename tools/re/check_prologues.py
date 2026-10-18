"""Byte-check hook prologues against the on-disk x86 empire.retail.dll.

Reads the PE section table (no third-party modules), maps each RVA to its file
offset and compares the bytes with the expected stolen bytes. Absolute
addresses inside the stolen bytes are compared as stored on disk (image base
0x10000000, before any rebase).

    python tools/check_prologues.py [path to empire.retail.dll]

The list below holds every patch site of every fix compiled into the DLL (pathfix,
artillery-targeting-fix, fire-by-rank-fix, siege-responsiveness-fix, wall-manning-fix): entry and mid-function
hooks, functions a fix calls directly, and [imm32 patch] instructions whose immediate a fix
rewrites. After the byte check the script fails when two sites share a byte, because
the fixes must never patch the same code.
"""
import struct
import sys

DLL = r"E:\SteamLibrary\steamapps\common\Total War SHOGUN 2\empire.retail.dll"

HOOKS = [
    ("FUN_101f6bf0", 0x1F6BF0, "81 EC AC 00 00 00"),
    ("FUN_102557e0", 0x2557E0, "56 8B 74 24 08"),
    ("FUN_1010bb50", 0x10BB50, "83 EC 60 C7 44 24 5C 00 00 00 00"),
    ("FUN_1035ae20", 0x35AE20, "81 EC 50 01 00 00"),
    ("FUN_1037a850", 0x37A850, "F3 0F 10 25 C8 C9 5A 11"),
    ("FUN_1020caa0", 0x20CAA0, "83 EC 34 56 68 CE 7C 5C 11"),
    ("FUN_1020cc30", 0x20CC30, "83 EC 34 56 68 E3 7C 5C 11"),
    ("FUN_10128570", 0x128570, "83 EC 60 56 57"),
    ("FUN_101a9750", 0x1A9750, "55 8B EC 83 E4 F8"),
    ("FUN_1039bb40", 0x39BB40, "81 EC 8C 01 00 00"),
    ("FUN_101bc880", 0x1BC880, "81 EC C8 00 00 00"),
    ("FUN_103dbbf0", 0x3DBBF0, "53 56 57 8B F9"),
    ("FUN_103dafb0", 0x3DAFB0, "56 8B F1 8B 46 28"),
    ("FUN_103df3f0+0x2D0", 0x3DF6C0, "80 BB 71 04 00 00 01"),
    ("FUN_10367600", 0x367600, "55 8B EC 83 E4 F0"),   # v24 integrator entry (docs/prologues_v24.txt)
    ("FUN_103df3f0+0xE1", 0x3DF4D1, "80 7C 24 0F 01"),  # v34 replan request radius (in the dev and public DLL)
    ("FUN_103670a0", 0x3670A0, "83 EC 10 56 57"),       # v38 cursor advance (in the dev and public DLL)
    # artillery-targeting-fix, was aimfix (docs/re/prologues_aimfix.txt): the fix hook, then the two probe-only hooks
    ("FUN_1047fd90", 0x47FD90, "53 56 8B F1 80 7E 7C 00"),
    ("FUN_103a6850", 0x3A6850, "83 EC 0C 56 8B F1"),
    ("FUN_1045a8a0", 0x45A8A0, "F3 0F 10 44 24 10"),
    # fire-by-rank-fix (docs/re/prologues_fbrfix*.txt): the four fix hooks, the ready test it calls,
    # then the four probe-only hooks
    ("FUN_1017b980", 0x17B980, "53 56 57 8B F9"),
    ("FUN_10188290", 0x188290, "83 EC 50 8B C1"),
    ("FUN_1017b9d0", 0x17B9D0, "53 56 57 8B F1"),
    ("FUN_101883f0", 0x1883F0, "83 EC 50 56 8B 74 24 58"),
    ("FUN_103cbe40", 0x3CBE40, "80 B9 84 03 00 00 01"),
    ("FUN_11374cf0", 0x1374CF0, "56 8B F1 57 56"),
    ("FUN_11374cf0+0x1B", 0x1374D0B, "89 7E 04 8B CF"),
    ("FUN_103db8f0", 0x3DB8F0, "53 56 57 8B F9"),
    ("FUN_103db740", 0x3DB740, "53 56 57 8B F9"),
    # siege-responsiveness-fix (docs/re/prologues_siegefix.txt): the query hook (the fix), the five
    # probe-only hooks, then the two [imm32 patch] compares (whole CMP + JAE checked, only the
    # immediate at +6 is written)
    ("FUN_10385fd0", 0x385FD0, "83 EC 40 53 55"),
    ("FUN_103aac30", 0x3AAC30, "81 EC 9C 00 00 00"),
    ("FUN_103aac30+0x1E8", 0x3AAE18, "8B BE 78 01 00 00"),
    ("FUN_1039a500", 0x39A500, "83 EC 40 53 55"),
    ("FUN_103aac30+0x28B", 0x3AAEBB, "88 47 21 C6 47 20 01"),
    ("FUN_103aac30+0x2D3", 0x3AAF03, "8B 86 8C 01 00 00"),
    ("FUN_1035fb40", 0x35FB40, "8B 44 24 04 56"),
    ("FUN_103aac30+0x274", 0x3AAEA4, "81 BE 70 04 00 00 F4 01 00 00 73 53"),   # [imm32 patch] node budget 500
    ("FUN_103aac30+0x91", 0x3AACC1, "81 BE 68 03 00 00 C8 00 00 00 0F 83"),    # [imm32 patch] incremental cap 200
    # wall-manning-fix (docs/re/prologues_wallfix*.txt): the probe-only hooks, then the fix hook
    ("FUN_10254e80", 0x254E80, "56 8B 74 24 08"),
    ("FUN_103db410", 0x3DB410, "56 8B F1 8B 46 04"),
    ("FUN_103e1250", 0x3E1250, "83 EC 18 57 8B F9"),
    ("FUN_101a53f0", 0x1A53F0, "83 EC 14 57 8B F9"),
    ("FUN_101a5500", 0x1A5500, "83 EC 18 53 55"),
    ("FUN_101a5600", 0x1A5600, "83 EC 14 53 56"),
    ("FUN_101a56e0", 0x1A56E0, "83 EC 18 53 56"),
    ("FUN_103be5d0", 0x3BE5D0, "53 56 8B 74 24 0C"),
    ("FUN_103e1780", 0x3E1780, "83 EC 08 56 57"),
    ("FUN_10386d90", 0x386D90, "83 EC 20 53 8B 5C 24 30"),
    ("FUN_1016af90", 0x16AF90, "83 EC 18 57 8B F9"),   # the fix: staging test of the wall task
]


def sections(data):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("not a PE file")
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    base = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    out = []
    off = pe + 24 + optsz
    for i in range(nsec):
        name = data[off:off + 8].rstrip(b"\0").decode("ascii", "replace")
        vsize, va, rsize, raw = struct.unpack_from("<IIII", data, off + 8)
        out.append((name, va, max(vsize, rsize), raw))
        off += 40
    return base, out


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else DLL
    data = open(path, "rb").read()
    base, secs = sections(data)
    print("file %s, %d bytes, image base 0x%08X" % (path, len(data), base))
    bad = 0
    for name, rva, hexbytes in HOOKS:
        want = bytes.fromhex(hexbytes)
        sec = [s for s in secs if s[1] <= rva < s[1] + s[2]]
        if not sec:
            print("%-13s RVA 0x%06X  no section" % (name, rva))
            bad += 1
            continue
        s = sec[0]
        fo = rva - s[1] + s[3]
        got = data[fo:fo + len(want)]
        ok = got == want
        bad += 0 if ok else 1
        print("%-13s RVA 0x%06X  %-6s file 0x%07X  %s%s" % (
            name, rva, s[0], fo, "MATCH" if ok else "DIFFER",
            "" if ok else "  got " + got.hex(" ").upper()))
    print("all %d MATCH" % len(HOOKS) if not bad else "%d DIFFER" % bad)
    # every fix compiles into one DLL: no two sites may share a byte
    spans = sorted((rva, rva + len(bytes.fromhex(h)), name) for name, rva, h in HOOKS)
    overlaps = 0
    for (a0, a1, an), (b0, b1, bn) in zip(spans, spans[1:]):
        if b0 < a1:
            overlaps += 1
            print("OVERLAP %s RVA 0x%06X..0x%06X and %s RVA 0x%06X" % (an, a0, a1, bn, b0))
    print("no overlapping sites" if not overlaps else "%d OVERLAPPING SITES" % overlaps)
    return 1 if bad or overlaps else 0


if __name__ == "__main__":
    sys.exit(main())
