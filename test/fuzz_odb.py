#!/usr/bin/env python3
"""Mutation fuzzer for odb2kicad: copies a seed job, applies 1-3 text mutations (named attribute
keys, bad index lines, odd numbers, truncated or reordered lines), runs a native CLI built with
ASan/UBSan, and keeps every case that crashes (sanitizer report, signal, timeout).

    g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined \
        -Iodb2kicad/src $(find odb2kicad/src -name '*.cpp' ! -name wasm_bindings.cpp) -o /tmp/odb2kicad_asan
    python3 test/fuzz_odb.py <seeds dir> <out dir> <n cases> <rng seed>   # ODB2KICAD_BIN=/tmp/odb2kicad_asan

<seeds dir> holds unpacked jobs (samples/*, test/synthetic_odb.py variants). With GEN_ONLY=1 the
cases are only written (<out>/g-<seed>-<n>), to run them through a WASM build instead."""
import os, random, re, shutil, subprocess, sys, json, tempfile

SEEDS, OUT, N, SEED = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
BIN = os.environ.get('ODB2KICAD_BIN', '/tmp/odb2kicad_asan')
SKIP_SEEDS = {'designodb_rigidflex'}  # 30 s per run; too slow to fuzz
rng = random.Random(SEED)

NUMS = ['', '-', '.', 'nan', 'inf', '-inf', '1e400', '-1e400', '9' * 400, '-1', '-2147483649',
        '2147483648', '99999999999', '0', '-0', '1e-400', '0x10', '1,5', '+', 'e5', '1.2.3']
WORDS = ['', 'X', ';', '$', '#', '@', '&', '=', ';0=1,ID=7', ';ID=7', ';0', ';=', ';,', '0=', ',',
         'r', 'rect', 'rect10x', 'r1e400', 'oval-1x2', 'donut_r10x20', 'thr', 's0', 'r0', 'moire',
         'rect10x10xr', 'el', 'tri0x0', 'hex_l', 'oct0x0x0', 'bfr', 'rc_ths', 'home', 'ID=', '\t']


def files_of(job):
    for root, _, fs in os.walk(job):
        for f in fs:
            if f.startswith('.') or f.endswith('.z'):
                continue
            yield os.path.join(root, f)


def mutate_lines(lines):
    if not lines:
        return ['@' + rng.choice(WORDS)]
    op = rng.randrange(10)
    i = rng.randrange(len(lines))
    toks = lines[i].split(' ')
    if op == 0:  # replace a token with a weird number
        j = rng.randrange(len(toks)); toks[j] = rng.choice(NUMS); lines[i] = ' '.join(toks)
    elif op == 1:  # replace a token with a weird word
        j = rng.randrange(len(toks)); toks[j] = rng.choice(WORDS); lines[i] = ' '.join(toks)
    elif op == 2:  # truncate the line
        lines[i] = lines[i][:rng.randrange(len(lines[i]) + 1)]
    elif op == 3:  # delete a line
        del lines[i]
    elif op == 4:  # duplicate a line elsewhere
        lines.insert(rng.randrange(len(lines) + 1), lines[i])
    elif op == 5:  # append named / odd attributes
        lines[i] = lines[i] + rng.choice([' ;0=1,ID=7', ';ID=99', ' ;0=1,2,3=,ID', ' ;' + ',' * 5, ' ;-1', ' ;99999999'])
    elif op == 6:  # numeric index fields (records like $12, @3, &4, T 1 ...)
        m = re.search(r'\d+', lines[i])
        if m:
            lines[i] = lines[i][:m.start()] + rng.choice(NUMS + ['100000', '65536', '-5']) + lines[i][m.end():]
    elif op == 7:  # drop the tail of the file
        del lines[i:]
    elif op == 8:  # swap two lines
        k = rng.randrange(len(lines)); lines[i], lines[k] = lines[k], lines[i]
    else:  # change a number by scaling it hugely
        m = list(re.finditer(r'-?\d+\.?\d*', lines[i]))
        if m:
            mm = rng.choice(m)
            lines[i] = lines[i][:mm.start()] + rng.choice(['1e9', '-1e9', '1e30', '0', '1e-9', '123456789']) + lines[i][mm.end():]
    return lines


def run(job):
    try:
        p = subprocess.run([BIN, job, os.path.join(tmp, 'out.kicad_pcb')], capture_output=True, timeout=60,
                           env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0:abort_on_error=0',
                                    UBSAN_OPTIONS='print_stacktrace=1:halt_on_error=1'))
        err = p.stderr.decode('latin1')
        crash = p.returncode < 0 or 'ERROR: AddressSanitizer' in err or 'runtime error:' in err \
            or 'terminate called' in err or p.returncode not in (0, 1, 2)
        return p.returncode, crash, err
    except subprocess.TimeoutExpired:
        return 'timeout', True, ''


seeds = [d for d in sorted(os.listdir(SEEDS)) if d not in SKIP_SEEDS]
os.makedirs(OUT, exist_ok=True)
tmp = tempfile.mkdtemp(prefix='fz')
stats = {'cases': 0, 'crash': 0, 'fail': 0}
for n in range(N):
    seed = rng.choice(seeds)
    job = os.path.join(tmp, seed)
    shutil.rmtree(job, ignore_errors=True)
    shutil.copytree(os.path.join(SEEDS, seed), job)
    fl = list(files_of(job))
    log = []
    for _ in range(rng.randint(1, 3)):
        f = rng.choice(fl)
        with open(f, encoding='latin1') as fh:
            lines = fh.read().split('\n')
        before = len(lines)
        lines = mutate_lines(lines)
        with open(f, 'w', encoding='latin1') as fh:
            fh.write('\n'.join(lines))
        log.append(os.path.relpath(f, job))
    if os.environ.get('GEN_ONLY'):
        dst = os.path.join(OUT, f'g-{SEED}-{n}'); shutil.copytree(job, dst)
        open(dst + '.json', 'w').write(json.dumps({'seed': seed, 'mutated': log}))
        continue
    rc, crash, err = run(job)
    stats['cases'] += 1
    if rc not in (0,) and not crash:
        stats['fail'] += 1
    if crash:
        stats['crash'] += 1
        sig = ''
        m = re.search(r'(runtime error: .*|ERROR: AddressSanitizer: [\w-]+|terminate called.*)', err)
        sig = m.group(1)[:160] if m else f'rc={rc}'
        frame = re.search(r'#\d+ 0x[0-9a-f]+ in (\S+) (\S+odb2kicad/src/\S+)', err)
        where = frame.group(2).split('odb2kicad/src/')[-1] if frame else ''
        dst = os.path.join(OUT, f'case-{SEED}-{n}')
        shutil.copytree(job, dst)
        with open(dst + '.json', 'w') as fh:
            json.dump({'seed': seed, 'mutated': log, 'rc': rc, 'sig': sig, 'where': where, 'stderr': err[-4000:]}, fh, indent=1)
        print(json.dumps({'n': n, 'seed': seed, 'sig': sig, 'where': where}), flush=True)
    if n % 50 == 0:
        print(json.dumps({'progress': n, **stats}), flush=True)
print(json.dumps({'done': True, **stats}), flush=True)
shutil.rmtree(tmp, ignore_errors=True)
