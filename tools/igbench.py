#!/usr/bin/env python3
"""igbench.py -- seeded bot matches under a server config, measured (IG-5).

The instagib fork's measuring tool. Where botduel.py pits two botlibs against
each other on upstream's stock maps, igbench plays ONE botlib's bots under a
real server config (instagib.cfg by default) on any installed map, and reports
what they did: frags, rail accuracy, jump pads, hook use, time on upper levels,
distance and time to first contact. A feature is measured by running twice --
feature off, feature on (`--set name value`) -- and comparing the two runs.

    tools/igbench.py run -o /tmp/ig-base --maps q2dm1,q2rdm11,ztn2dm3 --seeds 10
    tools/igbench.py run -o /tmp/ig-hook --maps ... --set some_libvar 1
    tools/igbench.py report -o /tmp/ig-base                 # report.md, matches.csv
    tools/igbench.py report -o /tmp/ig-hook --base /tmp/ig-base   # with deltas

Each match: the config is exec'd, the map loads, N bots join with a fixed
skill, the warm-up runs, then `sv botstats reset` and `sv igstats reset` open
the window, which lasts --frames frames (100 ms each; 6000 = ten minutes).
botduel's machinery makes it fast and repeatable: `fixedtime` (a match runs
as fast as the host can compute it) and the time() shim (a seed decides the
whole match). The seed also draws the bots' characters.

THE TREE. Each worker gets a throwaway game tree that links the real install
($Q2: baseq2 paks, the colosseum gamedir's maps, configs and Q3 bot files) and
puts the library under test (LIB, default release/game<cpu>.so, with q3bot.so
beside it) in place of the installed one. The map's Q3 .aas is copied, never
linked. Nothing is written into the install.

DEMOS. `--demo` records a Q2PRO server-side demo (MVD) of each match's window
into <out>/demos/.

ENV: Q2 (/home/greed/quake-dev/quake2: q2proded, baseq2/, colosseum/),
LIB (release/game<cpu>.so), BOTDUEL_CACHE (the seed shim's cache).
"""
import argparse
import concurrent.futures
import csv
import json
import os
import random
import re
import shutil
import subprocess
import sys
import time

TOOLS = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, TOOLS)
import botduel  # noqa: E402  (shim, waits, roster, parse helpers)

ROOT = os.path.dirname(TOOLS)
FRAME_S = 0.1

RE_BOTSTATS = botduel.RE_CLIENT
RE_IG = re.compile(r'^igstats client (\d+) (.*) name (.*)$')
RE_IGFRAME = re.compile(r'^igstats frame (\d+) since (\d+) frames (\d+) upperz (-?\d+) (\w+)$')


class Paths:
    def __init__(self):
        e = os.environ.get
        self.q2 = os.path.abspath(e('Q2', '/home/greed/quake-dev/quake2'))
        self.q2proded = os.path.join(self.q2, 'q2proded')
        self.gamedir = os.path.join(self.q2, 'colosseum')
        self.lib = os.path.abspath(e('LIB', os.path.join(ROOT, 'release', 'game%s.so' % botduel.cpu())))
        self.q3lib = os.path.join(os.path.dirname(self.lib), 'q3bot.so')
        self.cache = os.path.abspath(e('BOTDUEL_CACHE', os.path.expanduser('~/.cache/colosseum-botduel')))

    def check(self):
        for p in (self.q2proded, self.lib, self.q3lib, os.path.join(self.q2, 'baseq2'),
                  os.path.join(self.gamedir, 'q3bot', 'bots.cfg')):
            if not os.path.exists(p):
                sys.exit('igbench: missing %s' % p)


def make_tree(paths, d, m, aasdir=None):
    """A worker's game tree: the install linked, the library under test copied in."""
    shutil.rmtree(d, ignore_errors=True)
    g = os.path.join(d, 'colosseum')
    os.makedirs(os.path.join(g, 'q3bot', 'maps'))
    os.symlink(os.path.join(paths.q2, 'baseq2'), os.path.join(d, 'baseq2'))
    skip = {'q3bot', 'secrets.cfg', os.path.basename(paths.lib), 'q3bot.so', 'gladiator.so'}
    for f in os.listdir(paths.gamedir):
        if f in skip or f.endswith('.jsonl') or f.endswith('.log'):
            continue
        os.symlink(os.path.join(paths.gamedir, f), os.path.join(g, f))
    shutil.copy(paths.lib, os.path.join(g, os.path.basename(paths.lib)))
    shutil.copy(paths.q3lib, os.path.join(g, 'q3bot.so'))
    q3 = os.path.join(paths.gamedir, 'q3bot')
    for f in ('botfiles', 'bots.cfg'):
        os.symlink(os.path.join(q3, f), os.path.join(g, 'q3bot', f))
    aas = os.path.join(aasdir or os.path.join(q3, 'maps'), m + '.aas')
    if not os.path.exists(aas):
        raise RuntimeError('no Q3 navigation file for %s (%s)' % (m, aas))
    shutil.copy(aas, os.path.join(g, 'q3bot', 'maps', m + '.aas'))
    # the file the fork keeps beside a map's .aas: its climb table (IG-36),
    # from the same folder as the .aas
    for ext in ('.climbs',):
        extra = os.path.join(os.path.dirname(aas), m + ext)
        if os.path.exists(extra):
            shutil.copy(extra, os.path.join(g, 'q3bot', 'maps', m + ext))
    open(os.path.join(g, 'secrets.cfg'), 'w').close()


def bench_cfg(args):
    lines = ['exec "%s"' % os.path.basename(args.cfg) if os.path.isfile(args.cfg) else 'exec "%s"' % args.cfg]
    lines += ['set %s "%s"' % (k, v) for k, v in args.set]
    # The harness owns the bot count, the limits and the botlib.
    lines += ['set botlibs q3', 'set botfill 0', 'set bots_autoload 0', 'set bots_minplayers 0', 'set minimumplayers 0',
              'set timelimit 0', 'set fraglimit 0', 'set capturelimit 0', 'set flood_msgs 0', 'set statsfile 0',
              'set public 0']
    return '\n'.join(lines) + '\n'


def lineup(paths, spec):
    names = botduel.roster(os.path.join(paths.gamedir, 'q3bot', 'bots.cfg'))
    rng = random.Random('%s/%d' % (spec['map'], spec['seed']))
    return rng.sample(names, spec['bots'])


def match_id(spec):
    return '%s-s%02d' % (spec['map'], spec['seed'])


def play(paths, so, wdir, spec, args):
    m, seed = spec['map'], spec['seed']
    make_tree(paths, wdir, m, args.aasdir)
    if os.path.isfile(args.cfg):	# a config from outside the install (the server repo's ctf.cfg)
        dst = os.path.join(wdir, 'colosseum', os.path.basename(args.cfg))
        if os.path.lexists(dst):
            os.remove(dst)
        shutil.copy(args.cfg, dst)
    with open(os.path.join(wdir, 'colosseum', 'igbench.cfg'), 'w') as f:
        f.write(bench_cfg(args))
    names = lineup(paths, spec)
    script = ['wait 20']
    for n in names:
        script += ['sv addbot "%s" %d' % (n, spec['skill']), 'wait 5']
    script += botduel.waits(botduel.WARMUP['dm'])
    script += ['sv botstats reset', 'sv igstats reset']
    if args.demo:
        script += ['mvdrecord -z %s' % match_id(spec)]
    script += botduel.waits(spec['frames'])
    script += ['sv botstats', 'sv igstats']
    if args.demo:
        script += ['mvdstop']
    script += ['quit']
    argv = [paths.q2proded, '+set', 'basedir', wdir, '+set', 'homedir', wdir, '+set', 'libdir', wdir,
            '+set', 'game', 'colosseum', '+set', 'dedicated', '1', '+set', 'net_port', '0',
            '+set', 'maxclients', '16', '+set', 'cheats', '1', '+set', 'fixedtime', '200',
            '+set', 'sv_mvd_enable', '1' if args.demo else '0',
            # never suspend the demo for want of an active player: bots may not count as one
            '+set', 'sv_mvd_suspend_time', '0',
            '+exec', 'igbench.cfg', '+map', m]
    env = dict(os.environ, BOTDUEL_SEED=str(seed), LD_PRELOAD=so)
    t0 = time.time()
    try:
        r = subprocess.run(argv, cwd=wdir, input='\n'.join(script) + '\n', text=True, env=env,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT, errors='replace',
                           timeout=spec['frames'] / 20 + 600)
        text, rc = r.stdout, r.returncode
    except subprocess.TimeoutExpired as e:
        text = e.stdout.decode('latin1') if isinstance(e.stdout, bytes) else (e.stdout or '')
        rc = 'timeout'
    res = parse(text, spec, names)
    res['rc'], res['wall'] = rc, round(time.time() - t0, 1)
    if rc != 0:
        res['problems'].append('server exited %s' % rc)
    if args.demo:
        for root, _, files in os.walk(wdir):
            for f in files:
                if f.startswith(match_id(spec)) and '.mvd2' in f:
                    os.makedirs(os.path.join(args.out, 'demos'), exist_ok=True)
                    shutil.copy(os.path.join(root, f), os.path.join(args.out, 'demos', f))
                    res['demo'] = f
        if 'demo' not in res:
            res['problems'].append('no demo was written')
    with open(os.path.join(args.out, 'logs', match_id(spec) + '.log'), 'w') as f:
        f.write(text)
    res['valid'] = not res['problems']
    return res


def kv(rest):
    p = rest.split()
    return {p[i]: int(p[i + 1]) for i in range(0, len(p) - 1, 2)}


def parse(text, spec, names):
    res = {'spec': spec, 'problems': [], 'clients': []}
    lines = text.splitlines()
    loads = sum(1 for l in lines if l.startswith('SpawnServer:'))
    if loads != 1:
        res['problems'].append('the map loaded %d times' % loads)
    try:
        start = next(i for i, l in enumerate(lines) if l.startswith('igstats reset '))
    except StopIteration:
        res['problems'].append('the window never opened')
        return res
    bots, igs = {}, {}
    # the brain's own event lines (game_q3/ai_ig_revenge.c), counted per match
    res['revenge'] = sum(1 for l in lines[start:] if re.match(r'^ig: revenge \d+ on \d+', l))
    res['revengekill'] = sum(1 for l in lines[start:] if l.startswith('ig: revenge kill '))
    res['sling'] = sum(1 for l in lines[start:] if re.match(r'^ig: slingshot \d+ length', l))
    res['fighthook'] = sum(1 for l in lines[start:] if re.match(r'^ig: fighthook \d+$', l))
    # CTF (src/ig/ig_ctfstats.c): the flag events and the window's tallies
    ev = {}
    capcarry = []
    for l in lines[start:]:
        m = re.match(r'^igctf \d+ (\w+) (-?\d+) team \d+ flag \d+(?: carry (\d+) dist (\d+))?', l)
        if m:
            ev[m.group(1)] = ev.get(m.group(1), 0) + 1
            if m.group(1) == 'capture' and m.group(3):
                capcarry.append(int(m.group(3)))
        m = re.match(r'^igctf summary (.*)$', l)
        if m:
            res['ctf'] = kv(m.group(1))
        m = re.match(r'^igctf client (\d+) (.*) name (.*)$', l)
        if m:
            res.setdefault('ctfcl', {})[int(m.group(1))] = kv(m.group(2))
    if 'ctf' in res:
        res['ctfev'] = ev
        res['capcarry'] = capcarry
    for l in lines[start:]:
        m = botduel.RE_ENDED.match(l)
        if m:
            res['problems'].append('the window saw: %s' % l.strip())
        m = RE_BOTSTATS.match(l)
        if m:
            slot, lib, skill, team, score, rest, name = m.groups()
            d = kv(rest)
            d.update(slot=int(slot), lib=lib, skill=skill, score=int(score), name=name)
            bots[int(slot)] = d
            continue
        m = RE_IGFRAME.match(l)
        if m:
            res['frames'] = int(m.group(3))
            res['upperz'] = int(m.group(4))
            continue
        m = RE_IG.match(l)
        if m:
            igs[int(m.group(1))] = kv(m.group(2))
    if res.get('frames') != spec['frames']:
        res['problems'].append('the window was %s frames, not %d' % (res.get('frames'), spec['frames']))
    for slot, b in sorted(bots.items()):
        if slot not in igs:
            res['problems'].append('no igstats for slot %d' % slot)
            continue
        c = dict(b)
        c.update(igs[slot])
        if c['lib'] != 'q3':
            res['problems'].append('%s is driven by %s' % (c['name'], c['lib']))
        res['clients'].append(c)
    if len(res['clients']) != len(names):
        res['problems'].append('%d of %d bots measured' % (len(res['clients']), len(names)))
    return res


def cmd_run(args, paths):
    paths.check()
    os.makedirs(os.path.join(args.out, 'logs'), exist_ok=True)
    so = botduel.shim(paths)
    seeds = list(range(1, int(args.seeds) + 1)) if args.seeds.isdigit() else [int(s) for s in args.seeds.split(',')]
    args.seeds = seeds
    meta = {'cfg': args.cfg, 'set': args.set, 'aasdir': args.aasdir, 'maps': args.maps, 'seeds': seeds, 'frames': args.frames,
            'bots': args.bots, 'skill': args.skill, 'lib': paths.lib,
            'git': subprocess.run(['git', '-C', ROOT, 'describe', '--always', '--dirty'],
                                  capture_output=True, text=True).stdout.strip(),
            'q3git': subprocess.run(['git', '-C', os.path.join(ROOT, 'vendor', 'q3a_bot_backport_for_q2'),
                                     'describe', '--always', '--dirty'],
                                    capture_output=True, text=True).stdout.strip(),
            'label': args.label}
    with open(os.path.join(args.out, 'run.json'), 'w') as f:
        json.dump(meta, f, indent=1)
    done = set()
    resfile = os.path.join(args.out, 'results.jsonl')
    if os.path.exists(resfile):
        for l in open(resfile):
            r = json.loads(l)
            if r.get('valid'):
                done.add(match_id(r['spec']))
    specs = [{'map': m, 'seed': s, 'frames': args.frames, 'bots': args.bots, 'skill': args.skill}
             for m in args.maps.split(',') for s in seeds]
    todo = [s for s in specs if match_id(s) not in done]
    print('igbench: %d matches, %d to play, %d jobs' % (len(specs), len(todo), args.jobs))
    work = os.path.join(args.out, 'work')

    def one(i_spec):
        i, spec = i_spec
        return play(paths, so, os.path.join(work, 'w%d' % (i % args.jobs)), spec, args)

    bad = 0
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex, open(resfile, 'a') as out:
        # one worker directory per job slot: schedule in waves so two matches never share one
        for k in range(0, len(todo), args.jobs):
            for r in ex.map(one, enumerate(todo[k:k + args.jobs])):
                out.write(json.dumps(r) + '\n')
                out.flush()
                tag = 'ok ' if r['valid'] else 'BAD'
                bad += not r['valid']
                print('  %s %s  %ss%s' % (tag, match_id(r['spec']), r['wall'],
                                          '' if r['valid'] else '  ' + '; '.join(r['problems'])))
    shutil.rmtree(work, ignore_errors=True)
    print('igbench: done, %d invalid' % bad)
    return 1 if bad else 0


# ------------------------------------------------------------------ report

METRICS = [
    # name, header, per-bot value from a client dict and the window (frames)
    ('frags', 'frags/min', lambda c, fr: c['kills'] / (fr * FRAME_S / 60)),
    ('deaths', 'deaths/min', lambda c, fr: c['deaths'] / (fr * FRAME_S / 60)),
    # railhits: OSP's rail-only count. botstats' hits include hook damage, which
    # inflated it (runs before 2026-11-21 report that, marked by no railhits)
    ('acc', 'rail acc %', lambda c, fr: 100.0 * c.get('railhits', c['hits']) / c['shots'] if c['shots'] else None),
    ('pads', 'pads/min', lambda c, fr: c['pads'] / (c['alive'] * FRAME_S / 60) if c['alive'] else None),
    ('hooks', 'hooks/min', lambda c, fr: c['hooks'] / (c['alive'] * FRAME_S / 60) if c['alive'] else None),
    ('climbs', 'hooks that climbed %', lambda c, fr: 100.0 * c.get('climbs', 0) / c['hooks'] if c['hooks'] else None),
    ('hooktime', 'on hook %', lambda c, fr: 100.0 * c['hookframes'] / c['alive'] if c['alive'] else None),
    ('upper', 'upper %', lambda c, fr: 100.0 * c['upper'] / c['alive'] if c['alive'] else None),
    ('speed', 'units/s', lambda c, fr: c['dist'] / (c['alive'] * FRAME_S) if c['alive'] else None),
    ('fast', 'over 400 u/s %', lambda c, fr: 100.0 * c['fast'] / c['alive'] if c.get('alive') and 'fast' in c else None),
    ('faster', 'over 600 u/s %', lambda c, fr: 100.0 * c['faster'] / c['alive'] if c.get('alive') and 'faster' in c else None),
    ('nearest', 'nearest player units', lambda c, fr: c['nearest'] / c['nearframes'] if c.get('nearframes') else None),
    ('crowd', 'crowded %', lambda c, fr: 100.0 * c['crowd'] / c['alive'] if c.get('alive') and 'crowd' in c else None),
    ('contact', 'to contact s', lambda c, fr: c['contactframes'] * FRAME_S / c['contacts'] if c['contacts'] else None),
    ('suicides', 'suicides/min', lambda c, fr: c['suicides'] / (fr * FRAME_S / 60)),
]


# Per match rather than per bot: the brain's revenge events (IG-8).
def _ctfsum(r, k):
    return sum(c.get(k, 0) for c in r.get('ctfcl', {}).values())


def _ctf(fn):
    return lambda r, fr: fn(r, fr) if 'ctf' in r else None


MATCH_METRICS = [
    # CTF (ig_ctfstats.c); None outside CTF, and such rows are left out of the report
    ('caps', 'CTF captures/10 min', _ctf(lambda r, fr: (r['ctf']['caps1'] + r['ctf']['caps2']) / (fr * FRAME_S / 600))),
    ('takes', 'CTF flag pickups/10 min', _ctf(lambda r, fr: (r['ctfev'].get('take', 0) + r['ctfev'].get('repickup', 0)) / (fr * FRAME_S / 600))),
    ('capshare', 'CTF pickups captured %', _ctf(lambda r, fr: 100.0 * r['ctfev'].get('capture', 0) / (r['ctfev'].get('take', 0) + r['ctfev'].get('repickup', 0))
                                                if r['ctfev'].get('take', 0) + r['ctfev'].get('repickup', 0) else None)),
    ('capcarry', 'CTF carry to capture s', _ctf(lambda r, fr: botduel.mean([x * FRAME_S for x in r['capcarry']]) if r['capcarry'] else None)),
    ('carryspeed', 'CTF carrier units/s', _ctf(lambda r, fr: _ctfsum(r, 'carrydist') / (_ctfsum(r, 'carryframes') * FRAME_S) if _ctfsum(r, 'carryframes') else None)),
    ('home', 'CTF flag at home %', _ctf(lambda r, fr: 100.0 * (r['ctf']['home1'] + r['ctf']['home2']) / (2 * fr))),
    ('unguarded', 'CTF home flag unguarded %', _ctf(lambda r, fr: 100.0 * (r['ctf']['unguarded1'] + r['ctf']['unguarded2']) / (r['ctf']['home1'] + r['ctf']['home2'])
                                                   if r['ctf']['home1'] + r['ctf']['home2'] else None)),
    ('helped', 'CTF carrier with a teammate %', _ctf(lambda r, fr: 100.0 * _ctfsum(r, 'helpframes') / _ctfsum(r, 'carryframes') if _ctfsum(r, 'carryframes') else None)),
    ('chased', 'CTF carrier with an enemy near %', _ctf(lambda r, fr: 100.0 * _ctfsum(r, 'chaseframes') / _ctfsum(r, 'carryframes') if _ctfsum(r, 'carryframes') else None)),
    ('lost', 'CTF carriers killed/10 min', _ctf(lambda r, fr: r['ctfev'].get('lost', 0) / (fr * FRAME_S / 600))),
    ('returns', 'CTF returns by players/10 min', _ctf(lambda r, fr: r['ctfev'].get('return', 0) / (fr * FRAME_S / 600))),
    ('autoret', 'CTF flags back on their own/10 min', _ctf(lambda r, fr: r['ctfev'].get('autoreturn', 0) / (fr * FRAME_S / 600))),
    ('revenge', 'grudges/bot/min', lambda r, fr: r.get('revenge', 0) / max(1, len(r['clients'])) / (fr * FRAME_S / 60)),
    ('sling', 'slingshots/bot/min', lambda r, fr: r.get('sling', 0) / max(1, len(r['clients'])) / (fr * FRAME_S / 60)),
    ('fighthook', 'fight hooks/bot/min', lambda r, fr: r.get('fighthook', 0) / max(1, len(r['clients'])) / (fr * FRAME_S / 60)),
    ('revengekill', 'grudges settled %', lambda r, fr: 100.0 * r['revengekill'] / r['revenge'] if r.get('revenge') else None),
]


def load(out):
    rs = [json.loads(l) for l in open(os.path.join(out, 'results.jsonl'))]
    keep = {}
    for r in rs:
        if r.get('valid'):
            keep[match_id(r['spec'])] = r
    return list(keep.values()), json.load(open(os.path.join(out, 'run.json')))


def per_match(r):
    """Each metric's mean over the match's bots: the match is the unit."""
    fr = r['frames']
    out = {}
    for key, _, fn in METRICS:
        xs = [fn(c, fr) for c in r['clients']]
        out[key] = botduel.mean(xs)
    for key, _, fn in MATCH_METRICS:
        out[key] = fn(r, fr)
    return out


def summarise(rs):
    by = {}
    for r in rs:
        by.setdefault(r['spec']['map'], []).append(per_match(r))
    by['ALL'] = [x for k in list(by) for x in by[k]]
    return by


def fmt(x, d=2):
    return '-' if x is None else ('%.' + str(d) + 'f') % x


def cmd_report(args, paths):
    rs, meta = load(args.out)
    rng = random.Random(1)
    cur = summarise(rs)
    base = None
    if args.base:
        brs, bmeta = load(args.base)
        base = summarise(brs)
    lines = ['# igbench report: %s' % (meta.get('label') or os.path.basename(os.path.abspath(args.out))), '',
             '- config `%s`%s' % (meta['cfg'], ''.join(', `set %s %s`' % tuple(s) for s in meta['set'])),
             '- %d bots at skill %d, %d frames (%.1f min) per match, seeds %s' % (
                 meta['bots'], meta['skill'], meta['frames'], meta['frames'] * FRAME_S / 60,
                 ','.join(map(str, meta['seeds']))),
             '- colosseum `%s`, q3 backport `%s`' % (meta['git'], meta['q3git']),
             '- navigation files: %s' % (meta.get('aasdir') or 'the install\'s q3bot/maps'),
             '- %d valid matches' % len(rs)]
    if base:
        lines.append('- compared with `%s` (%s)' % (args.base, bmeta.get('label') or ''))
    lines += ['', 'Each figure is the mean over matches of the per-match mean over bots; '
              '[a, b] is a 95% bootstrap interval over matches.', '']
    for mp in sorted(k for k in cur if k != 'ALL') + ['ALL']:
        ms = cur[mp]
        lines += ['## %s (%d matches)' % (mp, len(ms)), '']
        if base and mp in base:
            lines += ['| metric | base | this | change | 95% interval of change |', '|---|---|---|---|---|']
        else:
            lines += ['| metric | mean | 95% interval |', '|---|---|---|']
        for key, head, _ in METRICS + MATCH_METRICS:
            xs = [m.get(key) for m in ms]
            if all(x is None for x in xs) and not (base and mp in base and any(m.get(key) is not None for m in base[mp])):
                continue	# a metric this mode does not have (CTF's outside CTF, rail accuracy in CTF)
            mu = botduel.mean(xs)
            lo, hi = botduel.bootstrap_ci(xs, rng)
            if base and mp in base:
                bxs = [m.get(key) for m in base[mp] if m.get(key) is not None]
                bmu = botduel.mean(bxs)
                diffs = None
                if bxs and [x for x in xs if x is not None]:
                    a = [x for x in xs if x is not None]
                    ds = sorted(botduel.mean([rng.choice(a) for _ in a]) - botduel.mean([rng.choice(bxs) for _ in bxs])
                                for _ in range(2000))
                    diffs = (ds[50], ds[1949])
                ch = None if mu is None or bmu is None else mu - bmu
                lines.append('| %s | %s | %s | %s | %s |' % (
                    head, fmt(bmu), fmt(mu), fmt(ch),
                    '-' if not diffs else '[%s, %s]%s' % (fmt(diffs[0]), fmt(diffs[1]),
                                                          ' *' if diffs[0] > 0 or diffs[1] < 0 else '')))
            else:
                lines.append('| %s | %s | %s |' % (head, fmt(mu), '-' if lo is None else '[%s, %s]' % (fmt(lo), fmt(hi))))
        lines.append('')
    if base:
        lines.append('`*` the interval of the change excludes 0.')
    with open(os.path.join(args.out, 'report.md'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    with open(os.path.join(args.out, 'matches.csv'), 'w', newline='') as f:
        w = csv.writer(f)
        cols = ['kills', 'deaths', 'suicides', 'hits', 'shots', 'alive', 'upper', 'dist', 'pads', 'hooks',
                'hookframes', 'climbs', 'lives', 'contacts', 'contactframes']
        w.writerow(['map', 'seed', 'frames', 'slot', 'name', 'skill'] + cols)
        for r in sorted(rs, key=lambda r: match_id(r['spec'])):
            for c in r['clients']:
                w.writerow([r['spec']['map'], r['spec']['seed'], r['frames'], c['slot'], c['name'], c['skill']]
                           + [c.get(k) for k in cols])
    print(open(os.path.join(args.out, 'report.md')).read())


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0], epilog=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    p = sub.add_parser('run', help='play the matches (resumable)')
    p.add_argument('-o', '--out', required=True)
    p.add_argument('--maps', required=True, help='comma list; each needs a Q3 .aas in the install')
    p.add_argument('--seeds', default='5', help='N for seeds 1..N, or a comma list (default 5)')
    p.add_argument('--frames', type=int, default=6000, help='the window, in frames (default 6000 = 10 min)')
    p.add_argument('--bots', type=int, default=5)
    p.add_argument('--skill', type=int, default=4)
    p.add_argument('--cfg', default='instagib.cfg', help='server config, relative to the gamedir, or a file path (copied in)')
    p.add_argument('--set', nargs=2, action='append', default=[], metavar=('CVAR', 'VALUE'),
                   help='set after the config (repeatable): the feature switch under test')
    p.add_argument('--label', default='')
    p.add_argument('--aasdir', help='take the maps\' Q3 .aas files from here (default: the install\'s q3bot/maps)')
    p.add_argument('--demo', action='store_true', help='record a server-side demo of each window')
    p.add_argument('-j', '--jobs', type=int, default=max(1, (os.cpu_count() or 2) - 1))
    p = sub.add_parser('report', help='report.md and matches.csv for a run')
    p.add_argument('-o', '--out', required=True)
    p.add_argument('--base', help='another run to compare against')
    args = ap.parse_args()
    paths = Paths()
    sys.exit({'run': cmd_run, 'report': cmd_report}[args.cmd](args, paths) or 0)


if __name__ == '__main__':
    main()
