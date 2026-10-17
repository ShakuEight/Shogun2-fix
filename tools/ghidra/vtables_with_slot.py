# Find vtables (runs of consecutive pointers to code in initialized,
# non-executable memory) that are long enough to have a given slot offset,
# and print the function at that slot. Used to resolve a virtual call such as
# CALL [RAX + 0x3A0] on an object of unknown class.
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\pathfix\tools\ghidra
#       -postScript vtables_with_slot.py <out.txt> <slot_hex> [<min_run>]
#
# A run starts at a pointer whose address is referenced (a vtable store) or
# after a non-code-pointer value. Heuristic: read the functions before trusting.
#
# @category pathfix
# @runtime Jython

import os

program = currentProgram
mem = program.getMemory()
funcmgr = program.getFunctionManager()
refmgr = program.getReferenceManager()
space = program.getAddressFactory().getDefaultAddressSpace()
psize = program.getDefaultPointerSize()


def run():
    args = getScriptArgs()
    out = os.path.abspath(args[0])
    slot = int(args[1], 16)
    min_run = int(args[2]) if len(args) > 2 else slot // psize + 1
    lines = []
    for blk in mem.getBlocks():
        if blk.isExecute() or not blk.isInitialized():
            continue
        start = blk.getStart()
        n = blk.getSize() // psize
        run_start = None
        run_len = 0
        for i in range(int(n)):
            at = start.add(i * psize)
            try:
                v = mem.getLong(at) if psize == 8 else (mem.getInt(at) & 0xFFFFFFFF)
            except:
                v = 0
            ok = False
            if v:
                try:
                    tgt = space.getAddress(v & 0xFFFFFFFFFFFFFFFF)
                    b = mem.getBlock(tgt)
                    ok = b is not None and b.isExecute()
                except:
                    ok = False
            if ok and run_start is not None and refmgr.hasReferencesTo(at) and run_len > 0:
                if run_len >= min_run:
                    emit(lines, run_start, run_len, slot)
                run_start = at
                run_len = 1
            elif ok:
                if run_start is None:
                    run_start = at
                    run_len = 0
                run_len += 1
            else:
                if run_start is not None and run_len >= min_run:
                    emit(lines, run_start, run_len, slot)
                run_start = None
                run_len = 0
    tmp = out + "_tmp"
    fh = open(tmp if os.path.exists(out) else out, "w")
    fh.write("\n".join(lines) + "\n")
    fh.close()
    if os.path.exists(tmp):
        os.rename(out, out + ".bak")
        os.rename(tmp, out)
    println("wrote %s (%d vtables)" % (out, len(lines)))


def emit(lines, run_start, run_len, slot):
    at = run_start.add(slot)
    v = mem.getLong(at) if psize == 8 else (mem.getInt(at) & 0xFFFFFFFF)
    f = funcmgr.getFunctionAt(space.getAddress(v))
    lines.append("%s len %d  [+0x%X] %x %s" % (run_start, run_len, slot, v, f.getName() if f else "(no function)"))


run()
