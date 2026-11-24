import json, sys, mlib, numpy as np
from fit_styles import T, F, ORDER
mlib.set_warm(0.15)
rep = json.load(open('fit_params.json'))
names = sys.argv[1:] or ORDER
rows = []
for n in names:
    if n not in rep: continue
    d = rep[n]['params']; errs = []
    line = f'{n:20s}'
    for dr in (0, .5, 1):
        m = mlib.measure(d, dr, fs=384000.0); t = T[n][dr]
        e = [max(m['H'][h],-100)-max(t['H'][h],-100) for h in (2,3,5)]
        errs += [abs(x) for x in e]
        line += f" | d{int(dr*100):3d} fund {m['fund']:6.1f}/{t['fund']:6.1f} thd {m['thd']:6.1f}/{t['thd']:6.1f} H2 {m['H'][2]:6.1f}/{t['H'][2]:6.1f} H3 {m['H'][3]:6.1f}/{t['H'][3]:6.1f} H5 {m['H'][5]:6.1f}/{t['H'][5]:6.1f}"
    m1 = mlib.measure(d,.5,fs=384000.0); m0 = mlib.measure(d,.5,100.0,fs=384000.0); m5 = mlib.measure(d,.5,5000.0,fs=384000.0); f=F[n]
    line += f" | g100 {m0['fund']:6.1f}/{f['g100']:6.1f} g5k {m5['fund']:6.1f}/{f['g5k']:6.1f}"
    print(line); print(f'{"":20s}  median |harm err| {np.median(errs):.1f} dB  max {max(errs):.1f}')
