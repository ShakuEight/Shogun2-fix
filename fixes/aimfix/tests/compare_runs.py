"""Compare aimfix probe runs: where each artillery shot was aimed relative to the target unit.

    python compare_runs.py <run_dir> [<run_dir> ...]

Reads aim_calls.csv (one row per shot, from the gun's fire routine) and aim_units.csv (soldier
positions once per game second per target unit) of each run folder written with AIMFIX_PROBE=1.
For every shot: distance of the aim point (target soldier) from the unit centroid, and its
position along the unit's frontage (0 = one end, 0.5 = middle, 1 = other end, from the principal
axis of the soldier positions). Also the shot count and the target units' strength over time.
"""
import csv
import os
import sys
from collections import defaultdict

import numpy as np


def load(run):
    calls = list(csv.DictReader(open(os.path.join(run, "aim_calls.csv"))))
    units = defaultdict(lambda: defaultdict(list))
    for r in csv.DictReader(open(os.path.join(run, "aim_units.csv"))):
        units[r["unit"]][int(r["tick"])].append((float(r["x"]), float(r["z"])))
    return calls, units


def frontage_pos(points, p):
    P = np.array(points)
    c = P.mean(0)
    Q = P - c
    w, v = np.linalg.eigh(np.cov(Q.T))
    lat = v[:, 1]
    pl = Q @ lat
    lo, hi = pl.min(), pl.max()
    x = (np.asarray(p) - c) @ lat
    frac = (x - lo) / (hi - lo) if hi > lo else 0.5
    return min(frac, 1 - frac) * 2, hi - lo   # 0 = an end, 1 = the middle


def summarize(run):
    calls, units = load(run)
    dists, mids, fronts = [], [], []
    for r in calls:
        if r["unit"] == "00000000":
            continue
        e = np.array([float(r["ent_x"]), float(r["ent_z"])])
        c = np.array([float(r["cen_x"]), float(r["cen_z"])])
        dists.append(float(np.hypot(*(e - c))))
        u, t = r["unit"], int(r["tick"])
        if units[u]:
            tt = min(units[u], key=lambda k: abs(k - t))
            m, f = frontage_pos(units[u][tt], e)
            mids.append(m)
            fronts.append(f)
    d = np.array(dists)
    m = np.array(mids)
    strength = {}
    for u, ticks in units.items():
        ts = sorted(ticks)
        strength[u] = (len(ticks[ts[0]]), len(ticks[ts[-1]]), ts[0], ts[-1])
    return {
        "shots": len(calls),
        "dist_median": float(np.median(d)), "dist_mean": float(d.mean()), "dist_max": float(d.max()),
        "within5": float((d <= 5).mean()),
        "mid_median": float(np.median(m)), "at_end": float((m < 0.1).mean()),
        "frontage": float(np.median(fronts)),
        "strength": strength,
    }


def main():
    for run in sys.argv[1:]:
        s = summarize(run)
        print("run %s" % os.path.basename(os.path.normpath(run)))
        print("  shots %d; aim point to unit centroid: median %.1f m, mean %.1f m, max %.1f m, within 5 m %.0f %%"
              % (s["shots"], s["dist_median"], s["dist_mean"], s["dist_max"], 100 * s["within5"]))
        print("  along the frontage (%.0f m wide; 0 = an end, 1 = the middle): median %.2f, shots at an end "
              "(< 0.1) %.0f %%" % (s["frontage"], s["mid_median"], 100 * s["at_end"]))
        for u, (n0, n1, t0, t1) in sorted(s["strength"].items()):
            print("  target unit %s: %d men at tick %d -> %d at tick %d (%d lost)" % (u, n0, t0, n1, t1, n0 - n1))


if __name__ == "__main__":
    main()
