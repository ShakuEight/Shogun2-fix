"""Byte-check hook prologues against the on-disk x86 empire.retail.dll.

Reads the PE section table (no third-party modules), maps each RVA to its file
offset and compares the bytes with the expected stolen bytes. Absolute
addresses inside the stolen bytes are compared as stored on disk (image base
0x10000000, before any rebase).

    python tools/check_prologues.py [path to empire.retail.dll]

The hook list below is the v17 table (src/hooks_v17.cpp, docs/prologues_v17.txt):
the thirteen v16 entry hooks plus the mid-function patch at RVA 0x3DF6C0
(FUN_103df3f0, the instruction after CALL FUN_10394e80).
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
    # artillery-targeting-fix, was aimfix (docs/re/prologues_aimfix.txt): the fix hook, then the two probe-only hooks
    ("FUN_1047fd90", 0x47FD90, "53 56 8B F1 80 7E 7C 00"),
    ("FUN_103a6850", 0x3A6850, "83 EC 0C 56 8B F1"),
    ("FUN_1045a8a0", 0x45A8A0, "F3 0F 10 44 24 10"),
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
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
