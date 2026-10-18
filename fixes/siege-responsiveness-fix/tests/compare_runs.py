"""Compare siegefix probe runs (<game>\\siegefix_runs\\<stamp>\\) side by side.

    python compare_runs.py <run> [<run> ...] [--from <game s>]

Per run, from `--from` game seconds on (default 160, the start of the wall assault in the
"wall siege bug" replay):
  - the local player's requests: queued, made immediate by the fix, served, served %, wait in
    ticks (p50 / p95 / max);
  - the other side's requests: queued, served, served %, wait;
  - the pathfinder per tick: queue length (mean / max), requests served, ticks that hit the node
    budget with requests left, wall time of the whole update and of the queue (p50 / p99 / max,
    microseconds).
A run with the fix on drifts from the recording (path timing changes), so compare these per-request
and per-tick figures, not the battle's outcome.
"""
import csv
import os
import sys


def pct(v, p):
    if not v:
        return '-'
    v = sorted(v)
    return v[min(len(v) - 1, int(round(p / 100.0 * (len(v) - 1))))]


def load(run, t_from):
    ticks = [r for r in csv.DictReader(open(os.path.join(run, 'siege_ticks.csv'), newline=''))
             if int(r['tick']) >= t_from]
    req = {'player': {'enq': 0, 'synced': 0, 'served': []}, 'other': {'enq': 0, 'synced': 0, 'served': []}}
    for r in csv.DictReader(open(os.path.join(run, 'siege_requests.csv'), newline='')):
        t = r['tick_q'] or r['tick_served']
        if not t or int(t) < t_from:
            continue
        side = 'player' if r['player'] == '1' else 'other'
        if r['kind'] == 'enq':
            req[side]['enq'] += 1
        elif r['kind'] == 'synced':
            req[side]['synced'] += 1
        elif r['kind'] == 'queued' and r['wait_ticks']:
            req[side]['served'].append(int(r['wait_ticks']))
    return ticks, req


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    t_from = 1600
    if '--from' in sys.argv:
        t_from = int(float(sys.argv[sys.argv.index('--from') + 1]) * 10)
        args = [a for a in args if a != sys.argv[sys.argv.index('--from') + 1]]
    for run in args:
        ticks, req = load(run, t_from)
        print('%s  (from %d game s, %d ticks)' % (os.path.basename(os.path.normpath(run)), t_from // 10, len(ticks)))
        for side in ('player', 'other'):
            d = req[side]
            s = d['served']
            print('  %-6s queued %6d  immediate %5d  served %6d (%5.1f %%)  wait p50/p95/max %s/%s/%s ticks' % (
                side, d['enq'], d['synced'], len(s), 100.0 * len(s) / d['enq'] if d['enq'] else 0.0,
                pct(s, 50), pct(s, 95), max(s) if s else '-'))
        if ticks:
            q = [int(t['q_start']) for t in ticks]
            sv = [int(t['served']) for t in ticks]
            hit = sum(int(t['budget_hit']) for t in ticks)
            ut = [int(t['us_total']) for t in ticks]
            ud = [int(t['us_drain']) for t in ticks]
            print('  queue mean %.0f max %d | served/tick %.1f | budget hit %.0f %% | update us %s/%s/%s | queue us %s/%s/%s' % (
                sum(q) / len(q), max(q), sum(sv) / len(sv), 100.0 * hit / len(ticks), pct(ut, 50), pct(ut, 99), max(ut),
                pct(ud, 50), pct(ud, 99), max(ud)))


if __name__ == '__main__':
    main()
