# Export every function of the program as CSV: start_rva, end_rva, entry_rva,
# name, one row per contiguous range of the function body (end_rva is one past
# the last byte; a fragmented function has several rows). Used by
# tools/analyze_capture_v3.py to name watch writer and stack addresses.
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\pathfix\tools\ghidra
#       -postScript dump_functions.py <out.csv>
#
# @category pathfix
# @runtime Jython

import os

program = currentProgram
funcmgr = program.getFunctionManager()
image_base = program.getImageBase()


def run():
    args = getScriptArgs()
    if len(args) != 1:
        println("usage: dump_functions.py <out.csv>")
        return
    target = os.path.abspath(args[0])
    rows = []
    for func in funcmgr.getFunctions(True):
        entry = func.getEntryPoint().subtract(image_base)
        # One row per contiguous range of the body, so a fragmented function
        # does not claim the code between its fragments.
        for rng in func.getBody().getAddressRanges():
            lo = rng.getMinAddress().subtract(image_base)
            hi = rng.getMaxAddress().subtract(image_base) + 1
            rows.append((lo, hi, entry, func.getName()))
    rows.sort()
    lines = ["start_rva,end_rva,entry_rva,name"]
    for lo, hi, entry, name in rows:
        lines.append("0x%06X,0x%06X,0x%06X,%s" % (lo, hi, entry, name))
    text = "\n".join(lines) + "\n"
    if os.path.exists(target):
        tmp = target + "_tmp"
        fh = open(tmp, "w")
        fh.write(text)
        fh.close()
        bak = target + ".bak"
        n = 1
        while os.path.exists(bak):
            n += 1
            bak = "%s.bak%d" % (target, n)
        os.rename(target, bak)
        os.rename(tmp, target)
    else:
        fh = open(target, "w")
        fh.write(text)
        fh.close()
    println("wrote %d functions to %s" % (len(rows), target))


run()
