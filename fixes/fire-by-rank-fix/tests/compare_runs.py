"""Compare fbrfix probe runs (fix off / on): fire-by-rank units' first shot, volleys, waits.

python -I fixes/fire-by-rank-fix/tests/compare_runs.py <run folder> [<run folder> ...] [--range M]

Reads fbr_states.csv, fbr_reload.csv, fbr_orders.csv and fbr_units.csv of each run folder written
with FBRFIX_PROBE=1 (fbr_fix.csv too when present). Per unit that ran the group fire task
(Shogun 2's fire by rank, owner vtable RVA 0x15C1048):
  inrange   first tick a target soldier is within --range m (default 100, the matchlock's
            effective range) of any soldier of the unit (targets = units in its fire orders)
  first     first shot (a soldier's reload time leaving 0), and first - inrange
  volleys   ORDER_GROUP_FIRE_STATE entries
  wait1     the first WAIT_FOR_GROUP_ALL_LOADED_AND_READY_STATE (seconds)
  maxwait   the longest such wait that ended
  hang      a wait still open at the end of the run (seconds), the clan bug's signature
  shots     shots of the unit until the end of the run
  idle      median time from a man's shot to his reload starting to count down (fbr_reload.csv
            'counting', written by probes since part 2); idle90 its 90th percentile
  cycle     median time between two shots of the same man
  wrapleft  largest reload time left in group 0 when the rotation came back to it (fbr_rowdump.csv)
  noshot    group turns (ORDER_GROUP_FIRE .. ADVANCE_GROUP) without a single shot
  early     early reload orders, cut = turns ended by the fire grace (fbr_fix.csv)
Fire-at-will units (no group task) get inrange / first / shots / cycle for reference.
"""
import csv
import os
import sys
from collections import defaultdict

args = [a for a in sys.argv[1:] if not a.startswith('--')]
RANGE = float(sys.argv[sys.argv.index('--range') + 1]) if '--range' in sys.argv else 100.0
WAIT = 'WAIT_FOR_GROUP_ALL_LOADED_AND_READY_STATE'


def rows(run, name):
    p = os.path.join(run, name)
    return list(csv.DictReader(open(p, encoding='utf-8'))) if os.path.exists(p) else []


MIN_RELOAD = 150   # ticks: no matchlock reloads faster than 15 s (19.8 s and up measured)


def real_shots(rl, u):
    """Shot ticks per soldier of unit u. Probes before part 2 tracked reload times per slot of the
    soldier array, which casualties shift, so a man could show a 'fired' moments after his last
    shot; a shot counts only MIN_RELOAD ticks after the man's previous counted one."""
    out = defaultdict(list)
    for r in sorted((r for r in rl if r['unit'] == u and r['event'] == 'fired'), key=lambda r: int(r['tick'])):
        t = int(r['tick'])
        prev = out[r['soldier']]
        if not prev or t - prev[-1] >= MIN_RELOAD:
            prev.append(t)
    return out


def analyse(run):
    st = rows(run, 'fbr_states.csv')
    rl = rows(run, 'fbr_reload.csv')
    od = rows(run, 'fbr_orders.csv')
    un = rows(run, 'fbr_units.csv')
    fx = rows(run, 'fbr_fix.csv')
    dump = rows(run, 'fbr_rowdump.csv')
    T = lambda r: int(r['tick'])
    end = max([T(r) for r in un] + [0])
    pos = defaultdict(lambda: defaultdict(list))
    for r in un:
        pos[r['unit']][T(r)].append((float(r['x']), float(r['z'])))
    units = sorted({r['unit'] for r in st} - {'00000000'})
    out = []
    for u in units:
        tus = {r['target_unit'] for r in od if r['unit'] == u and r['target_unit'] not in ('', '00000000')}
        shots = sorted(t for es in real_shots(rl, u).values() for t in es)
        inrange = None
        for t in sorted(pos[u]):
            best = 1e18
            for tu in tus:
                cand = [tt for tt in pos[tu] if abs(tt - t) <= 10]
                if not cand:
                    continue
                tt = min(cand, key=lambda x: abs(x - t))
                for (x, z) in pos[tu][tt]:
                    for (mx, mz) in pos[u][t]:
                        best = min(best, (x - mx) ** 2 + (z - mz) ** 2)
            if best <= RANGE * RANGE:
                inrange = t
                break
        g = [r for r in st if r['unit'] == u and r['owner_vt'] == '15C1048']
        rec = {'unit': u, 'group': bool(g), 'inrange': inrange, 'first': shots[0] if shots else None,
               'shots': len(shots), 'volleys': sum(1 for r in g if r['new'] == 'ORDER_GROUP_FIRE_STATE')}
        waits, opened = [], None
        for r in sorted(g, key=T):
            if r['new'] == WAIT:
                opened = T(r)
            elif r['old'] == WAIT and opened is not None:
                waits.append(T(r) - opened)
                opened = None
        rec['wait1'] = waits[0] / 10.0 if waits else None
        rec['maxwait'] = max(waits) / 10.0 if waits else None
        rec['hang'] = (end - opened) / 10.0 if opened is not None else 0.0
        # per-man reload timing
        evs = defaultdict(list)
        for r in rl:
            if r['unit'] == u:
                evs[r['soldier']].append((T(r), r['event']))
        idle, cycle = [], []
        real = real_shots(rl, u)
        for sol, es in evs.items():
            es.sort()
            fired = real.get(sol, [])
            es = [(t, e) for t, e in es if e != 'fired' or t in fired]
            cycle += [b - a for a, b in zip(fired, fired[1:])]
            for k, (t, e) in enumerate(es):
                if e == 'fired':
                    nxt = [t2 for t2, e2 in es[k + 1:] if e2 in ('counting', 'fired')]
                    if nxt and [e2 for t2, e2 in es[k + 1:] if t2 == nxt[0]][0] == 'counting':
                        idle.append(nxt[0] - t)
        med = lambda xs: sorted(xs)[len(xs) // 2] / 10.0 if xs else None
        rec['idle'], rec['cycle'] = med(idle), med(cycle)
        rec['idle90'] = sorted(idle)[int(len(idle) * 0.9)] / 10.0 if idle else None
        # reload left in group 0 at each wrap
        ctxs = {r['ctx'] for r in g}
        tasks = {'%08X' % (int(c, 16) - 0x18) for c in ctxs}
        left, seen0 = [], set()
        dumps = defaultdict(list)
        for r in dump:
            if r['task'] in tasks and r['why'] == WAIT and r['row'] == '0':
                dumps[(r['task'], T(r))].append(r)
        for (task, t), rs in sorted(dumps.items()):
            if task in seen0:
                left.append(max([float(r['reload']) for r in rs if r['trow'] == '0'] + [0.0]))
            seen0.add(task)
        rec['wrapleft'] = max(left) if left else None
        # turns without a shot
        turns, opened = [], None
        for r in sorted(g, key=T):
            if r['new'] == 'ORDER_GROUP_FIRE_STATE':
                opened = T(r)
            elif r['new'] == 'ADVANCE_GROUP_STATE' and opened is not None:
                turns.append((opened, T(r)))
                opened = None
        rec['noshot'] = sum(1 for a, b in turns if not any(a <= x <= b for x in shots))
        rec['early'] = sum(1 for r in fx if r['unit'] == u and r['answer'] == 'early')
        rec['cut'] = sum(1 for r in fx if r['unit'] == u and r['answer'] == 'cut')
        rec['grace'] = sum(1 for r in fx if r['unit'] == u and r['answer'] == 'grace')
        rec['skipped'] = sum(1 for r in fx if r['unit'] == u and r['answer'] == 'order_skipped')
        out.append(rec)
    return end, out


def f(v, fmt='%.1f'):
    return '-' if v is None else fmt % v


for run in args:
    end, recs = analyse(run)
    print('%s  (end %.1f s)' % (run, end / 10.0))
    print('  %-9s %-5s %8s %8s %7s %7s %6s %7s %7s %6s %5s %5s %5s %6s %5s %8s %6s %5s %4s' %
          ('unit', 'mode', 'inrange', 'first', 'delay', 'volleys', 'wait1', 'maxwait', 'hang', 'shots', 'grace', 'skip',
           'idle', 'idle90', 'cycle', 'wrapleft', 'noshot', 'early', 'cut'))
    for r in recs:
        if r['inrange'] is None and not r['shots']:
            continue
        delay = (r['first'] - r['inrange']) / 10.0 if r['first'] is not None and r['inrange'] is not None else None
        print('  %-9s %-5s %8s %8s %7s %7d %6s %7s %7.1f %6d %5d %5d %5s %6s %5s %8s %6d %5d %4d' %
              (r['unit'], 'FBR' if r['group'] else 'FAW', f(r['inrange'] and r['inrange'] / 10.0),
               f(r['first'] and r['first'] / 10.0), f(delay), r['volleys'], f(r['wait1']), f(r['maxwait']),
               r['hang'], r['shots'], r['grace'], r['skipped'], f(r['idle']), f(r['idle90']), f(r['cycle']), f(r['wrapleft']),
               r['noshot'], r['early'], r['cut']))
