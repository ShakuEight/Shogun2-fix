# Find virtual calls on an object loaded from [REG + FIELD]: an instruction
# that loads [REG + FIELD] followed, within WINDOW instructions in the same
# function, by CALL dword/qword ptr [R + SLOT]. Lists the slot offsets used.
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\Shogun2_fixes\tools\ghidra
#       -postScript find_vcall.py <out.txt> <field_hex> <window>
#
# Heuristic (no data flow): a call within the window is reported even if the
# loaded pointer was not the one called. Read the listing before trusting it.
#
# @category pathfix
# @runtime Jython

import os
import re

program = currentProgram
listing = program.getListing()
funcmgr = program.getFunctionManager()

CALLPAT = re.compile(r"CALL (?:dword|qword) ptr \[(E[A-Z]{2}|R[A-Z0-9]{1,3})(?: \+ (0x[0-9a-fA-F]+))?\]")


def run():
    args = getScriptArgs()
    out = os.path.abspath(args[0])
    field = int(args[1], 16)
    window = int(args[2])
    fpat = re.compile(r"\[(E[A-Z]{2}|R[A-Z0-9]{1,3}) \+ 0x%x\]" % field)
    lines = []
    for f in funcmgr.getFunctions(True):
        ins = list(listing.getInstructions(f.getBody(), True))
        found = []
        for i, a in enumerate(ins):
            s = a.toString()
            if s.startswith("LEA") or not fpat.search(s):
                continue
            for b in ins[i + 1:i + 1 + window]:
                m = CALLPAT.search(b.toString())
                if m:
                    found.append("%s->%s[+%s]" % (a.getAddress(), b.getAddress(), m.group(2) or "0x0"))
                    break
        if found:
            lines.append("%s %s (%d bytes)  %s" % (f.getEntryPoint(), f.getName(), f.getBody().getNumAddresses(),
                                                  " ".join(found[:12])))
    tmp = out + "_tmp"
    fh = open(tmp if os.path.exists(out) else out, "w")
    fh.write("\n".join(lines) + "\n")
    fh.close()
    if os.path.exists(tmp):
        os.rename(out, out + ".bak")
        os.rename(tmp, out)
    println("wrote %s (%d functions)" % (out, len(lines)))


run()
