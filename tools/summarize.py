#!/usr/bin/env python3
"""Aggregate results\\<area>.txt files produced by run_all.bat.

usage: summarize.py [results_dir] [-v]
  -v  also list every FAIL test with its failing check lines
"""
import os, re, sys

def main():
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    verbose = '-v' in sys.argv
    d = args[0] if args else 'results'
    tot = dict(tests=0, failed=0, skipped=0, checks_pass=0, checks_fail=0)
    rows, bad = [], []
    for fn in sorted(os.listdir(d)):
        if not fn.endswith('.txt'):
            continue
        area = fn[:-4]
        txt = open(os.path.join(d, fn), errors='replace').read()
        m = None
        for m in re.finditer(r'^SUMMARY (.*)$', txt, re.M):
            pass
        if not m:
            rows.append((area, 'NO SUMMARY (crash/hang/abort?)'))
            bad.append(area)
            continue
        kv = {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)', m.group(1))}
        for k in tot:
            tot[k] += kv.get(k, 0)
        rows.append((area, 'tests=%(tests)d failed=%(failed)d skipped=%(skipped)d checks=%(checks_pass)d/%(checks_fail)d' % kv))
        if kv.get('failed'):
            bad.append(area)
        if verbose:
            cur = None
            for line in txt.splitlines():
                if line.startswith('FAIL '):
                    print('%s: %s' % (area, line))
                elif line.startswith('  FAIL '):
                    print('    ' + line.strip())
    w = max([len(r[0]) for r in rows] + [4])
    for a, s in rows:
        print('%-*s  %s' % (w, a, s))
    print('-' * 60)
    print('TOTAL tests=%(tests)d failed=%(failed)d skipped=%(skipped)d checks_pass=%(checks_pass)d checks_fail=%(checks_fail)d' % tot)
    print('areas with failures: %s' % (', '.join(bad) or 'none'))
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
