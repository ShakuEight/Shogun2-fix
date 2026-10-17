# Find functions that read (and/or write) a SET of fields through the same
# base register: e.g. the per-tick mover that consumes the steering target
# entity+0x16C written by FUN_103942f0 together with state +0x10C.
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\Shogun2_fixes\tools\ghidra
#       -postScript sweep_fieldsets.py <out.csv> <offsets> [<min_hits>]
#
#   offsets   colon list of hex displacements (":" because the Windows batch
#             launcher splits arguments on "," and "=")
#   min_hits  a function is written when one base register touches at least
#             this many DISTINCT offsets of the list (default 2)
#
# Memory access = any operand of the form [REG + OFF] (no index), read or
# write; LEA counts as kind "a" (address taken); base ESP excluded. One row per (function, base
# register) with the offsets touched, read / write marks and the first
# address of each.
#
# @category pathfix
# @runtime Jython

import os
import re

program = currentProgram
listing = program.getListing()
funcmgr = program.getFunctionManager()

plain_mem_re = re.compile(r"^(?:\w+ ptr )?\[(E[A-Z]{2}) \+ (0x[0-9a-fA-F]+)\]$")
READ_FIRST = set(["CMP", "TEST", "PUSH", "FLD", "FILD", "FCOM", "FCOMP", "FCOMPP", "COMISS", "UCOMISS",
                  "COMISD", "UCOMISD"])


def safe_write(target, text):
    if os.path.exists(target):
        tmp = target + "_tmp"
        fh = open(tmp, "w"); fh.write(text); fh.close()
        bak = target + ".bak"
        n = 1
        while os.path.exists(bak):
            n += 1
            bak = "%s.bak%d" % (target, n)
        os.rename(target, bak)
        os.rename(tmp, target)
    else:
        fh = open(target, "w"); fh.write(text); fh.close()


def accesses(ins):
    out = []
    mnem = ins.getMnemonicString().upper()
    n = ins.getNumOperands()
    for i in range(n):
        text = ins.getDefaultOperandRepresentation(i)
        if "*" in text:
            continue
        m = plain_mem_re.match(text)
        if not m:
            continue
        reg = m.group(1)
        if reg == "ESP":
            continue
        kind = "r"
        if mnem == "LEA":
            kind = "a"
        elif i == 0 and mnem not in READ_FIRST and n > 1:
            kind = "w" if mnem.startswith("MOV") or mnem.startswith("FST") else "rw"
        if n == 1 and mnem.startswith("FST"):
            kind = "w"
        out.append((reg, int(m.group(2), 16), kind))
    return out


def run():
    args = getScriptArgs()
    if len(args) < 2:
        println("usage: sweep_fieldsets.py <out.csv> <offsets> [<min_hits>]")
        return
    target = os.path.abspath(args[0])
    wanted = set(int(x, 16) for x in args[1].replace(",", ":").split(":") if x)
    min_hits = int(args[2]) if len(args) > 2 else 2
    lines = ["entry,name,body_bytes,base,n_offsets,offsets"]
    nfun = 0
    nrow = 0
    for f in funcmgr.getFunctions(True):
        nfun += 1
        per = {}
        for ins in listing.getInstructions(f.getBody(), True):
            for reg, off, kind in accesses(ins):
                if off not in wanted:
                    continue
                d = per.setdefault(reg, {})
                if off not in d:
                    d[off] = [kind, ins.getAddress().getOffset()]
                elif kind not in d[off][0]:
                    d[off][0] = d[off][0] + kind
        for reg in sorted(per.keys()):
            d = per[reg]
            if len(d) < min_hits:
                continue
            parts = ["%x%s@%x" % (o, d[o][0], d[o][1]) for o in sorted(d.keys())]
            lines.append("%x,%s,%d,%s,%d,%s" % (f.getEntryPoint().getOffset(), f.getName(),
                                                f.getBody().getNumAddresses(), reg, len(d), " ".join(parts)))
            nrow += 1
    safe_write(target, "\n".join(lines) + "\n")
    println("sweep_fieldsets: %d functions, %d rows -> %s" % (nfun, nrow, target))


run()
