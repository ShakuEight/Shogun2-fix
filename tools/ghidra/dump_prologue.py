# Print the first instructions of the given functions with their raw bytes,
# so that entry-hook stolen bytes are taken on whole-instruction boundaries
# from the analysed program instead of being counted by hand.
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\Shogun2_fixes\tools\ghidra
#       -postScript dump_prologue.py <out.txt> <min_bytes> <entry> [<entry> ...]
#
# For each entry it lists instructions until at least <min_bytes> bytes are
# covered, flags any instruction that is position dependent (relative
# call/jump), and reports whether any jump or call elsewhere in the function
# targets an address inside the stolen range (which would make the patch
# unsafe).
#
# @category pathfix
# @runtime Jython

import os

program = currentProgram
listing = program.getListing()
funcmgr = program.getFunctionManager()
refmgr = program.getReferenceManager()
image_base = program.getImageBase()
addr_space = program.getAddressFactory().getDefaultAddressSpace()


def run():
    args = getScriptArgs()
    if len(args) < 3:
        println("usage: dump_prologue.py <out.txt> <min_bytes> <entry> ...")
        return
    target = os.path.abspath(args[0])
    min_bytes = int(args[1])
    out = []
    for text in args[2:]:
        addr = addr_space.getAddress(int(text, 16))
        func = funcmgr.getFunctionAt(addr)
        out.append("FUNCTION %s entry %s RVA 0x%X" % (
            func.getName() if func else "(no function)", addr, addr.subtract(image_base)))
        covered = 0
        ins = listing.getInstructionAt(addr)
        stolen = []
        relocatable = True
        while ins is not None and covered < min_bytes:
            raw = ins.getBytes()
            hexs = " ".join("%02X" % (b & 0xFF) for b in raw)
            flow = ins.getFlowType()
            note = ""
            if flow.isCall() or flow.isJump() or flow.isTerminal():
                note = "   <-- control flow: NOT safe to relocate"
                relocatable = False
            out.append("  +0x%02X  %-24s %s%s" % (covered, hexs, ins.toString(), note))
            stolen.extend(b & 0xFF for b in raw)
            covered += len(raw)
            ins = ins.getNext()
        out.append("  STOLEN_LEN %d" % covered)
        out.append("  STOLEN_BYTES " + ", ".join("0x%02X" % b for b in stolen))
        inbound = []
        for off in range(1, covered):
            a = addr.add(off)
            for ref in refmgr.getReferencesTo(a):
                if ref.getReferenceType().isFlow():
                    inbound.append("%s -> %s" % (ref.getFromAddress(), a))
        out.append("  BRANCHES_INTO_STOLEN_RANGE %s" % (", ".join(inbound) if inbound else "none"))
        out.append("  RELOCATABLE %s" % ("yes" if relocatable and not inbound else "NO"))
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
    for line in out:
        println(line)


run()
