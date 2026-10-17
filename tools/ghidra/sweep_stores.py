# Sweep a call tree for stores to [reg + OFF] (given offsets) and for
# allocation-size immediates pushed shortly before a call.
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\pathfix\tools\ghidra
#       -postScript sweep_stores.py <out.csv> <depth> <offsets> <sizes> <root> [<root> ...]
#
#   offsets  comma list of hex displacements, e.g. 74,78
#   sizes    comma list of hex allocation sizes, e.g. a0,100
#   roots    function entry addresses (hex)
#
# Store: a MOV / MOVSS / MOVD / FST / FSTP / MOVAPS-family instruction whose
# FIRST operand is a memory operand [reg + OFF] (any base register, no index).
# Allocation candidate: PUSH <size> or MOV reg,<size> followed by a CALL within
# the next four instructions; the call target is recorded.
#
# @category pathfix
# @runtime Jython

import os
import re

program = currentProgram
listing = program.getListing()
funcmgr = program.getFunctionManager()
image_base = program.getImageBase()
space = program.getAddressFactory().getDefaultAddressSpace()
monitor_ = monitor

STORE_MNEMONICS = set(["MOV", "MOVSS", "MOVD", "FST", "FSTP", "MOVAPS", "MOVUPS", "MOVQ", "MOVLPS"])


def callees(func):
    try:
        return list(func.getCalledFunctions(monitor_))
    except Exception:
        return []


def run():
    args = getScriptArgs()
    if len(args) < 5:
        println("usage: sweep_stores.py <out.csv> <depth> <offsets> <sizes> <root> ...")
        return
    target = os.path.abspath(args[0])
    depth = int(args[1])
    offsets = [int(x, 16) for x in args[2].split(",") if x]
    sizes = [int(x, 16) for x in args[3].split(",") if x]
    store_re = re.compile(r"^(?:\w+ ptr )?\[(E[A-Z]{2}) \+ (0x[0-9a-fA-F]+)\]$")

    # breadth-first over the call tree, each function once at its smallest depth
    seen = {}
    order = []
    frontier = []
    for text in args[4:]:
        f = funcmgr.getFunctionAt(space.getAddress(int(text, 16)))
        if f is None:
            println("no function at %s" % text)
            continue
        if f.getEntryPoint() not in seen:
            seen[f.getEntryPoint()] = (0, f.getName())
            order.append((f, 0, f.getName()))
            frontier.append((f, f.getName()))
    for d in range(1, depth + 1):
        nxt = []
        for f, root in frontier:
            for c in callees(f):
                if c.isThunk() or c.isExternal():
                    continue
                if c.getEntryPoint() in seen:
                    continue
                seen[c.getEntryPoint()] = (d, root)
                order.append((c, d, root))
                nxt.append((c, root))
        frontier = nxt

    rows = []
    for f, d, root in order:
        insns = list(listing.getInstructions(f.getBody(), True))
        for i, ins in enumerate(insns):
            mnem = ins.getMnemonicString().upper()
            if mnem in STORE_MNEMONICS and ins.getNumOperands() >= 1:
                op0 = ins.getDefaultOperandRepresentation(0)
                m = store_re.match(op0)
                if m and int(m.group(2), 16) in offsets:
                    rows.append((root, d, f.getName(), ins.getAddress(), "store_+0x%X" % int(m.group(2), 16),
                                 ins.toString(), ""))
            if mnem in ("PUSH", "MOV") and ins.getNumOperands() >= 1:
                last = ins.getDefaultOperandRepresentation(ins.getNumOperands() - 1)
                try:
                    val = int(last, 16) if last.lower().startswith("0x") else None
                except ValueError:
                    val = None
                if val is not None and val in sizes:
                    for j in range(i + 1, min(i + 5, len(insns))):
                        if insns[j].getFlowType().isCall():
                            tgt = ""
                            for ref in insns[j].getReferencesFrom():
                                cf = funcmgr.getFunctionAt(ref.getToAddress())
                                if cf is not None:
                                    tgt = cf.getName()
                            rows.append((root, d, f.getName(), ins.getAddress(), "size_0x%X" % val,
                                         ins.toString(), "call at %s %s" % (insns[j].getAddress(), tgt)))
                            break

    lines = ["root,depth,function,address,kind,instruction,note"]
    for root, d, fname, addr, kind, text, note in rows:
        lines.append('%s,%d,%s,%s,%s,"%s","%s"' % (root, d, fname, addr, kind, text, note))
    lines.append("# functions swept: %d (depth <= %d from %d roots)" % (len(order), depth, len(args) - 4))
    text = "\n".join(lines) + "\n"
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
    println("swept %d functions, %d hits -> %s" % (len(order), len(rows), target))


run()
