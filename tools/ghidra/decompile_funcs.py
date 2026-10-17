# Decompile a list of functions of the current program to .c files.
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\Shogun2_fixes\tools\ghidra
#       -postScript decompile_funcs.py <out_dir> <entry>[:<file stem>] ...
#
# <entry> is a hex address. With ":<file stem>" the file is written as
# <file stem>.c (used to name x64 decompiles after their x86 counterpart);
# otherwise the Ghidra function name is used. (The separator is ":" because
# the Windows batch launcher splits arguments on "=".)
#
# Existing files are never overwritten blind (tmp / .bak / move).
# An entry with no function (e.g. a handler reached only through a table) gets one created in
# memory; with -readOnly (as ghx86.bat runs it) the project is not changed.
#
# @category pathfix
# @runtime Jython

import os

from ghidra.app.decompiler import DecompInterface

program = currentProgram
funcmgr = program.getFunctionManager()
image_base = program.getImageBase()
addr_space = program.getAddressFactory().getDefaultAddressSpace()


def safe_write(target, text):
    parent = os.path.dirname(target)
    if not os.path.isdir(parent):
        os.makedirs(parent)
    if not os.path.exists(target):
        fh = open(target, "wb")
        fh.write(text)
        fh.close()
        return "created"
    tmp = target + "_tmp"
    fh = open(tmp, "wb")
    fh.write(text)
    fh.close()
    bak = target + ".bak"
    n = 1
    while os.path.exists(bak):
        n += 1
        bak = "%s.bak%d" % (target, n)
    os.rename(target, bak)
    os.rename(tmp, target)
    return "replaced (previous kept as %s)" % os.path.basename(bak)


def run():
    args = getScriptArgs()
    if len(args) < 2:
        println("usage: decompile_funcs.py <out_dir> <entry>[:<file stem>] ...")
        return
    out_dir = os.path.abspath(args[0])
    decomp = DecompInterface()
    decomp.openProgram(program)
    for spec in args[1:]:
        if ":" in spec:
            entry_text, stem = spec.split(":", 1)
        else:
            entry_text, stem = spec, None
        addr = addr_space.getAddress(int(entry_text, 16))
        func = funcmgr.getFunctionAt(addr)
        if func is None:
            func = funcmgr.getFunctionContaining(addr)
        if func is None:
            # Code reached only through pointer tables (command handlers) has no function yet.
            # Create it in memory: ghx86.bat opens the project -readOnly, so nothing is saved.
            try:
                func = createFunction(addr, None)
            except:
                func = None
            if func is not None:
                println("decompile_funcs: created in-memory function at %s" % entry_text)
        if func is None:
            println("decompile_funcs: no function at %s" % entry_text)
            continue
        res = decomp.decompileFunction(func, 300, monitor)
        if res is None or not res.decompileCompleted():
            println("decompile_funcs: FAILED %s: %s" % (
                func.getName(), res.getErrorMessage() if res is not None else "no result"))
            continue
        header = "// %s @ %s (RVA 0x%x) in %s\n" % (
            func.getName(), func.getEntryPoint(), func.getEntryPoint().subtract(image_base), program.getName())
        if stem:
            header += "// x86 counterpart: %s\n" % stem
        text = header + "\n" + res.getDecompiledFunction().getC()
        target = os.path.join(out_dir, (stem or func.getName()) + ".c")
        println("decompile_funcs: %s -> %s (%s, %d lines)" % (
            func.getName(), target, safe_write(target, text.encode("utf-8")), text.count("\n")))
    decomp.dispose()


run()
