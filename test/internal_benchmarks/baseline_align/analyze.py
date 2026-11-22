#!/usr/bin/env python3
"""analyze.py FILE.json...: per compiler, x86-64 level and CPU, the time of each alignment variant vs the default
build (mean over the paddings of the minimum over the rounds), and [the spread over the paddings]."""
import collections
import json
import math
import re
import statistics
import sys


def cpu_short(c):
    m = re.search(r'(EPYC \S+|Xeon\(R\) (?:Platinum|Gold) \S+|Xeon\(R\) \S+|Neoverse-\S+|Ryzen \S+ \S+|\S+$)', c)
    return (m.group(1) if m else c).replace('Xeon(R) Platinum ', 'Xeon ').replace('Xeon(R) ', 'Xeon ')


jobs = [json.load(open(f)) for f in sys.argv[1:]]
groups = collections.defaultdict(list)
for j in jobs:
    groups[(j['compiler'], (re.findall(r'\d+\.\d+(?:\.\d+)?', j['version']) or ['?'])[0],
            j['arch'], cpu_short(j['cpu']))].append(j)
for key, js in sorted(groups.items()):
    comp = key[0]
    v0 = js[0]['results'][next(iter(js[0]['results']))]
    names = sorted({re.sub(r'-p\d+\.so$', '', n) for n in v0}, key=lambda n: (n != f'{comp}-def', n))
    pads = sorted({int(re.search(r'-p(\d+)\.so$', n).group(1)) for n in v0})
    als = [n.split('-', 1)[1] for n in names]
    print(f'== {comp} {key[1]} v{key[2]} on {key[3]}: {len(js)} jobs ({", ".join(j["label"] for j in js)})')
    lay = js[0]['layout']
    print('   ' + '; '.join(f"{a}: cgoto {lay[f'{comp}-{a}-p0']['cgoto_size']} B, handlers 64-aligned "
                           f"{lay[f'{comp}-{a}-p0']['h64']}/155, executed padding {lay[f'{comp}-{a}-p0']['cgoto_pad_exec']} B"
                           for a in als))
    per_prog = collections.defaultdict(lambda: {a: {'r': [], 's': []} for a in als})
    geo = {a: [] for a in als}
    for j in js:
        rat = {a: [] for a in als}
        for prog, byv in j['results'].items():
            v = {k[:-3]: min(t) for k, t in byv.items()}
            base = statistics.mean(v[f'{comp}-def-p{p}'] for p in pads)
            for a in als:
                per = [v[f'{comp}-{a}-p{p}'] for p in pads]
                r = statistics.mean(per) / base
                rat[a].append(r)
                per_prog[prog][a]['r'].append(r)
                per_prog[prog][a]['s'].append(max(per) / min(per) - 1)
        for a in als:
            geo[a].append(math.exp(statistics.mean(map(math.log, rat[a]))))
    print(f'   {"program":30}' + ''.join(f'{a:>18}' for a in als))
    for prog in sorted(per_prog):
        d = per_prog[prog]
        print(f'   {prog:30}' + ''.join(
            f'{100 * (statistics.median(d[a]["r"]) - 1):+7.1f}% [{100 * statistics.median(d[a]["s"]):5.1f}%]' for a in als))
    print(f'   {"geomean per job":30}' + ''.join(f'{",".join(f"{100 * (g - 1):+.1f}" for g in geo[a]):>18}' for a in als))
