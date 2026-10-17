#!/usr/bin/env python3
"""
stack_calc.py - compute x86 (32-bit) stack offsets for a naked detour.

Never hand-calculate [esp+N] in a detour: simulate the pushes instead.

The detour is described as a sequence of stack operations executed from the
hooked location. For an ENTRY hook the stack at that point is:

    [ESP+0]        return address
    [ESP+4*k]      stack argument k (k = 1, 2, ...)

Supported operations (one per --op, or comma separated in one --ops):

    pushad            push EAX,ECX,EDX,EBX,ESP(original),EBP,ESI,EDI (32 bytes)
    pushfd            push EFLAGS (4 bytes)
    push              push any 4-byte value (4 bytes)
    push:NAME         same, and remember the slot under NAME
    sub:N             sub esp, N
    mark:NAME         record the current ESP as a named base pointer

Mid-function patch (v17): the hook point is the instruction after a CALL
inside a function, so there is no return address. The caller's locals that
were passed to the callee by address are described by the LEA that formed the
address and the number of caller pushes already done when it ran:

    --mid --caller-pushes 7 --callee-ret 0x1C --lea apex:0x28@5 --lea wpbuf:0x5C@6

means: 7 pushes before the CALL, the callee pops 0x1C bytes (checked: must be
7 * 4, so ESP after the call equals ESP before the first push), and
`LEA EAX,[ESP+0x28]` ran after 5 pushes, so the local is at [ESP+0x28-0x14]
= [ESP+0x14] at the hook point.

Output: for every named base (and the final ESP) the offset of each saved
register, the return address and the stack arguments. With --header the
offsets relative to one chosen base are also written as a C header, so the
numbers are pasted by the tool and never typed.

Example (the v12 capture detours):

    python tools/stack_calc.py --ops pushad,pushfd,mark:ctx,push:ctxptr --args 4 \
        --base ctx --prefix CTX --header src/stack_offsets_v12.h
"""
import argparse
import os
import sys

# Order in which PUSHAD pushes; the last one pushed ends up at the lowest address.
PUSHAD_ORDER = ["EAX", "ECX", "EDX", "EBX", "ESP_BEFORE_PUSHAD", "EBP", "ESI", "EDI"]


def mid_locals(caller_pushes, callee_ret, leas):
    """Locals at the hook point after a CALL: name -> offset from ESP there."""
    if caller_pushes * 4 != callee_ret:
        raise SystemExit("caller pushes %d * 4 != callee RET 0x%X: ESP after the call is not "
                         "ESP before the pushes" % (caller_pushes, callee_ret))
    out = {}
    for spec in leas:
        name, _, rest = spec.partition(":")
        disp, _, k = rest.partition("@")
        disp, k = int(disp, 0), int(k, 0)
        if not 0 <= k <= caller_pushes:
            raise SystemExit("lea %s: push count %d outside 0..%d" % (name, k, caller_pushes))
        off = disp - 4 * k
        if off < 0:
            raise SystemExit("lea %s: local would be below the hook-point ESP" % name)
        out[name.strip().upper()] = off
    return out


def simulate(ops, n_args, entry_extra=0, mid=None):
    """Return (slots, marks, depth).

    Addresses are expressed relative to ESP at hook entry (entry ESP = 0, the
    stack grows downward, so pushes produce negative addresses).
    slots: name -> address; marks: name -> address.
    """
    esp = 0
    if mid is not None:
        slots = dict(mid)
    else:
        slots = {"RET": entry_extra}
        for k in range(1, n_args + 1):
            slots["ARG%d" % k] = entry_extra + 4 * k
    marks = {}
    for op in ops:
        name, _, param = op.partition(":")
        name = name.strip().lower()
        if name == "pushad":
            before = esp
            for reg in PUSHAD_ORDER:
                esp -= 4
                slots[reg] = esp
            slots["_ESP_BEFORE_PUSHAD_VALUE"] = before
        elif name == "pushfd":
            esp -= 4
            slots["EFLAGS"] = esp
        elif name == "push":
            esp -= 4
            if param:
                slots[param.strip().upper()] = esp
        elif name == "sub":
            esp -= int(param, 0)
        elif name == "mark":
            marks[param.strip()] = esp
        else:
            raise SystemExit("unknown operation: %r" % op)
    marks["final"] = esp
    return slots, marks, -esp


def self_test():
    """Known answers, checked on every run."""
    slots, marks, depth = simulate(["pushad"], 2)
    b = marks["final"]
    # Intel SDM: after PUSHAD, [ESP]=EDI ... [ESP+28]=EAX.
    assert slots["EDI"] - b == 0 and slots["ESI"] - b == 4 and slots["EBP"] - b == 8
    assert slots["ESP_BEFORE_PUSHAD"] - b == 12 and slots["EBX"] - b == 16
    assert slots["EDX"] - b == 20 and slots["ECX"] - b == 24 and slots["EAX"] - b == 28
    assert slots["RET"] - b == 32 and slots["ARG1"] - b == 36 and slots["ARG2"] - b == 40
    slots, marks, depth = simulate(["pushad", "pushfd", "mark:ctx", "push:p"], 1)
    assert depth == 40 and slots["EFLAGS"] - marks["ctx"] == 0 and slots["ECX"] - marks["ctx"] == 28
    assert slots["RET"] - marks["ctx"] == 36 and slots["P"] - marks["final"] == 0
    # v17 mid-function frame at 0x103df6c0 (FUN_103df3f0 after CALL FUN_10394e80).
    loc = mid_locals(7, 0x1C, ["apex:0x28@5", "wpbuf:0x5C@6"])
    assert loc == {"APEX": 0x14, "WPBUF": 0x44}
    slots, marks, depth = simulate(["pushad", "pushfd", "mark:ctx", "push:p"], 0, mid=loc)
    assert slots["APEX"] - marks["ctx"] == 0x38 and slots["EAX"] - marks["ctx"] == 0x20
    assert "RET" not in slots


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--ops", default="", help="comma separated operations")
    ap.add_argument("--op", action="append", default=[], help="one operation (repeatable)")
    ap.add_argument("--args", type=int, default=4, help="number of stack arguments to list")
    ap.add_argument("--entry-extra", type=lambda s: int(s, 0), default=0,
                    help="bytes already pushed between function entry and the hook point "
                         "(0 for an entry hook)")
    ap.add_argument("--mid", action="store_true", help="mid-function hook point after a CALL (no RET)")
    ap.add_argument("--caller-pushes", type=int, default=0, help="--mid: pushes before the CALL")
    ap.add_argument("--callee-ret", type=lambda s: int(s, 0), default=0, help="--mid: callee RET imm16")
    ap.add_argument("--lea", action="append", default=[], help="--mid: NAME:DISP@PUSHES_DONE (repeatable)")
    ap.add_argument("--base", default="final", help="mark to express header offsets against")
    ap.add_argument("--prefix", default="STK", help="prefix for the #define names")
    ap.add_argument("--header", default=None, help="write a C header with the offsets")
    args = ap.parse_args()

    self_test()
    ops = [o for o in args.ops.split(",") if o.strip()] + args.op
    if not ops:
        ap.error("no operations given")
    mid = None
    if args.mid:
        mid = mid_locals(args.caller_pushes, args.callee_ret, args.lea)
        args.args = 0
        print("mid-function hook point: caller locals at hook-point ESP: " +
              ", ".join("%s=[ESP+0x%X]" % (n, o) for n, o in sorted(mid.items())))
    slots, marks, depth = simulate(ops, args.args, args.entry_extra, mid)
    if args.base not in marks:
        ap.error("unknown base %r (marks: %s)" % (args.base, ", ".join(sorted(marks))))

    order = ["EFLAGS"] + list(reversed(PUSHAD_ORDER)) + ["RET"] + ["ARG%d" % k for k in range(1, args.args + 1)]
    extra = [n for n in sorted(slots) if n not in order and not n.startswith("_")]
    names = [n for n in order if n in slots] + extra

    print("operations: " + ", ".join(ops))
    print("total bytes pushed at the end: %d (0x%X)" % (depth, depth))
    print("")
    for mark in sorted(marks, key=lambda m: -marks[m]):
        base = marks[mark]
        print("offsets from ESP at '%s' (entry ESP %+d):" % (mark, base))
        for n in names:
            off = slots[n] - base
            if off < 0:
                continue   # pushed after this mark
            print("    [%s%+5d]  0x%02X  %s" % ("ESP", off, off, n))
        print("")

    if args.header:
        base = marks[args.base]
        guard = "%s_OFFSETS_H" % args.prefix
        lines = ["// GENERATED by tools/stack_calc.py. Do not edit by hand.",
                 "// operations: " + ", ".join(ops),
                 "// offsets are from ESP at mark '%s' (%d bytes below ESP at hook entry)" % (args.base, -base),
                 "#pragma once",
                 "#ifndef " + guard, "#define " + guard, ""]
        for n in names:
            off = slots[n] - base
            if off < 0:
                continue
            lines.append("#define %s_OFF_%-18s 0x%02X" % (args.prefix, n, off))
        lines += ["#define %s_BYTES_BELOW_ENTRY      0x%02X" % (args.prefix, -base), "", "#endif", ""]
        text = "\n".join(lines)
        target = os.path.abspath(args.header)
        if os.path.exists(target):
            tmp = target + "_tmp"
            with open(tmp, "w") as fh:
                fh.write(text)
            bak, k = target + ".bak", 1
            while os.path.exists(bak):
                k += 1
                bak = "%s.bak%d" % (target, k)
            os.rename(target, bak)
            os.rename(tmp, target)
            print("replaced %s (previous kept as %s)" % (target, bak))
        else:
            with open(target, "w") as fh:
                fh.write(text)
            print("created %s" % target)
    return 0


if __name__ == "__main__":
    sys.exit(main())
