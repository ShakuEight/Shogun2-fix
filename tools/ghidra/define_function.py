# Define a function at a given entry (CreateFunctionCmd), default FUN_ name.
#
# Ghidra headless post-script (Jython). MODIFIES the program: run WITHOUT
# -readOnly, only for entries the plan authorizes (Phase 10 D1: 0x101B7B70,
# the vtable code at slot 0x115c1744, s2x86). Refuses if a function already
# exists at or contains the entry. Prints the resulting body ranges.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis
#       -scriptPath E:\_SHOGUN\pathfix\tools\ghidra
#       -postScript define_function.py <entry> [<entry> ...]
#
# @category pathfix
# @runtime Jython

from ghidra.app.cmd.function import CreateFunctionCmd

program = currentProgram
fm = program.getFunctionManager()
space = program.getAddressFactory().getDefaultAddressSpace()

for text in getScriptArgs():
    a = space.getAddress(int(text, 16))
    if fm.getFunctionContaining(a) is not None:
        println("define_function: %s already in %s, nothing done" % (a, fm.getFunctionContaining(a).getName()))
        continue
    cmd = CreateFunctionCmd(a)
    ok = cmd.applyTo(program, monitor)
    f = fm.getFunctionAt(a)
    if not ok or f is None:
        println("define_function: FAILED at %s: %s" % (a, cmd.getStatusMsg()))
        continue
    println("define_function: created %s at %s, body %s (%d bytes), calling convention %s" % (
        f.getName(), a, f.getBody(), f.getBody().getNumAddresses(), f.getCallingConventionName()))
