#!/usr/bin/env python3
"""
compare_anchors.py - pair x86 functions with their x64 counterparts.

Joins docs/anchors_x86.csv and docs/anchors_x64.csv (from anchor_hunt.py) on
the anchor id, scores every candidate pair on body_bytes ratio, n_callees
ratio and shared constants / strings, and writes docs/function_map_v1.md.

A pair is marked CONFIRMED only when both decompiles agree on a source-level
fact listed in FACTS below. Everything else is CANDIDATE, whatever its score.

Pairs found by walking the call graph (tools/propagate_map.py) can be merged
in with --pairs; they go through the same fact checks.

Usage:
    python tools/compare_anchors.py
        --x86 docs/anchors_x86.csv --x64 docs/anchors_x64.csv
        --x86-decomp docs/anchor_decompiles/x86
        --x64-decomp docs/anchor_decompiles/x64 --x64-decomp docs/x64_decompiles
        [--pairs docs/walk_pairs.csv] -o docs/function_map_v1.md
"""
import argparse
import csv
import math
import os
import re
import struct
import sys
from collections import OrderedDict, defaultdict

X86_BASE = 0x10000000


def f32_hex(value, ulps=1):
    """Hex strings (as in the consts column) for a float32 and its neighbours."""
    raw = struct.unpack("<I", struct.pack("<f", value))[0]
    return {"%08x" % (raw + d) for d in range(-ulps, ulps + 1)}


# ---------------------------------------------------------------------------
# Source-level facts. Each fact is (label, checker). A checker receives the
# decompile text and the function's constant set and returns a short evidence
# string when the fact holds, or None.
# ---------------------------------------------------------------------------

def fact_text(pattern, flags=re.S):
    rx = re.compile(pattern, flags)

    def check(text, consts):
        m = rx.search(text)
        if not m:
            return None
        line_start = text.rfind("\n", 0, m.start()) + 1
        line_end = text.find("\n", m.end())
        return "decompile: " + " ".join(text[line_start:line_end].split())[:110]
    return check


def fact_float(value, text_pattern):
    """Float constant: accepted from the decompile text or from the consts set."""
    rx = re.compile(text_pattern)
    hexes = f32_hex(value)

    def check(text, consts):
        m = rx.search(text)
        if m:
            line_start = text.rfind("\n", 0, m.start()) + 1
            line_end = text.find("\n", m.end())
            return "decompile: " + " ".join(text[line_start:line_end].split())[:110]
        hit = hexes & consts
        if hit:
            return "constant 0x%s loaded by the function" % sorted(hit)[0]
        return None
    return check


FLOAT_DEREF = r"\*\(float \*\)\((\w+) \+ (0x[0-9a-f]+|\d+)\)"


def fact_max_tracker(text, consts):
    """if (hw > obj+X) obj+X = hw, in any of the shapes the decompiler emits."""
    lines = text.split("\n")
    for i, line in enumerate(lines):
        if "if (" not in line or ("<" not in line and ">" not in line):
            continue
        for m in re.finditer(FLOAT_DEREF, line):
            target = m.group(0)
            for j in range(i, min(i + 5, len(lines))):
                if re.search(re.escape(target) + r" = [^=]", lines[j]):
                    return "decompile: %s ... %s" % (" ".join(line.split())[:70],
                                                     " ".join(lines[j].split())[:60])
    # x64 often turns the compare+store into a temp: f = obj+X; if (f <= hw) f = hw; obj+X = f
    for m in re.finditer(r"(\w+) = " + FLOAT_DEREF + r";", text):
        var, base, off = m.group(1), m.group(2), m.group(3)
        tail = text[m.end():m.end() + 400]
        if re.search(r"if \([^)]*\b%s\b[^)]*[<>]" % re.escape(var), tail) and \
           re.search(r"\*\(float \*\)\(%s \+ %s\) = " % (re.escape(base), re.escape(off)), tail):
            return "decompile: max via temp on *(float *)(%s + %s)" % (base, off)
    return None


def fact_three_midpoints(text, consts):
    """Three '(a + b) * 0.5' bounding-box midpoints.

    The decompiler prints the 0.5 either as a literal or as the read-only
    global it is loaded from (DAT_xxx), so accept three '(a + b) * <same
    multiplier>' terms as long as the function really loads 0.5f.
    """
    mults = re.findall(r"\+[^;\n]*\)\s*\*\s*(0\.5\b|DAT_[0-9a-f]+)", text)
    if not mults:
        return None
    best = max(set(mults), key=mults.count)
    n = mults.count(best)
    if n < 3:
        return None
    if best != "0.5" and "3f000000" not in consts:
        return None
    return "decompile: %d '(a + b) * %s' midpoint terms%s" % (
        n, best, "" if best == "0.5" else " (0.5f loaded by the function)")


def fact_consts(*hexes):
    """All of the given 32-bit constants are used by the function."""
    want = set(hexes)

    def check(text, consts):
        if want <= consts:
            return "constants %s used by the function" % ", ".join("0x" + h for h in sorted(want))
        return None
    return check


def fact_count(pattern, n):
    """Exactly n occurrences of a pattern (e.g. n constructions of one object)."""
    rx = re.compile(pattern)

    def check(text, consts):
        found = len(rx.findall(text))
        return "decompile: %d occurrences" % found if found == n else None
    return check


FACTS = {
    "FUN_101f6bf0": [
        ("usage string (location,angle,width,move_fast)", fact_text(r"move_fast")),
        ("angle * 182.04443", fact_float(182.04443, r"182\.04\d*")),
        ("width > 0 check", fact_text(r"(<=?\s*0\.0\b)|(\b0\.0\s*<)|(<=?\s*0\))|(\b0\.0\s*>=?)")),
    ],
    "FUN_1035ae20": [
        ('compare against "rocket"', fact_text(r'"rocket"')),
        ('compare against "torpedo"', fact_text(r'"torpedo"')),
        ("angle lower bound pi/10", fact_float(math.pi / 10.0, r"0\.3141\d*")),
        ("angle upper bound 0.9*pi", fact_float(0.9 * math.pi, r"2\.8274\d*")),
    ],
    "FUN_1041c5d0": [
        ("float max-tracker (if hw > obj+X then obj+X = hw)", fact_max_tracker),
        ("three bounding-box midpoint accumulations", fact_three_midpoints),
    ],
    # Facts below were chosen after reading both decompiles side by side.
    "FUN_10366e40": [
        ("best-distance search starts at FLT_MAX and samples with 0.1f", fact_consts("7f7fffff", "3dcccccd")),
        ("projection parameter clamped at 0 (0.0 <= t)", fact_text(r"0\.0 <= \w+\) goto")),
        ("keeps the larger progress when the best segment is the current one",
         fact_text(r"== \*\w+\) && \(\w+ < ")),
    ],
    "FUN_1042ea90": [
        ("three bounding-box midpoints", fact_three_midpoints),
        ("endless sampling loop (do ... while (true))", fact_text(r"while\( true \)")),
    ],
    "FUN_1037a850": [
        ("eight query-builder constructions", fact_count(r"FUN_10409e10\(|FUN_18055df20\(", 8)),
        ("constant 10.0f", fact_consts("41200000")),
    ],
    "FUN_10399f80": [
        ("null check then straight xyz copy", fact_text(r"\*param_2 = \*param_1;")),
        ("copies the third component as well", fact_text(r"param_2\[2\] = ")),
    ],
    "FUN_103b24a0": [
        ("takes x and z from the position copier result",
         fact_text(r"local_\w+ = puVar2\[2\];\s*local_\w+ = \*puVar2;")),
    ],
    "FUN_10369940": [
        ("returns a field of the lookup result, else a default from this",
         fact_text(r"!= 0\) \{\s*return \*\([^;]*\+ 0x[0-9a-f]+\);\s*\}\s*return \*\(")),
    ],
    # Phase 7 Task 2 facts (docs/PHASE7_capture_plan.md, items a-f).
    "FUN_101a9750": [
        ("segment search called once per entity, last argument 0",
         fact_text(r"FUN_(10366e40|18049b660)\([^;]*,0\);")),
        ("new cursor kept only if the compare function says so",
         fact_text(r"FUN_(10391600|1804cc720)\([^;]*\);\s*if \(cVar\d+ != '\\0'\) \{\s*local_\w+ = local_\w+;")),
        ("first-run flag: set to 1 and pass -1",
         fact_text(r"= 0xffffffff;\s*\*\(undefined1 \*\)\(param_1 \+ 0x[0-9a-f]+\) = 1;|"
                   r"\*\(undefined1 \*\)\(param_1 \+ 0x[0-9a-f]+\) = 1;\s*\w+ = 0xffffffff;")),
        ("temporary object freed with a fixed size at the end", fact_text(r",0x(3c|58)\);\s*\}\s*return;")),
    ],
    "FUN_10179d20": [
        ("loops over the entity array calling the segment search with last argument 0",
         fact_text(r"do \{\s*FUN_(10366e40|18049b660)\([^;]*,0\);\s*\w+ = \w+ \+ 1;")),
        ("returns its second parameter", fact_text(r"return param_2;")),
    ],
    "FUN_1036e710": [
        ("3x3 rotate plus translate using param_1[7..0xf]", fact_text(r"param_1\[0xf\]")),
        ("writes three outputs", fact_text(r"param_2\[2\] = ")),
    ],
}

# Human names for the functions we care about, for the map table.
KNOWN = OrderedDict([
    ("FUN_101f6bf0", "move command parser"),
    ("FUN_102557e0", "move order constructor"),
    ("FUN_1010bb50", "unit command processor"),
    ("FUN_1035ae20", "unit setup (once per command)"),
    ("FUN_1041c5d0", "path object registration (max half-width)"),
    ("FUN_1037a850", "collision setup (once per command)"),
    ("FUN_1042ea90", "center-line path sampler"),
    ("FUN_101b7b70", "per-entity tick (vtable)"),
    ("FUN_10366e40", "path segment search"),
    ("FUN_10369940", "position utility"),
    ("FUN_103b24a0", "position wrapper"),
    ("FUN_10399f80", "position copier into temp query buffer"),
    ("FUN_1036e710", "transform helper (non-null branch of the copier)"),
    ("FUN_10397c30", "sample builder called by the segment search"),
    ("FUN_101a9750", "path chain B (caller of the segment search)"),
    ("FUN_10179d20", "third caller of the segment search"),
    ("FUN_1040d500", "path object constructor A (standard)"),
    ("FUN_1040d610", "path object constructor B (rocket)"),
    ("FUN_1040d450", "path object constructor C (oblique)"),
    ("FUN_1009f4e0", "config param registration"),
    ("FUN_10353dd0", "naval path builder (exclude)"),
])


def load_anchor_csv(path):
    """anchor -> OrderedDict(func_entry -> row). Rows without a function are dropped."""
    per_anchor = defaultdict(OrderedDict)
    with open(path, newline="") as fh:
        for row in csv.DictReader(fh):
            if not row["func_entry"]:
                continue
            per_anchor[row["anchor"]].setdefault(row["func_entry"], row)
    return per_anchor


def as_set(cell, sep):
    return {x.strip() for x in cell.split(sep) if x.strip()}


def ratio_score(a, b):
    """1.0 for equal sizes, falling to 0 at a 4x difference."""
    a, b = float(a) + 1.0, float(b) + 1.0
    return max(0.0, 1.0 - abs(math.log(b / a, 2)) / 2.0)


def jaccard(a, b):
    if not a and not b:
        return None
    return len(a & b) / float(len(a | b))


def score_pair(r86, r64):
    parts = OrderedDict()
    parts["size"] = (ratio_score(r86["body_bytes"], r64["body_bytes"]), 0.25)
    parts["callees"] = (ratio_score(r86["n_callees"], r64["n_callees"]), 0.25)
    c86, c64 = as_set(r86["consts"], ";"), as_set(r64["consts"], ";")
    jc = jaccard(c86, c64)
    if jc is not None:
        parts["consts"] = (jc, 0.30)
    s86, s64 = as_set(r86["strings"], " | "), as_set(r64["strings"], " | ")
    js = jaccard(s86, s64)
    if js is not None:
        parts["strings"] = (js, 0.20)
    wsum = sum(w for _, w in parts.values())
    total = sum(v * w for v, w in parts.values()) / wsum
    detail = "size x%.2f (%s -> %s bytes); callees %s vs %s" % (
        (float(r64["body_bytes"]) + 1) / (float(r86["body_bytes"]) + 1),
        r86["body_bytes"], r64["body_bytes"], r86["n_callees"], r64["n_callees"])
    if jc is not None:
        detail += "; %d shared constants of %d/%d" % (len(c86 & c64), len(c86), len(c64))
    if js is not None:
        detail += "; %d shared strings" % len(s86 & s64)
    return total, detail


def find_decompile(dirs, names):
    for d in dirs:
        for n in names:
            p = os.path.join(d, n + ".c")
            if os.path.isfile(p):
                with open(p, encoding="utf-8", errors="replace") as fh:
                    return p, fh.read()
    return None, None


def check_facts(x86_name, text86, consts86, text64, consts64):
    """Return (all_agree, list of evidence lines)."""
    facts = FACTS.get(x86_name)
    if not facts:
        return False, []
    out, all_ok = [], True
    if text86 is None or text64 is None:
        missing = "x86" if text86 is None else "x64"
        return False, ["fact check skipped: no %s decompile on disk" % missing]
    for label, checker in facts:
        e86 = checker(text86, consts86)
        e64 = checker(text64, consts64)
        if e86 and e64:
            out.append("FACT OK %s [x86 %s] [x64 %s]" % (label, e86, e64))
        else:
            all_ok = False
            side = "neither side" if not e86 and not e64 else ("x86 only" if e86 else "x64 only")
            out.append("FACT MISSING %s (%s)" % (label, side))
    return all_ok, out


def safe_write(target, text):
    target = os.path.abspath(target)
    os.makedirs(os.path.dirname(target), exist_ok=True)
    if not os.path.exists(target):
        with open(target, "w", encoding="utf-8") as fh:
            fh.write(text)
        return "created %s" % target
    tmp = target + "_tmp"
    with open(tmp, "w", encoding="utf-8") as fh:
        fh.write(text)
    bak, n = target + ".bak", 1
    while os.path.exists(bak):
        n += 1
        bak = "%s.bak%d" % (target, n)
    os.rename(target, bak)
    os.rename(tmp, target)
    return "replaced %s (previous version kept as %s)" % (target, bak)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--x86", default="docs/anchors_x86.csv")
    ap.add_argument("--x64", default="docs/anchors_x64.csv")
    ap.add_argument("--x86-decomp", action="append", default=[])
    ap.add_argument("--x64-decomp", action="append", default=[])
    ap.add_argument("--pairs", default=None,
                    help="CSV of call-graph-walk pairs: x86_name,x86_entry,x64_name,x64_entry,score,evidence")
    ap.add_argument("--max-fanout", type=int, default=16,
                    help="skip anchors referenced by more functions than this on either side")
    ap.add_argument("--min-score", type=float, default=0.20,
                    help="anchor pairs scoring below this are listed as rejected, not mapped")
    ap.add_argument("-o", "--output", default="docs/function_map_v1.md")
    args = ap.parse_args()

    a86 = load_anchor_csv(args.x86)
    a64 = load_anchor_csv(args.x64)

    # Every (x86 func, x64 func) pair that shares at least one anchor.
    pair_anchors = defaultdict(list)
    rows86, rows64 = {}, {}
    skipped = []
    anchor_order = []
    for anchor in a86:
        if anchor not in anchor_order:
            anchor_order.append(anchor)
    for anchor in a64:
        if anchor not in anchor_order:
            anchor_order.append(anchor)
    for anchor in anchor_order:
        f86, f64 = a86.get(anchor, {}), a64.get(anchor, {})
        if not f86 or not f64:
            skipped.append("%s: %d x86 functions, %d x64 functions (nothing to pair)" % (
                anchor, len(f86), len(f64)))
            continue
        if len(f86) > args.max_fanout or len(f64) > args.max_fanout:
            skipped.append("%s: %d x86 functions, %d x64 functions (too many referencing "
                           "functions to pair by anchor; not discriminating)" % (anchor, len(f86), len(f64)))
            continue
        for e86, r86 in f86.items():
            rows86[e86] = r86
            for e64, r64 in f64.items():
                rows64[e64] = r64
                unique = len(f86) == 1 and len(f64) == 1
                pair_anchors[(e86, e64)].append((anchor, unique))

    scored = []
    for (e86, e64), anchors in pair_anchors.items():
        base, detail = score_pair(rows86[e86], rows64[e64])
        bonus = 0.10 * (len(anchors) - 1) + (0.15 if any(u for _, u in anchors) else 0.0)
        scored.append((min(1.0, base + bonus), e86, e64, anchors, detail))
    scored.sort(key=lambda t: -t[0])

    # Greedy one-to-one assignment, best score first.
    used86, used64, chosen, runners_up = set(), set(), [], defaultdict(list)
    for item in scored:
        score, e86, e64, anchors, detail = item
        if score < args.min_score or e86 in used86 or e64 in used64:
            runners_up[e86].append(item)
            continue
        used86.add(e86)
        used64.add(e64)
        chosen.append(item)

    results = []   # dicts for the table
    for score, e86, e64, anchors, detail in chosen:
        r86, r64 = rows86[e86], rows64[e64]
        evidence = ["anchor " + ", ".join(
            "%s%s" % (a, " (only function on both sides)" if u else "") for a, u in anchors), detail]
        results.append({"x86_name": r86["func_name"], "x86_entry": e86, "x64_name": r64["func_name"],
                        "x64_entry": e64, "score": score, "evidence": evidence, "source": "anchor",
                        "consts86": as_set(r86["consts"], ";"), "consts64": as_set(r64["consts"], ";")})

    if args.pairs and os.path.isfile(args.pairs):
        have = {(r["x86_entry"].lower(), r["x64_entry"].lower()) for r in results}
        by86 = {r["x86_entry"].lower(): r for r in results}
        with open(args.pairs, newline="") as fh:
            for row in csv.DictReader(fh):
                key = (row["x86_entry"].lower(), row["x64_entry"].lower())
                if key in have:
                    for r in results:
                        if (r["x86_entry"].lower(), r["x64_entry"].lower()) == key:
                            r["evidence"].append("call-graph walk agrees: " + row["evidence"])
                    continue
                if key[0] in by86:
                    by86[key[0]]["evidence"].append(
                        "CONFLICT: call-graph walk proposes x64 %s instead (%s)" % (row["x64_entry"], row["evidence"]))
                    continue
                results.append({"x86_name": row["x86_name"], "x86_entry": row["x86_entry"],
                                "x64_name": row["x64_name"], "x64_entry": row["x64_entry"],
                                "score": float(row["score"] or 0), "evidence": [row["evidence"]],
                                "source": "walk",
                                "consts86": as_set(row.get("consts86", ""), ";"),
                                "consts64": as_set(row.get("consts64", ""), ";")})

    for r in results:
        _, t86 = find_decompile(args.x86_decomp, [r["x86_name"]])
        _, t64 = find_decompile(args.x64_decomp, [r["x64_name"], r["x86_name"]])
        ok, facts = check_facts(r["x86_name"], t86, r["consts86"], t64, r["consts64"])
        r["confidence"] = "CONFIRMED" if ok else "CANDIDATE"
        r["facts"] = facts

    def sort_key(r):
        name = r["x86_name"].lower()
        known = [k.lower() for k in KNOWN]
        return (0 if name in known else 1, known.index(name) if name in known else 0,
                0 if r["confidence"] == "CONFIRMED" else 1, -r["score"])
    results.sort(key=sort_key)

    L = ["# x86 -> x64 function map v1", "",
         "Generated by tools/compare_anchors.py from %s and %s." % (args.x86, args.x64), "",
         "x86 = original `empire.retail.dll` (image base 0x10000000). "
         "x64 = Complete Edition `empire.retail.x64.dll` (image base 0x180000000).", "",
         "CONFIRMED means both decompiles agree on a source-level fact (listed per row). "
         "CANDIDATE means the pair is only supported by anchors, call-graph position and size / "
         "call-count / constant similarity; treat it as a lead, not a fact.", "",
         "| x86 name | Role | x86 RVA | x64 address | x64 RVA | Confidence | Score | Evidence |",
         "|----------|------|---------|-------------|---------|------------|-------|----------|"]
    for r in results:
        e86 = int(r["x86_entry"], 16)
        e64 = int(r["x64_entry"], 16)
        role = KNOWN.get(r["x86_name"], KNOWN.get(r["x86_name"].lower(), ""))
        if not role:
            for k, v in KNOWN.items():
                if k.lower() == r["x86_name"].lower():
                    role = v
        ev = "; ".join(r["evidence"] + r["facts"]).replace("|", "/")
        L.append("| %s | %s | 0x%06X | %X | 0x%07X | %s | %.2f | %s |" % (
            r["x86_name"], role, e86 - X86_BASE, e64, e64 - 0x180000000, r["confidence"], r["score"], ev))
    L.append("")
    n_conf = sum(1 for r in results if r["confidence"] == "CONFIRMED")
    L.append("%d pairs: %d CONFIRMED, %d CANDIDATE." % (len(results), n_conf, len(results) - n_conf))
    L.append("")

    missing = [k for k in KNOWN if k.lower() not in {r["x86_name"].lower() for r in results}]
    if missing:
        L.append("## Known x86 functions with no x64 counterpart yet")
        L.append("")
        for k in missing:
            L.append("- %s (%s)" % (k, KNOWN[k]))
        L.append("")
    if runners_up:
        L.append("## Rejected alternatives (lower score, same anchor)")
        L.append("")
        for e86, items in runners_up.items():
            for score, _e86, e64, anchors, detail in items[:3]:
                L.append("- %s -> x64 %s, score %.2f, %s" % (rows86[e86]["func_name"], e64, score, detail))
        L.append("")
    if skipped:
        L.append("## Anchors not used for pairing")
        L.append("")
        for s in skipped:
            L.append("- " + s)
        L.append("")

    print(safe_write(args.output, "\n".join(L) + "\n"))
    for r in results:
        print("%-10s %-14s -> %s  score %.2f" % (r["confidence"], r["x86_name"], r["x64_entry"], r["score"]))
        for f in r["facts"]:
            print("             " + f[:150])
    return 0


if __name__ == "__main__":
    sys.exit(main())
