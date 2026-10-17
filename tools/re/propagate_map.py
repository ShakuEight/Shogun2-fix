#!/usr/bin/env python3
"""
propagate_map.py - walk the call graph out from confirmed x86 <-> x64 pins.

Offline. Reads the per-function dumps written by tools/dump_callgraph.py for
both programs, starts from seed pairs (the pins compare_anchors.py confirmed)
and repeatedly:

  * aligns the ordered call sequence of each matched x86 function against the
    call sequence of its x64 partner (Needleman-Wunsch, scored by function
    similarity) and proposes the aligned callees as new pairs;
  * proposes callers when the fan-in is unambiguous.

A proposal is accepted only when it is the mutual best for both functions.
Every accepted pair records which parent pair and call position produced it,
so the chain of evidence back to a pin is printed for each target.

Usage:
    python tools/propagate_map.py
        --x86 data/callgraph_x86.csv --x64 data/callgraph_x64.csv
        --seed 101f6bf0=180xxxxxx --seed 1035ae20=180yyyyyy
        --target 1037a850 --target 1042ea90 ...
        --out-targets docs/walk_pairs.csv --out-all docs/walk_pairs_all.csv
"""
import argparse
import csv
import math
import os
import sys
from collections import defaultdict

csv.field_size_limit(1 << 30)


class Func(object):
    __slots__ = ("entry", "name", "thunk_of", "body", "n_instr", "n_callers", "n_ptr",
                 "seq", "consts", "strings", "n_indirect")


def load(path):
    funcs = {}
    with open(path, newline="") as fh:
        for row in csv.DictReader(fh):
            f = Func()
            f.entry = int(row["entry"], 16)
            f.name = row["name"]
            f.thunk_of = int(row["thunk_of"], 16) if row["thunk_of"] else None
            f.body = int(row["body_bytes"])
            f.n_instr = int(row["n_instr"])
            f.n_callers = int(row["n_callers"])
            f.n_ptr = int(row["n_ptr_refs"])
            seq = []
            for tok in row["call_seq"].split(";"):
                if not tok:
                    continue
                if tok == "?":
                    seq.append(None)
                else:
                    seq.append(int(tok[2:] if tok.startswith("j:") else tok, 16))
            f.seq = seq
            f.consts = frozenset(x for x in row["consts"].split(";") if x)
            f.strings = frozenset(x.strip() for x in row["strings"].split(" | ") if x.strip())
            funcs[f.entry] = f
    # Resolve thunks so that a call to a thunk counts as a call to its target.
    def resolve(e):
        seen = 0
        while e in funcs and funcs[e].thunk_of is not None and seen < 8:
            e = funcs[e].thunk_of
            seen += 1
        return e
    callers = defaultdict(set)
    for f in funcs.values():
        f.seq = [resolve(t) if t is not None else None for t in f.seq]
        f.n_indirect = sum(1 for t in f.seq if t is None)
        for t in f.seq:
            if t is not None:
                callers[t].add(f.entry)
    return funcs, callers


def ratio_score(a, b):
    return max(0.0, 1.0 - abs(math.log((b + 1.0) / (a + 1.0), 2)) / 2.0)


_sim_cache = {}


def similarity(f86, f64):
    key = (f86.entry, f64.entry)
    v = _sim_cache.get(key)
    if v is not None:
        return v
    parts = [(ratio_score(f86.body, f64.body), 0.25),
             (ratio_score(len(f86.seq), len(f64.seq)), 0.25),
             (ratio_score(f86.n_indirect, f64.n_indirect), 0.10)]
    if f86.consts or f64.consts:
        parts.append((len(f86.consts & f64.consts) / float(len(f86.consts | f64.consts)), 0.25))
    if f86.strings or f64.strings:
        parts.append((len(f86.strings & f64.strings) / float(len(f86.strings | f64.strings)), 0.15))
    v = sum(s * w for s, w in parts) / sum(w for _, w in parts)
    _sim_cache[key] = v
    return v


GAP = -0.15


def align(seq86, seq64, F86, F64, m86, m64):
    """Global alignment of two call sequences. Returns [(i, j)] aligned index pairs."""
    n, m = len(seq86), len(seq64)
    if n == 0 or m == 0 or n * m > 4000000:
        return []

    def cell(a, b):
        if a is None and b is None:
            return 0.6
        if a is None or b is None:
            return -1.0
        if a in m86:
            return 1.5 if m86[a] == b else -1.0
        if b in m64:
            return -1.0
        fa, fb = F86.get(a), F64.get(b)
        if fa is None or fb is None:
            return -1.0
        return similarity(fa, fb) - 0.35

    score = [[0.0] * (m + 1) for _ in range(n + 1)]
    move = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        score[i][0] = i * GAP
        move[i][0] = 1
    for j in range(1, m + 1):
        score[0][j] = j * GAP
        move[0][j] = 2
    for i in range(1, n + 1):
        a = seq86[i - 1]
        row, prev = score[i], score[i - 1]
        for j in range(1, m + 1):
            d = prev[j - 1] + cell(a, seq64[j - 1])
            u = prev[j] + GAP
            l = row[j - 1] + GAP
            if d >= u and d >= l:
                row[j] = d
                move[i][j] = 0
            elif u >= l:
                row[j] = u
                move[i][j] = 1
            else:
                row[j] = l
                move[i][j] = 2
    out = []
    i, j = n, m
    while i > 0 or j > 0:
        mv = move[i][j]
        if mv == 0:
            out.append((i - 1, j - 1))
            i -= 1
            j -= 1
        elif mv == 1:
            i -= 1
        else:
            j -= 1
    out.reverse()
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--x86", default="data/callgraph_x86.csv")
    ap.add_argument("--x64", default="data/callgraph_x64.csv")
    ap.add_argument("--seed", action="append", default=[], help="x86entry=x64entry (hex)")
    ap.add_argument("--manual", action="append", default=[],
                    help="x86entry=x64entry (hex): pair fixed by hand after reading both decompiles; "
                         "it is never reassigned and is reported as such")
    ap.add_argument("--target", action="append", default=[], help="x86 entry (hex) to report on")
    ap.add_argument("--rounds", type=int, default=6)
    ap.add_argument("--min-sim", type=float, default=0.45)
    ap.add_argument("--out-targets", default=None)
    ap.add_argument("--out-all", default=None)
    args = ap.parse_args()

    F86, C86 = load(args.x86)
    F64, C64 = load(args.x64)
    print("loaded %d x86 functions, %d x64 functions" % (len(F86), len(F64)))

    m86, m64 = {}, {}
    info = {}   # x86 entry -> (round, sim, evidence text, parent x86 entry or None)
    for s in args.seed:
        a, b = s.split("=")
        a, b = int(a, 16), int(b, 16)
        if a not in F86 or b not in F64:
            print("ERROR: seed %s not present in the dumps" % s)
            return 2
        m86[a], m64[b] = b, a
        info[a] = (0, similarity(F86[a], F64[b]), "seed (confirmed pin)", None)

    for s in args.manual:
        a, b = s.split("=")
        a, b = int(a, 16), int(b, 16)
        if a not in F86 or b not in F64:
            print("ERROR: manual pair %s not present in the dumps" % s)
            return 2
        m86[a], m64[b] = b, a
        info[a] = (-1, similarity(F86[a], F64[b]),
                   "paired by hand after reading both decompiles (not a walk result)", None)

    frontier = list(m86.keys())
    for rnd in range(1, args.rounds + 1):
        votes = defaultdict(float)
        why = {}
        for a in frontier:
            b = m86[a]
            fa, fb = F86[a], F64[b]
            # Downward: aligned callees.
            for (i, j) in align(fa.seq, fb.seq, F86, F64, m86, m64):
                x, y = fa.seq[i], fb.seq[j]
                if x is None or y is None or x in m86 or y in m64:
                    continue
                if x not in F86 or y not in F64:
                    continue
                s = similarity(F86[x], F64[y])
                if s < args.min_sim:
                    continue
                votes[(x, y)] += s
                why.setdefault((x, y), (a, "call #%d of %d in %s <-> call #%d of %d in x64 %X" % (
                    i + 1, len(fa.seq), fa.name, j + 1, len(fb.seq), b)))
            # Upward: callers, only where unambiguous.
            ca = [c for c in C86.get(a, ()) if c not in m86]
            cb = [c for c in C64.get(b, ()) if c not in m64]
            if len(ca) == 1 and len(cb) == 1 and len(C86[a]) == 1 and len(C64[b]) == 1:
                s = similarity(F86[ca[0]], F64[cb[0]])
                if s >= 0.30:
                    votes[(ca[0], cb[0])] += s + 0.5
                    why.setdefault((ca[0], cb[0]), (a, "only caller of %s <-> only caller of x64 %X" % (fa.name, b)))
            elif 0 < len(ca) <= 40 and 0 < len(cb) <= 60:
                for x in ca:
                    ranked = sorted(((similarity(F86[x], F64[y]), y) for y in cb), reverse=True)
                    best = ranked[0]
                    margin = best[0] - (ranked[1][0] if len(ranked) > 1 else 0.0)
                    if best[0] >= 0.60 and margin >= 0.10:
                        votes[(x, best[1])] += best[0] * 0.7
                        why.setdefault((x, best[1]), (a, "caller of %s <-> caller of x64 %X (best of %d by %.2f)" % (
                            fa.name, b, len(cb), margin)))
        best86, best64 = {}, {}
        for (x, y), v in votes.items():
            if v > best86.get(x, (0, None))[0]:
                best86[x] = (v, y)
            if v > best64.get(y, (0, None))[0]:
                best64[y] = (v, x)
        frontier = []
        for x, (v, y) in best86.items():
            if best64[y][1] != x or x in m86 or y in m64:
                continue
            m86[x], m64[y] = y, x
            parent, text = why[(x, y)]
            info[x] = (rnd, similarity(F86[x], F64[y]), text, parent)
            frontier.append(x)
        print("round %d: %d new pairs (total %d)" % (rnd, len(frontier), len(m86)))
        if not frontier:
            break

    def chain(x):
        steps = []
        while x is not None:
            rnd, s, text, parent = info[x]
            steps.append("%s -> %X [similarity %.2f; %s]" % (F86[x].name, m86[x], s, text))
            x = parent
        return steps

    def row_for(x):
        y = m86[x]
        rnd, s, text, parent = info[x]
        hops = []
        p = parent
        while p is not None:
            hops.append(F86[p].name)
            p = info[p][3]
        lead = "call-graph walk, %d hop(s) from pin%s: " % (rnd, (" via " + " <- ".join(hops)) if hops else "")
        if rnd < 0:
            lead = ""
        evidence = "%s%s; size %d -> %d bytes; calls %d vs %d; %d shared constants" % (
            lead, text, F86[x].body, F64[y].body,
            len(F86[x].seq), len(F64[y].seq), len(F86[x].consts & F64[y].consts))
        return [F86[x].name, "%x" % x, F64[y].name, "%x" % y, "%.2f" % s, evidence,
                ";".join(sorted(F86[x].consts)), ";".join(sorted(F64[y].consts))]

    header = ["x86_name", "x86_entry", "x64_name", "x64_entry", "score", "evidence", "consts86", "consts64"]

    def write(path, entries):
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        tmp = path + "_tmp"
        with open(tmp, "w", newline="") as fh:
            w = csv.writer(fh)
            w.writerow(header)
            for x in entries:
                w.writerow(row_for(x))
        if os.path.exists(path):
            bak, n = path + ".bak", 1
            while os.path.exists(bak):
                n += 1
                bak = "%s.bak%d" % (path, n)
            os.rename(path, bak)
        os.rename(tmp, path)
        print("wrote %d pairs to %s" % (len(entries), path))

    print("")
    targets = [int(t, 16) for t in args.target]
    found = []
    for t in targets:
        name = F86[t].name if t in F86 else "FUN_%08x" % t
        if t not in F86:
            print("TARGET %s: not a function in the x86 dump" % name)
        elif t in m86:
            found.append(t)
            print("TARGET %s: matched" % name)
            for step in chain(t):
                print("    " + step)
        else:
            f = F86[t]
            print("TARGET %s: NOT matched (size %d, %d calls, %d callers; callers matched so far: %s)" % (
                name, f.body, len(f.seq), len(C86.get(t, ())),
                ", ".join(F86[c].name for c in C86.get(t, ()) if c in m86) or "none"))
    if args.out_targets:
        write(args.out_targets, [t for t in found if info[t][0] != 0])
    if args.out_all:
        write(args.out_all, sorted(m86.keys()))
    return 0


if __name__ == "__main__":
    sys.exit(main())
