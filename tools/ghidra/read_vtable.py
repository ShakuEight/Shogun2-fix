# Print the entries of vtables (pointer arrays) with the function at each entry.
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\Shogun2_fixes\tools\ghidra
#       -postScript read_vtable.py <out.txt> <count> <addr> [<addr> ...]
#
# Pointer size follows the program (4 bytes x86, 8 bytes x64).
#
# @category pathfix
# @runtime Jython

import os

program = currentProgram
mem = program.getMemory()
funcmgr = program.getFunctionManager()
space = program.getAddressFactory().getDefaultAddressSpace()
psize = program.getDefaultPointerSize()


def run():
    args = getScriptArgs()
    out = os.path.abspath(args[0])
    count = int(args[1])
    lines = []
    for a in args[2:]:
        base = space.getAddress(a)
        lines.append("VTABLE %s" % base)
        for i in range(count):
            at = base.add(i * psize)
            if psize == 8:
                v = mem.getLong(at) & 0xFFFFFFFFFFFFFFFF
            else:
                v = mem.getInt(at) & 0xFFFFFFFF
            f = funcmgr.getFunctionAt(space.getAddress(v))
            lines.append("  [%2d] +0x%02X  %x  %s" % (i, i * psize, v, f.getName() if f else "(no function)"))
    tmp = out + "_tmp"
    fh = open(tmp if os.path.exists(out) else out, "w")
    fh.write("\n".join(lines) + "\n")
    fh.close()
    if os.path.exists(tmp):
        os.rename(out, out + ".bak")
        os.rename(tmp, out)
    println("wrote %s" % out)


run()
