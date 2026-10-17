# Dump one CSV row per function of the current program: size, ordered call
# sequence, float/large constants and referenced strings. This is the offline
# input for tools/propagate_map.py, which walks callees/callers out from the
# confirmed x86 <-> x64 pins without needing Ghidra again.
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\pathfix\tools\ghidra
#       -postScript dump_callgraph.py <out.csv>
#
# Columns:
#   entry      function entry address (hex, no 0x)
#   name       Ghidra name
#   thunk_of   entry of the thunked function, if this is a thunk
#   body_bytes, n_instr
#   n_callers  distinct calling functions
#   n_ptr_refs data references to the entry (vtable slots, callbacks)
#   call_seq   direct call targets in address order, ';' separated; '?' marks
#              an indirect call, 'j:<entry>' a tail jump into another function
#   consts     32-bit immediates / read-only loads that are not image
#              addresses, hex, ';' separated
#   strings    up to 8 referenced C strings, ' | ' separated
#
# @category pathfix
# @runtime Jython

import csv
import os

from ghidra.program.model.scalar import Scalar

program = currentProgram
memory = program.getMemory()
listing = program.getListing()
refmgr = program.getReferenceManager()
funcmgr = program.getFunctionManager()
addr_space = program.getAddressFactory().getDefaultAddressSpace()


def read_cstring(addr, limit=60):
    out = []
    cur = addr
    for _ in range(limit):
        try:
            b = memory.getByte(cur) & 0xFF
        except:
            break
        if b == 0:
            break
        out.append(chr(b) if 0x20 <= b <= 0x7E and b != 0x7C else '?')
        cur = cur.add(1)
    return "".join(out)


def is_const(v):
    if v < 0x10000 or v >= 0xFFFF0000:
        return False
    return not memory.contains(addr_space.getAddress(v))


def describe(func):
    body = func.getBody()
    entry = func.getEntryPoint()
    n_instr = 0
    seq = []
    consts = set()
    strings = []
    it = listing.getInstructions(body, True)
    while it.hasNext():
        ins = it.next()
        n_instr += 1
        flow = ins.getFlowType()
        if flow.isCall():
            target = None
            for ref in ins.getReferencesFrom():
                if ref.getReferenceType().isCall():
                    f = funcmgr.getFunctionAt(ref.getToAddress())
                    if f is not None:
                        target = f
                        break
            seq.append("%x" % target.getEntryPoint().getOffset() if target is not None else "?")
        elif flow.isJump() and not flow.isConditional():
            for ref in ins.getReferencesFrom():
                if ref.getReferenceType().isJump():
                    to = ref.getToAddress()
                    if not body.contains(to):
                        f = funcmgr.getFunctionAt(to)
                        if f is not None:
                            seq.append("j:%x" % to.getOffset())
        for op in range(ins.getNumOperands()):
            for obj in ins.getOpObjects(op):
                if isinstance(obj, Scalar):
                    v = obj.getUnsignedValue() & 0xFFFFFFFF
                    if is_const(v):
                        consts.add(v)
        for ref in ins.getReferencesFrom():
            rt = ref.getReferenceType()
            if not rt.isData():
                continue
            to = ref.getToAddress()
            block = memory.getBlock(to)
            if block is None or not block.isInitialized() or block.isExecute():
                continue
            data = listing.getDataAt(to)
            if data is not None and data.hasStringValue():
                if len(strings) < 8:
                    s = read_cstring(to)
                    if len(s) >= 4 and s not in strings:
                        strings.append(s)
                continue
            if rt.isRead() and not block.isWrite():
                try:
                    v = memory.getInt(to) & 0xFFFFFFFF
                except:
                    continue
                if is_const(v):
                    consts.add(v)
    n_ptr = 0
    for ref in refmgr.getReferencesTo(entry):
        if ref.getReferenceType().isData():
            n_ptr += 1
    thunk = ""
    if func.isThunk():
        t = func.getThunkedFunction(False)
        if t is not None:
            thunk = "%x" % t.getEntryPoint().getOffset()
    return ["%x" % entry.getOffset(), func.getName(), thunk, body.getNumAddresses(), n_instr,
            len(func.getCallingFunctions(monitor)), n_ptr, ";".join(seq),
            ";".join("%08x" % c for c in sorted(consts)), " | ".join(strings)]


def run():
    args = getScriptArgs()
    if len(args) < 1:
        println("usage: dump_callgraph.py <out.csv>")
        return
    target = os.path.abspath(args[0])
    parent = os.path.dirname(target)
    if not os.path.isdir(parent):
        os.makedirs(parent)
    tmp = target + "_tmp"
    fh = open(tmp, "wb")
    w = csv.writer(fh, lineterminator="\n")
    w.writerow(["entry", "name", "thunk_of", "body_bytes", "n_instr", "n_callers", "n_ptr_refs",
                "call_seq", "consts", "strings"])
    total = funcmgr.getFunctionCount()
    n = 0
    for func in funcmgr.getFunctions(True):
        if func.isExternal():
            continue
        w.writerow(describe(func))
        n += 1
        if n % 10000 == 0:
            println("dump_callgraph: %d / %d functions" % (n, total))
        if monitor.isCancelled():
            break
    fh.close()
    # Never overwrite an existing dump blind.
    if os.path.exists(target):
        bak = target + ".bak"
        k = 1
        while os.path.exists(bak):
            k += 1
            bak = "%s.bak%d" % (target, k)
        os.rename(target, bak)
        println("dump_callgraph: previous dump kept as " + bak)
    os.rename(tmp, target)
    println("dump_callgraph: wrote %d functions to %s" % (n, target))


run()
