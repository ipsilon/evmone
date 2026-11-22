#!/usr/bin/env python3
"""Throwaway: builds libevmone variants (code alignment flags for baseline_execution.cpp x padding of the SMOD
handler) and times them in one process.

variants.py build ARCH BUILD_DIR VARIANTS_DIR
variants.py run ARCH VARIANTS_DIR ROUNDS OUT.json LABEL

ARCH is the x86-64 level (2, 3) or '-' for the default (non-x86). Uses $CC and $CXX.
The SMOD padding (0, 16, 32, 48 bytes of NOPs at the start of its handler) moves the code after that handler:
these are the layout samples. A flag that aligns the handlers absorbs the padding at the next handler.
"""
import collections
import glob
import json
import os
import platform
import re
import shlex
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '../../..'))
ALIGN = {
    # -falign-loops=64 does not move GCC's computed-goto handlers; it is a noise control for the dispatch.
    'gcc': {'def': [], 'al64': ['-falign-loops=64'], 'lab32': ['-falign-labels=32']},
    # clang's handlers are loop headers: -falign-loops aligns 149 of the 155 handler entries.
    'clang': {'def': [], 'al32': ['-falign-loops=32'], 'al64': ['-falign-loops=64']},
}
PADS = (0, 16, 32, 48)


def sh(cmd, **kw):
    print('+', cmd if isinstance(cmd, str) else ' '.join(cmd), flush=True)
    return subprocess.run(cmd, check=True, shell=isinstance(cmd, str), **kw)


def family():
    v = subprocess.run([os.environ['CXX'], '--version'], check=True, capture_output=True, text=True).stdout
    return ('clang' if 'clang' in v else 'gcc'), v.splitlines()[0]


def build(arch, bdir, vdir):
    comp, _ = family()
    cfg = ['cmake', '-S', ROOT, '-B', bdir, '-G', 'Ninja', '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_SHARED_LIBS=ON',
           '-DEVMONE_TESTING=OFF', '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON']
    if arch != '-':
        cfg.append(f'-DEVMONE_X86_64_ARCH_LEVEL={arch}')
    sh(cfg)
    sh(['cmake', '--build', bdir, '--target', 'evmone', '--parallel', str(os.cpu_count())])

    src = os.path.join(ROOT, 'lib/evmone/baseline_execution.cpp')
    hooked = os.path.abspath(os.path.join(vdir, 'baseline_execution_pad.cpp'))
    os.makedirs(vdir, exist_ok=True)
    s = open(src).read()
    old = '#define ASM_COMMENT(COMMENT) asm("# " #COMMENT)  // NOLINT(hicpp-no-assembler)'
    new = ('#ifndef PAD_OP\n#define PAD_OP OP_STOP\n#define PAD_N 0\n#endif\n'
           '#define ASM_COMMENT(COMMENT) asm volatile("# " #COMMENT "\\n\\t.if %c0\\n\\t.skip %c0, 0x90\\n\\t.endif" '
           ':: "i"(COMMENT == PAD_OP ? PAD_N : 0))')
    assert s.count(old) == 1
    open(hooked, 'w').write(s.replace(old, new))

    # The compile command of baseline_execution.cpp, without the dependency/module scanning arguments.
    cmds = json.load(open(os.path.join(bdir, 'compile_commands.json')))
    entry = next(c for c in cmds if c['file'].endswith('lib/evmone/baseline_execution.cpp'))
    args = shlex.split(entry['command']) if 'command' in entry else entry['arguments']
    i = args.index('-o')
    obj_rel = args[i + 1]
    args = [a for k, a in enumerate(args) if k not in (i, i + 1) and a != '-c' and a != entry['file']
            and not a.endswith('lib/evmone/baseline_execution.cpp')]
    args = [a for a in args if not a.startswith(('-MD', '-MT', '-MF', '-fmodules-ts', '-fmodule-mapper', '-fdeps-',
                                                 '@')) and not a.endswith(('.o', '.d', '.modmap'))]

    # The link command of libevmone, from ninja.
    link = subprocess.run(['ninja', '-C', bdir, '-t', 'commands', 'evmone'], check=True, capture_output=True,
                          text=True).stdout.strip().splitlines()
    link = next(c for c in reversed(link) if 'baseline_execution.cpp.o' in c and ' -o ' in c)
    m = re.match(r'^cd (\S+) && (.*)$', link)
    link_dir, link = (m.group(1), m.group(2)) if m else (bdir, link)
    link = re.sub(r'^:\s*&&\s*', '', link)
    link = re.sub(r'\s*&&\s*:\s*$', '', link)
    link = shlex.split(link)
    for j in reversed(range(len(link))):  # drop the dependency file, which the parallel links would share
        if link[j].startswith('--dependency-file='):
            del link[j - 1 if link[j - 1] == '-Xlinker' else j:j + 1]
    k = next(j for j, a in enumerate(link) if a.endswith('baseline_execution.cpp.o'))
    o = link.index('-o')

    def one(job):
        al, p = job
        name = f'{comp}-{al}-p{p}'
        obj = os.path.abspath(os.path.join(vdir, f'{name}.o'))
        subprocess.run(args + ['-I', os.path.join(ROOT, 'lib/evmone'), '-DPAD_OP=OP_SMOD', f'-DPAD_N={p}']
                       + ALIGN[comp][al] + ['-c', hooked, '-o', obj], check=True, cwd=entry['directory'])
        cmd = list(link)
        cmd[k] = obj
        cmd[o + 1] = os.path.abspath(os.path.join(vdir, f'{name}.so'))
        subprocess.run(cmd, check=True, cwd=link_dir)
        return name

    with ThreadPoolExecutor(max_workers=os.cpu_count()) as ex:
        for n in ex.map(one, [(al, p) for al in ALIGN[comp] for p in PADS]):
            print('built', n, flush=True)


NOP = re.compile(r'\bnop|xchg\s+%ax,%ax')
TERM = re.compile(r'^(jmp|jmpq|ret|retq|ud2|int3|hlt|b|br|ret|brk|udf)(\s|$)')


def layout(obj):
    """The handler entries' alignment and the padding in dispatch_cgoto of a variant's object."""
    rel = subprocess.run(['readelf', '-rW', obj], check=True, capture_output=True, text=True).stdout
    handlers = []
    for t in rel.split("\nRelocation section '"):
        if t.startswith('.rela.data.rel.ro'):
            offs = re.findall(r'R_\w+?_(?:64|ABS64)\s+[0-9a-f]+ \.text \+ ([0-9a-f]+)', t)
            if len(offs) == 256:
                handlers = [int(x, 16) for x in offs]
    hs = sorted(set(handlers))
    dis = subprocess.run(['objdump', '-d', '-w', '-j', '.text', obj], check=True, capture_output=True,
                         text=True).stdout
    func = None
    size = collections.Counter()
    pad = collections.Counter()
    pad_exec = collections.Counter()
    falls = False  # whether the previous instruction falls through into this one
    for line in dis.splitlines():
        mm = re.match(r'^([0-9a-f]+) <(.+)>:$', line)
        if mm:
            func = 'dispatch_cgoto' if 'dispatch_cgoto' in mm.group(2) else 'other'
            falls = False
            continue
        mm = re.match(r'^\s+[0-9a-f]+:\t([0-9a-f ]+)\t(.*)$', line)
        if not mm or func is None:
            continue
        n = len(mm.group(1).replace(' ', '')) // 2
        text = mm.group(2).strip()
        size[func] += n
        if NOP.search(text):
            pad[func] += n
            if falls:
                pad_exec[func] += n
        else:
            falls = not TERM.match(text)
    return {'handlers': len(hs), 'h64': sum(1 for h in hs if h % 64 == 0), 'h32': sum(1 for h in hs if h % 32 == 0),
            'h16': sum(1 for h in hs if h % 16 == 0), 'cgoto_size': size['dispatch_cgoto'],
            'cgoto_pad': pad['dispatch_cgoto'], 'cgoto_pad_exec': pad_exec['dispatch_cgoto'],
            'text_size': sum(size.values())}


def cpu_name():
    try:
        out = subprocess.run(['lscpu'], check=True, capture_output=True, text=True).stdout
        m = re.search(r'^Model name:\s*(.+)$', out, re.M)
        if m:
            return m.group(1).strip()
    except (OSError, subprocess.CalledProcessError):
        pass
    return platform.processor() or '?'


def run(arch, vdir, rounds, out, label):
    comp, version = family()
    exe = os.path.join(vdir, 'evmtime')
    sh([os.environ['CXX'], '-O2', '-std=c++20', '-I', os.path.join(ROOT, 'evmc/include'),
        os.path.join(HERE, 'evmtime.cpp'), '-ldl', '-o', exe])
    libs = sorted(glob.glob(os.path.join(vdir, f'{comp}-*.so')))
    lay = {os.path.basename(o)[:-2]: layout(o) for o in sorted(glob.glob(os.path.join(vdir, f'{comp}-*.o')))}
    for name, d in lay.items():
        print(name, d, flush=True)
    r = subprocess.run([exe, str(rounds), os.path.join(HERE, 'programs'), os.path.join(HERE, 'reps.txt')] + libs,
                       check=True, capture_output=True, text=True)
    json.dump({'label': label, 'compiler': comp, 'version': version, 'arch': arch, 'cpu': cpu_name(),
               'nproc': os.cpu_count(), 'layout': lay, 'results': json.loads(r.stdout)}, open(out, 'w'))
    print('cpu', cpu_name())


if __name__ == '__main__':
    if sys.argv[1] == 'build':
        build(sys.argv[2], sys.argv[3], sys.argv[4])
    else:
        run(sys.argv[2], sys.argv[3], int(sys.argv[4]), sys.argv[5], sys.argv[6])
