# Print the instruction listing (not the decompile) of whole functions:
# address, raw bytes, instruction text, and the name of any called function.
# For reading which register offsets are loaded or passed around a call.
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\pathfix\tools\ghidra
#       -postScript dump_listing.py <out.txt> <entry> [<entry> ...]
#
# @category pathfix
# @runtime Jython

import os

program = currentProgram
listing = program.getListing()
funcmgr = program.getFunctionManager()
image_base = program.getImageBase()
addr_space = program.getAddressFactory().getDefaultAddressSpace()


def describe(ins):
    notes = []
    for ref in ins.getReferencesFrom():
        rt = ref.getReferenceType()
        if rt.isCall() or rt.isJump():
            f = funcmgr.getFunctionAt(ref.getToAddress())
            if f is not None and rt.isCall():
                notes.append("-> " + f.getName())
    return "   ; " + ", ".join(notes) if notes else ""


def run():
    args = getScriptArgs()
    if len(args) < 2:
        println("usage: dump_listing.py <out.txt> <entry> ...")
        return
    target = os.path.abspath(args[0])
    out = []
    for text in args[1:]:
        addr = addr_space.getAddress(int(text, 16))
        func = funcmgr.getFunctionAt(addr)
        if func is None:
            out.append("NO FUNCTION AT %s" % addr)
            continue
        body = func.getBody()
        out.append("FUNCTION %s entry %s RVA 0x%X  (%d bytes)" % (
            func.getName(), addr, addr.subtract(image_base), body.getNumAddresses()))
        out.append("  signature: %s" % func.getSignature().getPrototypeString())
        it = listing.getInstructions(body, True)
        while it.hasNext():
            ins = it.next()
            raw = " ".join("%02X" % (b & 0xFF) for b in ins.getBytes())
            out.append("  %s  %-30s %s%s" % (ins.getAddress(), raw, ins.toString(), describe(ins)))
        out.append("")
    text = "\n".join(out) + "\n"
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
        parent = os.path.dirname(target)
        if not os.path.isdir(parent):
            os.makedirs(parent)
        fh = open(target, "w")
        fh.write(text)
        fh.close()
    println("wrote %s" % target)


run()
