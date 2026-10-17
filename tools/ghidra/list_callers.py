# List the callers (call references) of the given functions, with call sites.
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\Shogun2_fixes\tools\ghidra
#       -postScript list_callers.py <out.txt> <entry> [<entry> ...]
#
# @category pathfix
# @runtime Jython

import os

program = currentProgram
funcmgr = program.getFunctionManager()
refmgr = program.getReferenceManager()
space = program.getAddressFactory().getDefaultAddressSpace()


def run():
    args = getScriptArgs()
    if len(args) < 2:
        println("usage: list_callers.py <out.txt> <entry> ...")
        return
    target = os.path.abspath(args[0])
    out = []
    for text in args[1:]:
        addr = space.getAddress(int(text, 16))
        f = funcmgr.getFunctionAt(addr)
        out.append("CALLEE %s (%s)" % (addr, f.getName() if f else "no function"))
        for ref in refmgr.getReferencesTo(addr):
            caller = funcmgr.getFunctionContaining(ref.getFromAddress())
            out.append("  %s  %-10s from %s  size %s" % (
                ref.getFromAddress(), ref.getReferenceType().getName(),
                caller.getName() if caller else "(no function)",
                caller.getBody().getNumAddresses() if caller else "-"))
        out.append("")
    text = "\n".join(out) + "\n"
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
    println("wrote %s" % target)


run()
