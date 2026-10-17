# List functions that access memory operands [REG + DISP] for every one of a
# set of displacements (loads and stores), e.g. the unit's slot-pair vector
# +0x518 / +0x51C / +0x520, to find the consumers of a struct field.
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\Shogun2_fixes\tools\ghidra
#       -postScript find_disp.py <out.txt> <need> <disp>[:<disp>...]
#
#   need   minimum number of the listed displacements a function must use
#          (all of them: give the list length)
#   disp   colon list of hex displacements (the batch launcher splits on ",")
# Output: one line per matching function with, per displacement, the
# addresses of the instructions that use it (LEA included, ESP base excluded).
#
# @category pathfix
# @runtime Jython

import os
import re

program = currentProgram
listing = program.getListing()
funcmgr = program.getFunctionManager()

PAT = re.compile(r"\[(E[A-Z]{2}|R[A-Z0-9]{1,3}) \+ (0x[0-9a-fA-F]+)\]")


def run():
    args = getScriptArgs()
    out = os.path.abspath(args[0])
    need = int(args[1])
    disps = [int(x, 16) for x in args[2].split(":")]
    dset = set(disps)
    lines = []
    for f in funcmgr.getFunctions(True):
        hits = {}
        for ins in listing.getInstructions(f.getBody(), True):
            s = ins.toString()
            for m in PAT.finditer(s):
                if m.group(1) in ("ESP", "RSP"):
                    continue
                d = int(m.group(2), 16)
                if d in dset:
                    hits.setdefault(d, []).append(str(ins.getAddress()))
        if len(hits) >= need:
            parts = []
            for d in disps:
                if d in hits:
                    parts.append("+0x%X: %s" % (d, " ".join(hits[d][:8])))
            lines.append("%s %s (%d bytes)  %s" % (f.getEntryPoint(), f.getName(), f.getBody().getNumAddresses(),
                                                  " | ".join(parts)))
    if os.path.exists(out):
        tmp = out + "_tmp"
        fh = open(tmp, "w")
        fh.write("\n".join(lines) + "\n")
        fh.close()
        os.rename(out, out + ".bak")
        os.rename(tmp, out)
    else:
        fh = open(out, "w")
        fh.write("\n".join(lines) + "\n")
        fh.close()
    println("wrote %s (%d functions)" % (out, len(lines)))


run()
