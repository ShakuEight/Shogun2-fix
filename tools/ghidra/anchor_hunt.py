# Locate the cross-version anchor functions (strings / float constants from
# CLAUDE.md) in the current program and write one CSV row per
# (anchor, referencing function), with size / call-graph / constant metrics
# that tools/compare_anchors.py uses to pair x86 functions with x64 ones.
#
# Ghidra headless post-script (Jython). Never modifies the program.
#
#   analyzeHeadless <projdir> <proj> -process <program> -noanalysis -readOnly
#       -scriptPath E:\_SHOGUN\pathfix\tools\ghidra
#       -postScript anchor_hunt.py <out.csv> [<decompile_dir>]
#
# If <decompile_dir> is given, the decompile of every distinct anchor function
# is written there as <function name>.c.
#
# Existing output files are never overwritten blind: new content goes to
# <name>_tmp, the old file is moved to <name>.bak, then tmp is moved in.
#
# @category pathfix
# @runtime Jython

import csv
import os

import jarray
from ghidra.app.decompiler import DecompInterface
from ghidra.program.model.scalar import Scalar

# (anchor id, kind, payload, whole_string_only)
#   string : functions referencing the string
#   float  : functions using the 32-bit constant (rdata load or immediate)
#   all_of : functions referencing every one of the listed whole strings
ANCHORS = [
    ("move_fast_parser", "string", "expecting at most 4 arguments (location,angle,width,move_fast)", True),
    ("angle_const_182", "float", 0x43360B60, True),
    ("ui_move_orientation_width", "string", "Move Orientation Width", True),
    ("ui_move_rotation_angle", "string", "Move Rotation Angle", True),
    ("corridor_factor", "string", "ENTITY_ACTION_ATTACK_CORRIDOR_FACTOR", True),
    ("entity_action_attack_cpp", "string", "EntityActionAttack.cpp", False),
    ("find_waypoint", "string", "FIND_WAYPOINT", True),
    ("move_to_waypoint", "string", "MOVE_TO_WAYPOINT", True),
    ("arrived_at_waypoint", "string", "ARRIVED_AT_WAYPOINT", True),
    ("soft_collision_cpp", "string", "SoftCollisionObject.cpp", False),
    # Never a standalone string: it only occurs inside the database-table
    # error messages ("In table %S: ... DATABASE_TABLE<NAVAL_UNIT_RECORD ...")
    # and "NAVAL_UNIT_RECORD::STATS", so substring matches must be kept.
    ("naval_unit_record", "string", "NAVAL_UNIT_RECORD", False),
    ("rocket_torpedo", "all_of", ["rocket", "torpedo"], True),
]

MAX_DECOMPILES = 80
DECOMPILE_TIMEOUT_S = 240

HEADER = ["anchor", "anchor_kind", "match", "data_addr", "ref_addr", "ref_kind",
          "func_name", "func_entry", "func_rva", "body_bytes", "n_instr",
          "n_callees", "n_callers", "callees", "consts", "strings", "note"]

program = currentProgram
memory = program.getMemory()
listing = program.getListing()
refmgr = program.getReferenceManager()
funcmgr = program.getFunctionManager()
image_base = program.getImageBase()
addr_space = program.getAddressFactory().getDefaultAddressSpace()


def to_jbytes(values):
    return jarray.array([v - 256 if v > 127 else v for v in values], 'b')


def find_all(pattern_bytes):
    """All addresses in initialized memory where the byte pattern occurs."""
    hits = []
    jb = to_jbytes(pattern_bytes)
    for block in memory.getBlocks():
        if not block.isInitialized():
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


def string_bounds(hit, length):
    """Return (start address of the C string containing the hit, is_whole)."""
    start = hit
    for _ in range(600):
        prev = start.subtract(1)
        b = byte_at(prev)
        if b is None or b < 0x20 or b > 0x7E:
            break
        start = prev
    after = byte_at(hit.add(length))
    whole = (start.equals(hit) and after == 0)
    return start, whole


def read_cstring(addr, limit=200):
    out = []
    cur = addr
    for _ in range(limit):
        b = byte_at(cur)
        if b is None or b == 0:
            break
        out.append(chr(b) if 0x20 <= b <= 0x7E else '?')
        cur = cur.add(1)
    return "".join(out)


def rva_of(addr):
    return addr.subtract(image_base)


_metrics_cache = {}


def func_metrics(func):
    key = func.getEntryPoint().getOffset()
    if key in _metrics_cache:
        return _metrics_cache[key]
    body = func.getBody()
    n_instr = 0
    consts = set()
    strings = []
    it = listing.getInstructions(body, True)
    while it.hasNext():
        ins = it.next()
        n_instr += 1
        for op in range(ins.getNumOperands()):
            for obj in ins.getOpObjects(op):
                if isinstance(obj, Scalar):
                    v = obj.getUnsignedValue() & 0xFFFFFFFF
                    if v >= 0x10000 and v < 0xFFFF0000 and not memory.contains(addr_space.getAddress(v)):
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
                if len(strings) < 16:
                    s = read_cstring(to, 80)
                    if s and s not in strings:
                        strings.append(s)
                continue
            if rt.isRead() and not block.isWrite():
                try:
                    v = memory.getInt(to) & 0xFFFFFFFF
                except:
                    continue
                if v >= 0x10000 and v < 0xFFFF0000 and not memory.contains(addr_space.getAddress(v)):
                    consts.add(v)
    callees = sorted(f.getEntryPoint().getOffset() for f in func.getCalledFunctions(monitor))
    callers = func.getCallingFunctions(monitor)
    m = {
        "name": func.getName(),
        "entry": func.getEntryPoint(),
        "body_bytes": body.getNumAddresses(),
        "n_instr": n_instr,
        "n_callees": len(callees),
        "n_callers": len(callers),
        "callees": callees,
        "consts": sorted(consts),
        "strings": strings,
    }
    _metrics_cache[key] = m
    return m


def orphan_function(addr):
    """Function for code that Ghidra disassembled but never made a function of.

    Static initializers and small table-dispatched handlers are only reached
    through pointer tables, so auto-analysis leaves them as bare instructions.
    Walk back along the fall-through chain to the first instruction and create
    the function IN MEMORY ONLY: the script is run with -readOnly, so nothing
    is saved to the project. Rows produced this way have ref_kind code_orphan.
    """
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


def referencing_functions(data_addr):
    """List of (ref_addr, ref_kind, function or None) for refs to data_addr."""
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
        # Reference from data (pointer table). Follow one hop: who reads the
        # pointer slot itself.
        hop = False
        for ref2 in refmgr.getReferencesTo(frm):
            f2 = funcmgr.getFunctionContaining(ref2.getFromAddress())
            if f2 is not None:
                out.append((ref2.getFromAddress(), "via_pointer@%s" % frm, f2))
                hop = True
        if not hop:
            out.append((frm, "data_only", None))
    return out


def const_users(value):
    """Functions that use a 32-bit constant: (data_addr, ref_addr, kind, func)."""
    out = []
    pat = [(value >> (8 * i)) & 0xFF for i in range(4)]
    for hit in find_all(pat):
        block = memory.getBlock(hit)
        if block is not None and block.isExecute():
            ins = listing.getInstructionContaining(hit)
            if ins is None:
                continue
            func = funcmgr.getFunctionContaining(ins.getAddress())
            if func is not None:
                out.append((hit, ins.getAddress(), "immediate", func))
            continue
        any_ref = False
        for (frm, kind, func) in referencing_functions(hit):
            any_ref = True
            out.append((hit, frm, "load_" + kind, func))
        if not any_ref:
            out.append((hit, None, "unreferenced", None))
    return out


def string_users(text, whole_only):
    """(data_addr, match, ref_addr, kind, func) for every user of the string."""
    out = []
    pat = [ord(c) for c in text]
    for hit in find_all(pat):
        start, whole = string_bounds(hit, len(pat))
        if whole_only and not whole:
            continue
        match = "exact" if whole else "substring_of:" + read_cstring(start, 120)
        refs = referencing_functions(start)
        if not refs:
            out.append((start, match, None, "unreferenced", None))
        for (frm, kind, func) in refs:
            out.append((start, match, frm, kind, func))
    return out


def make_row(anchor, kind, match, data_addr, ref_addr, ref_kind, func, note=""):
    row = {h: "" for h in HEADER}
    row.update({"anchor": anchor, "anchor_kind": kind, "match": match,
                "data_addr": str(data_addr) if data_addr is not None else "",
                "ref_addr": str(ref_addr) if ref_addr is not None else "",
                "ref_kind": ref_kind, "note": note})
    if func is not None:
        m = func_metrics(func)
        row.update({
            "func_name": m["name"],
            "func_entry": str(m["entry"]),
            "func_rva": "0x%x" % rva_of(m["entry"]),
            "body_bytes": m["body_bytes"],
            "n_instr": m["n_instr"],
            "n_callees": m["n_callees"],
            "n_callers": m["n_callers"],
            "callees": ";".join("%x" % c for c in m["callees"]),
            "consts": ";".join("%08x" % c for c in m["consts"]),
            "strings": " | ".join(m["strings"]),
        })
    return row


def safe_write(target, text):
    target = os.path.abspath(target)
    parent = os.path.dirname(target)
    if not os.path.isdir(parent):
        os.makedirs(parent)
    if not os.path.exists(target):
        fh = open(target, "wb")
        fh.write(text)
        fh.close()
        return "created " + target
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
    return "replaced %s (previous kept as %s)" % (target, bak)


def run():
    args = getScriptArgs()
    if len(args) < 1:
        println("usage: anchor_hunt.py <out.csv> [<decompile_dir>]")
        return
    out_csv = args[0]
    decomp_dir = args[1] if len(args) > 1 else None

    println("anchor_hunt: program %s, image base %s, %d functions" % (
        program.getName(), image_base, funcmgr.getFunctionCount()))

    rows = []
    anchor_funcs = []   # ordered distinct functions for the decompile dump
    seen_funcs = set()

    def note_func(func):
        if func is None:
            return
        k = func.getEntryPoint().getOffset()
        if k not in seen_funcs:
            seen_funcs.add(k)
            anchor_funcs.append(func)

    for (anchor, kind, payload, whole_only) in ANCHORS:
        monitor.setMessage("anchor " + anchor)
        before = len(rows)
        if kind == "string":
            seen = set()
            for (data_addr, match, ref_addr, ref_kind, func) in string_users(payload, whole_only):
                key = (str(data_addr), func.getEntryPoint().getOffset() if func else str(ref_addr))
                if key in seen:
                    continue
                seen.add(key)
                rows.append(make_row(anchor, kind, match, data_addr, ref_addr, ref_kind, func))
                note_func(func)
        elif kind == "float":
            seen = set()
            for (data_addr, ref_addr, ref_kind, func) in const_users(payload):
                key = (func.getEntryPoint().getOffset() if func else str(data_addr), ref_kind)
                if key in seen:
                    continue
                seen.add(key)
                rows.append(make_row(anchor, kind, "0x%08x" % payload, data_addr, ref_addr, ref_kind, func))
                note_func(func)
        elif kind == "all_of":
            per_string = []
            first_ref = {}
            for text in payload:
                funcs = {}
                for (data_addr, match, ref_addr, ref_kind, func) in string_users(text, True):
                    if func is None:
                        continue
                    k = func.getEntryPoint().getOffset()
                    funcs[k] = func
                    first_ref.setdefault((k, text), (data_addr, ref_addr, ref_kind))
                per_string.append(funcs)
            common = set(per_string[0].keys())
            for funcs in per_string[1:]:
                common &= set(funcs.keys())
            if common:
                for k in sorted(common):
                    func = per_string[0][k]
                    data_addr, ref_addr, ref_kind = first_ref[(k, payload[0])]
                    rows.append(make_row(anchor, kind, "all:" + "+".join(payload), data_addr, ref_addr,
                                         ref_kind, func))
                    note_func(func)
            else:
                for idx, funcs in enumerate(per_string):
                    for k in sorted(funcs.keys()):
                        data_addr, ref_addr, ref_kind = first_ref[(k, payload[idx])]
                        rows.append(make_row(anchor, kind, "only:" + payload[idx], data_addr, ref_addr,
                                             ref_kind, funcs[k],
                                             note="no single function references all of " + "+".join(payload)))
        n_new = len(rows) - before
        funcs_here = sorted(set(r["func_name"] for r in rows[before:] if r["func_name"]))
        println("  %-28s %3d rows, functions: %s" % (
            anchor, n_new, ", ".join(funcs_here[:10]) + (" ..." if len(funcs_here) > 10 else "")))
        if n_new == 0:
            rows.append(make_row(anchor, kind, "", None, None, "not_found", None,
                                 note="anchor not found in this program"))

    import StringIO
    buf = StringIO.StringIO()
    w = csv.writer(buf, lineterminator="\n")
    w.writerow(HEADER)
    for r in rows:
        w.writerow([r[h] for h in HEADER])
    println("anchor_hunt: " + safe_write(out_csv, buf.getvalue()))

    if decomp_dir:
        decomp = DecompInterface()
        decomp.openProgram(program)
        todo = anchor_funcs[:MAX_DECOMPILES]
        if len(anchor_funcs) > MAX_DECOMPILES:
            println("anchor_hunt: %d anchor functions, decompiling the first %d" % (
                len(anchor_funcs), MAX_DECOMPILES))
        for func in todo:
            monitor.setMessage("decompile " + func.getName())
            res = decomp.decompileFunction(func, DECOMPILE_TIMEOUT_S, monitor)
            if res is None or not res.decompileCompleted():
                println("  decompile FAILED for %s: %s" % (
                    func.getName(), res.getErrorMessage() if res is not None else "no result"))
                continue
            text = "// %s @ %s (RVA 0x%x) in %s\n\n%s" % (
                func.getName(), func.getEntryPoint(), rva_of(func.getEntryPoint()),
                program.getName(), res.getDecompiledFunction().getC())
            safe_write(os.path.join(decomp_dir, func.getName() + ".c"), text.encode("utf-8"))
        decomp.dispose()
        println("anchor_hunt: decompiles written to " + decomp_dir)


run()
