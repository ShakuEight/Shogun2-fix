# Sweep the whole image (or a call tree) for LOADS of [reg + OFF] for given
# displacements, and score each hit for evidence that the base register holds
# a path node pointer (Phase 10 D2; derived from sweep_stores.py).
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\pathfix\tools\ghidra
#       -postScript sweep_loads.py <out.csv> <dir_offsets> <width_offsets> [<min_score>] [<root> ...]
#
#   dir_offsets    colon list of hex displacements reported as category "direction"
#                  (Phase 10: 64:68:90:94; the Windows batch launcher splits
#                  arguments on "," and "=", so the separator is ":")
#   width_offsets  colon list reported as category "width" (Phase 10: 74:78); "-" for none
#   min_score      rows with a smaller node score are counted but not written (default 1)
#   roots          optional function entries; if given, only their call trees
#                  (depth 3) are swept, otherwise every function of the image
#
# Load: an instruction that reads memory operand [REG + OFF] (no index): any
# operand after the first, or the first operand of a read-only mnemonic
# (CMP, TEST, PUSH, FLD, FILD, FCOM*, COMISS, UCOMISS). Stores are skipped.
#
# Excluded: LEA (address arithmetic, not a load), base ESP (stack locals), and
# base EBP in functions that set up an EBP frame (MOV EBP,ESP).
#
# Node evidence; "node_score" is the sum of the weights:
#   acc  (2) the function calls a node point accessor or sampler wrapper:
#            FUN_10381da0 (node+0x80), FUN_103a5d40 (node+0x88), FUN_10381cb0,
#            FUN_103a5ce0
#   src  (2) within the 12 instructions before the hit, REG was loaded from
#            [X + 0xA8] (cursor node in P); weight 1 for [X + 0x8] (next) or
#            [X + 0xA0] (list head)
#   sig  (2) the same base register is used for at least 3 OTHER node-record
#            fields in the function: +0x0C side, +0x64..+0x70 perpendiculars,
#            +0x74 / +0x78 widths, +0x7C clearance, +0x80..+0x8C followed points,
#            +0x90 / +0x94 along vector, +0x9C flag (weight 1 for exactly 2)
#   walk (1) the function loads [REG + 0x8] with the same base register
#   pair (0) both halves of the pair read with that base (informative only)
# Builder-family functions (already known to read the record) are marked in
# the "known" column so they can be separated.
#
# @category pathfix
# @runtime Jython

import os
import re

program = currentProgram
listing = program.getListing()
funcmgr = program.getFunctionManager()
space = program.getAddressFactory().getDefaultAddressSpace()

READ_FIRST = set(["CMP", "TEST", "PUSH", "FLD", "FILD", "FCOM", "FCOMP", "FCOMPP", "COMISS", "UCOMISS",
                  "COMISD", "UCOMISD"])
ACCESSORS = {0x10381da0: "FUN_10381da0", 0x103a5d40: "FUN_103a5d40",
             0x10381cb0: "FUN_10381cb0", 0x103a5ce0: "FUN_103a5ce0"}
KNOWN = {0x103baac0: "record ctor", 0x1039bb40: "path builder", 0x1039add0: "path build",
         0x103592a0: "node creator", 0x10351690: "record copy", 0x10372920: "template copy",
         0x1039c4b0: "path build 2", 0x10366e40: "segment search", 0x10374270: "corner clearance"}
REC_FIELDS = set([0x0C, 0x64, 0x68, 0x6C, 0x70, 0x74, 0x78, 0x7C, 0x80, 0x84, 0x88, 0x8C, 0x90, 0x94, 0x9C])
SRC_OFFS = set([0xA8, 0x8, 0xA0])

mem_re = re.compile(r"\[(E[A-Z]{2}) \+ (0x[0-9a-fA-F]+)\]")
plain_mem_re = re.compile(r"^(?:\w+ ptr )?\[(E[A-Z]{2}) \+ (0x[0-9a-fA-F]+)\]$")
reg_load_re = re.compile(r"^MOV (E[A-Z]{2}),dword ptr \[(E[A-Z]{2}) \+ (0x[0-9a-fA-F]+)\]$")


def safe_write(target, text):
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


def loads_of(ins):
    """[(reg, off)] for memory operands this instruction reads."""
    out = []
    mnem = ins.getMnemonicString().upper()
    if mnem == "LEA":
        return out
    n = ins.getNumOperands()
    for i in range(n):
        if i == 0 and n > 1 and mnem not in READ_FIRST:
            continue
        if i == 0 and n == 1 and mnem not in READ_FIRST:
            continue
        text = ins.getDefaultOperandRepresentation(i)
        if "*" in text:
            continue
        m = plain_mem_re.match(text)
        if m:
            out.append((m.group(1), int(m.group(2), 16)))
    return out


def tree(roots, depth):
    seen = set()
    order = []
    frontier = roots[:]
    for f in roots:
        seen.add(f.getEntryPoint())
        order.append(f)
    for d in range(depth):
        nxt = []
        for f in frontier:
            for c in f.getCalledFunctions(monitor):
                if c.isThunk() or c.isExternal() or c.getEntryPoint() in seen:
                    continue
                seen.add(c.getEntryPoint())
                order.append(c)
                nxt.append(c)
        frontier = nxt
    return order


def run():
    args = getScriptArgs()
    if len(args) < 3:
        println("usage: sweep_loads.py <out.csv> <dir_offsets> <width_offsets> [<min_score>] [<root> ...]")
        return
    target = os.path.abspath(args[0])
    dir_offs = set(int(x, 16) for x in args[1].replace(",", ":").split(":") if x)
    width_offs = set() if args[2] == "-" else set(int(x, 16) for x in args[2].replace(",", ":").split(":") if x)
    min_score = int(args[3]) if len(args) > 3 else 1
    roots = []
    for t in args[4:]:
        f = funcmgr.getFunctionAt(space.getAddress(int(t, 16)))
        if f is not None:
            roots.append(f)
    funcs = tree(roots, 3) if roots else list(funcmgr.getFunctions(True))
    wanted = dir_offs | width_offs

    rows = []
    raw = {"direction": 0, "width": 0}
    nfunc = 0
    for f in funcs:
        if monitor.isCancelled():
            break
        if f.isThunk() or f.isExternal():
            continue
        nfunc += 1
        insns = list(listing.getInstructions(f.getBody(), True))
        ebp_frame = False
        for ins in insns[:6]:
            if ins.toString() == "MOV EBP,ESP":
                ebp_frame = True
        hits = []
        reads = {}          # reg -> set of offsets read with that base
        for i, ins in enumerate(insns):
            for reg, off in loads_of(ins):
                if reg == "ESP" or (reg == "EBP" and ebp_frame):
                    continue
                reads.setdefault(reg, set()).add(off)
                if off in wanted:
                    hits.append((i, reg, off))
        if not hits:
            continue
        acc = []
        for ins in insns:
            if ins.getFlowType().isCall():
                for ref in ins.getReferencesFrom():
                    o = ref.getToAddress().getOffset()
                    if o in ACCESSORS and ACCESSORS[o] not in acc:
                        acc.append(ACCESSORS[o])
        entry = f.getEntryPoint().getOffset()
        for i, reg, off in hits:
            cat = "width" if off in width_offs else "direction"
            raw[cat] += 1
            ev = []
            score = 0
            if acc:
                ev.append("acc"); score += 2
            regreads = reads.get(reg, set())
            for j in range(max(0, i - 12), i):
                m = reg_load_re.match(insns[j].toString())
                if m and m.group(1) == reg and int(m.group(3), 16) in SRC_OFFS:
                    so = int(m.group(3), 16)
                    ev.append("src[%s+0x%X]" % (m.group(2), so))
                    score += 2 if so == 0xA8 else 1
                    break
            sig = sorted(regreads & (REC_FIELDS - set([off])))
            if len(sig) >= 3:
                ev.append("sig"); score += 2
            elif len(sig) == 2:
                ev.append("sig2"); score += 1
            if 0x8 in regreads:
                ev.append("walk"); score += 1
            if cat == "direction":
                if (off in (0x64, 0x68) and 0x64 in regreads and 0x68 in regreads) or                    (off in (0x90, 0x94) and 0x90 in regreads and 0x94 in regreads):
                    ev.append("pair")
            else:
                if 0x74 in regreads and 0x78 in regreads:
                    ev.append("pair")
            fields = ":".join("%X" % x for x in sig)
            if score < min_score:
                continue
            ins = insns[i]
            rows.append((cat, score, f.getName(), "0x%06X" % (entry - 0x10000000), str(ins.getAddress()),
                         "+0x%X" % off, reg, ins.toString(), ";".join(ev), fields, ";".join(acc),
                         KNOWN.get(entry, ""), f.getBody().getNumAddresses()))

    rows.sort(key=lambda r: (r[0], -r[1], r[2], r[4]))
    lines = ["category,node_score,function,func_rva,address,offset,base_reg,instruction,evidence,same_base_fields,accessors,known,func_size"]
    for r in rows:
        lines.append('%s,%d,%s,%s,%s,%s,%s,"%s",%s,%s,%s,%s,%d' % r)
    lines.append("# functions swept: %d (%s); raw loads: direction %d, width %d; rows written (node_score >= %d): %d"
                 % (nfunc, "call trees depth 3" if roots else "whole image", raw["direction"], raw["width"],
                    min_score, len(rows)))
    safe_write(target, "\n".join(lines) + "\n")
    println("sweep_loads: %d functions, raw direction %d, width %d, written %d -> %s" % (
        nfunc, raw["direction"], raw["width"], len(rows), target))


run()
