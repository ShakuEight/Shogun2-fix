# List every code reference to a set of strings, with the containing function, and optionally
# decompile those functions.
#
# Ghidra headless post-script (Jython). Never modifies the program (functions it has to create for
# orphan code exist in memory only; run with -readOnly).
#
#   ghx86.bat <logname> find_string_refs.py <out.txt> <strings.txt> [<decompile_dir>]
#
# <strings.txt> holds one string per line (read from a file because the batch launcher splits
# arguments on "=" and ","). A line "w:<text>" searches the UTF-16LE form, "s:<text>" also keeps
# hits that are only a substring of a longer string; plain lines must match a whole ASCII string.
# Blank lines and lines starting with "#" are ignored.
#
# Output: one block per string, one line per reference:
#   <ref addr>  <kind>  <function name> @ <entry> (RVA 0x...)  size <bytes>
#
# Existing files are never overwritten blind (tmp / .bak / move).
#
# @category tools
# @runtime Jython

import codecs
import os

import jarray
from ghidra.app.decompiler import DecompInterface

program = currentProgram
memory = program.getMemory()
listing = program.getListing()
refmgr = program.getReferenceManager()
funcmgr = program.getFunctionManager()
image_base = program.getImageBase()

MAX_DECOMPILES = 60


def to_jbytes(values):
    return jarray.array([v - 256 if v > 127 else v for v in values], 'b')


def find_all(pattern_bytes):
    hits = []
    jb = to_jbytes(pattern_bytes)
    for block in memory.getBlocks():
        if not block.isInitialized() or block.isExecute():
            continue
        start, end = block.getStart(), block.getEnd()
        cur = start
        while cur is not None and cur.compareTo(end) <= 0:
            found = memory.findBytes(cur, end, jb, None, True, monitor)
            if found is None:
                break
            hits.append(found)
            if found.compareTo(end) >= 0:
                break
            cur = found.add(1)
    return hits


def byte_at(addr):
    try:
        return memory.getByte(addr) & 0xFF
    except:
        return None


def bounds(hit, length, step):
    """(start of the string containing the hit, is_whole). step 1 = ASCII, 2 = UTF-16LE."""
    start = hit
    for _ in range(600):
        prev = start.subtract(step)
        b = byte_at(prev)
        hi = byte_at(prev.add(1)) if step == 2 else 0
        if b is None or b < 0x20 or b > 0x7E or hi != 0:
            break
        start = prev
    after = byte_at(hit.add(length))
    after2 = byte_at(hit.add(length + 1)) if step == 2 else 0
    whole = start.equals(hit) and after == 0 and after2 == 0
    return start, whole


def read_string(addr, step, limit=160):
    out = []
    cur = addr
    for _ in range(limit):
        b = byte_at(cur)
        if b is None or b == 0:
            break
        out.append(chr(b) if 0x20 <= b <= 0x7E else '?')
        cur = cur.add(step)
    return "".join(out)


def orphan_function(addr):
    ins = listing.getInstructionContaining(addr)
    if ins is None:
        return None
    for _ in range(20000):
        prev = ins.getFallFrom()
        if prev is None:
            break
        p = listing.getInstructionAt(prev)
        if p is None or funcmgr.getFunctionContaining(prev) is not None:
            break
        ins = p
    start = ins.getAddress()
    func = funcmgr.getFunctionAt(start)
    if func is None:
        try:
            func = createFunction(start, None)
        except:
            func = None
    if func is not None and not func.getBody().contains(addr):
        return None
    return func


def referencing(data_addr):
    out = []
    for ref in refmgr.getReferencesTo(data_addr):
        frm = ref.getFromAddress()
        func = funcmgr.getFunctionContaining(frm)
        if func is not None:
            out.append((frm, "code", func))
            continue
        func = orphan_function(frm)
        if func is not None:
            out.append((frm, "code_orphan", func))
            continue
        hop = False
        for ref2 in refmgr.getReferencesTo(frm):
            f2 = funcmgr.getFunctionContaining(ref2.getFromAddress())
            if f2 is not None:
                out.append((ref2.getFromAddress(), "via_pointer@%s" % frm, f2))
                hop = True
        if not hop:
            out.append((frm, "data_only", None))
    return out


def safe_write(target, text):
    target = os.path.abspath(target)
    parent = os.path.dirname(target)
    if parent and not os.path.isdir(parent):
        os.makedirs(parent)
    if not os.path.exists(target):
        fh = open(target, "wb"); fh.write(text); fh.close()
        return "created " + target
    tmp = target + "_tmp"
    fh = open(tmp, "wb"); fh.write(text); fh.close()
    bak = target + ".bak"
    n = 1
    while os.path.exists(bak):
        n += 1
        bak = "%s.bak%d" % (target, n)
    os.rename(target, bak)
    os.rename(tmp, target)
    return "replaced %s (previous kept as %s)" % (target, bak)


def run():
    args = getScriptArgs()
    if len(args) < 2:
        println("usage: find_string_refs.py <out.txt> <strings.txt> [<decompile_dir>]")
        return
    out_path, strings_path = args[0], args[1]
    decomp_dir = args[2] if len(args) > 2 else None
    fh = codecs.open(strings_path, "r", "utf-8")
    specs = [l.rstrip("\r\n") for l in fh.readlines()]
    fh.close()

    lines = []
    funcs = []
    seen_funcs = set()
    for spec in specs:
        if not spec.strip() or spec.startswith("#"):
            continue
        wide = spec.startswith("w:")
        sub = spec.startswith("s:")
        text = spec[2:] if (wide or sub) else spec
        step = 2 if wide else 1
        pat = []
        for c in text:
            pat.append(ord(c))
            if wide:
                pat.append(0)
        lines.append("STRING %s%s" % ("(utf16) " if wide else "", text))
        n = 0
        for hit in find_all(pat):
            start, whole = bounds(hit, len(pat), step)
            if not whole and not sub:
                continue
            label = "exact" if whole else "in: " + read_string(start, step)
            refs = referencing(start)
            lines.append("  data %s  %s  (%d refs)" % (start, label, len(refs)))
            for (frm, kind, func) in refs:
                n += 1
                if func is None:
                    lines.append("    %s  %-12s (no function)" % (frm, kind))
                    continue
                e = func.getEntryPoint()
                lines.append("    %s  %-12s %s @ %s (RVA 0x%x)  size %d" % (
                    frm, kind, func.getName(), e, e.subtract(image_base), func.getBody().getNumAddresses()))
                k = e.getOffset()
                if k not in seen_funcs:
                    seen_funcs.add(k)
                    funcs.append(func)
        if n == 0:
            lines.append("  (no references)")
        lines.append("")
    println("find_string_refs: " + safe_write(out_path, ("\n".join(lines) + "\n").encode("utf-8")))

    if decomp_dir:
        decomp = DecompInterface()
        decomp.openProgram(program)
        for func in funcs[:MAX_DECOMPILES]:
            res = decomp.decompileFunction(func, 240, monitor)
            if res is None or not res.decompileCompleted():
                println("  decompile FAILED for %s" % func.getName())
                continue
            text = "// %s @ %s (RVA 0x%x) in %s\n\n%s" % (
                func.getName(), func.getEntryPoint(), func.getEntryPoint().subtract(image_base),
                program.getName(), res.getDecompiledFunction().getC())
            safe_write(os.path.join(decomp_dir, func.getName() + ".c"), text.encode("utf-8"))
        decomp.dispose()
        println("find_string_refs: %d decompiles written to %s" % (min(len(funcs), MAX_DECOMPILES), decomp_dir))


run()
