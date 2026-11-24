# Fits every Neptune MB style to the published black-box measurements of the reference
# plug-in (harmonics H2..H7 + fundamental at 0/50/100 % drive, 1 kHz, -6 dBFS, and the
# 100 Hz / 5 kHz gain + THD at 50 % drive).  Writes ../Source/StyleTable.h and fit_report.json
import json, sys, os, numpy as np, mlib
from scipy.optimize import least_squares
from multiprocessing import Pool
T, F = mlib.load_targets()
mlib.set_warm(0.1)
ORDER = ['Subtle Tube','Clean Tube','Warm Tube','Broken Tube','Subtle Tape','Clean Tape','Warm Tape','Old Tape',
 'American Tweed Amp','American Plexi Amp','British Rock Amp','British Pop Amp','Smooth Amp','Crunchy Amp','Lead Amp',
 'Screaming Amp','Power Amp','Subtle Saturation','Gentle Saturation','Heavy Saturation','Subtle Transformer',
 'Gentle Transformer','Warm Transformer','Smudge','Breakdown','Foldback','Rectify','Destroy']
BASE = dict(kind=0,g0=0,g100=20,gamma=1,knee=2,b0=0,b100=0,preHp=0,prePkF=0,prePkDb=0,prePkQ=0.7,postPkF=0,postPkDb=0,
            postPkQ=0.7,postLp=0,postLpOrder=0,s0=0,s100=0,knee2=2,c0=0,c50=0,c100=0)
# free parameter -> (lo, hi, log?)
B = dict(g0=(-40,45,0), g100=(-30,60,0), gamma=(0.25,4,0), knee=(0.7,16,0), b0=(-1.5,1.5,0), b100=(-1.5,1.5,0),
         preHp=(15,900,1), prePkF=(60,6000,1), prePkDb=(-24,24,0), prePkQ=(0.25,4,0), postPkF=(60,9000,1),
         postPkDb=(-24,24,0), postPkQ=(0.25,4,0), postLp=(1200,22000,1), s0=(-1,5,0), s100=(-1,5,0), a0=(-24,30,0), a100=(-24,30,0), knee2=(0.7,16,0))
GEN = ['g0','g100','gamma','knee']
ASYM = ['b0','b100']
FILT = ['preHp','prePkF','prePkDb','prePkQ','postPkF','postPkDb','postPkQ','postLp']
def cfg(name):
    if 'Tube' in name:        return dict(free=GEN+ASYM+['prePkF','prePkDb','postLp'], init=dict(postLp=20000,postLpOrder=1,prePkF=800))
    if 'Tape' in name:        return dict(free=GEN+['prePkF','prePkDb','prePkQ','postLp'], init=dict(postLp=16000,postLpOrder=1,prePkF=200))
    if 'Saturation' in name:  return dict(free=GEN+['postLp'], init=dict(postLp=20000,postLpOrder=1))
    if 'Transformer' in name: return dict(free=GEN+ASYM+['prePkF','prePkDb'], init=dict(knee=8,prePkF=300))
    if 'Amp' in name:         return dict(free=GEN+ASYM+FILT+['a0','a100','knee2'], init=dict(kind=6,preHp=120,prePkF=900,prePkDb=6,postPkF=1000,postPkDb=-3,postLp=5000,postLpOrder=2,g0=10,g100=30,s0=0,s100=6,knee2=3))
    if name == 'Foldback':    return dict(free=['g0','g100','gamma','knee'], init=dict(kind=1,g0=-3,g100=15,knee=4))
    if name == 'Breakdown':   return dict(free=['g0','g100','gamma','knee','b100'], init=dict(kind=4,s0=0,s100=1,g0=-6,g100=30,knee=4))
    if name == 'Destroy':     return dict(free=['g0','g100','gamma','knee','s0','s100'], init=dict(kind=3,s0=4.6,s100=2.8,g0=0,g100=12,knee=6))
    return None

def unpack(x, free, init):
    d = dict(BASE); d.update(init)
    for v, k in zip(x, free):
        lo, hi, lg = B[k]; d[k] = 10**v if lg else v
    if 'a0' in d: d['s0'] = d.pop('a0')
    if 'a100' in d: d['s100'] = d.pop('a100')
    return d
def pack(d, free):
    out = []
    for k in free:
        lo, hi, lg = B[k]; v = np.clip(d['s0' if k=='a0' else 's100' if k=='a100' else k], lo, hi); out.append(np.log10(v) if lg else v)
    return np.array(out)
def bounds(free):
    lo = [np.log10(B[k][0]) if B[k][2] else B[k][0] for k in free]
    hi = [np.log10(B[k][1]) if B[k][2] else B[k][1] for k in free]
    return lo, hi
FL = -100.0
NSTART = int(os.environ.get('NSTART','1')); NFEV = int(os.environ.get('NFEV','60'))
def resid(x, name, free, init):
    d = unpack(x, free, init); t = T[name]; f = F[name]; r = []
    for dr in (0.0, 0.5, 1.0):
        m = mlib.measure(d, dr)
        for h in (2,3,4,5,7):
            tt = max(t[dr]['H'][h], FL); mm = max(m['H'][h], FL)
            r.append((mm - tt) / (2.0 if tt > -60 else 6.0))
        r.append((20*np.log10(max(m['thd'],1e-3)) - 20*np.log10(max(t[dr]['thd'],1e-3))) / 1.5)
    m1 = mlib.measure(d, 0.5); m0 = mlib.measure(d, 0.5, 100.0); m5 = mlib.measure(d, 0.5, 5000.0)
    r.append(((m0['fund']-m1['fund']) - (f['g100']-f['g1k'])) / 1.0)
    r.append(((m5['fund']-m1['fund']) - (f['g5k']-f['g1k'])) / 1.0)
    r.append((20*np.log10(max(m0['thd'],1e-3)) - 20*np.log10(max(f['t100'],1e-3))) / 2.0)
    r.append((20*np.log10(max(m5['thd'],1e-3)) - 20*np.log10(max(f['t5k'],1e-3))) / 2.0)
    return np.array(r)

def makeup(d, name):
    for dr, key in ((0.0,'c0'),(0.5,'c50'),(1.0,'c100')):
        d[key] = 0.0
    for dr, key in ((0.0,'c0'),(0.5,'c50'),(1.0,'c100')):
        dd = dict(d); dd['c0'] = dd['c50'] = dd['c100'] = 0.0
        d[key] = T[name][dr]['fund'] - mlib.measure(dd, dr)['fund']
    return d

def fit_one(name):
    c = cfg(name)
    if c is None: return name, None, None
    free, init = c['free'], c['init']
    best = None
    rng = np.random.default_rng(abs(hash(name)) % 2**32)
    lo, hi = bounds(free)
    starts = [pack(unpack(pack(dict(BASE, **init), free), free, init), free)]
    for _ in range(NSTART):
        starts.append(np.array([rng.uniform(a, b) for a, b in zip(lo, hi)]))
    for x0 in starts:
        x0 = np.clip(x0, np.array(lo)+1e-6, np.array(hi)-1e-6)
        try:
            res = least_squares(resid, x0, bounds=(lo, hi), args=(name, free, init), diff_step=1e-3, max_nfev=NFEV, x_scale='jac')
        except Exception as e:
            continue
        if best is None or res.cost < best.cost: best = res
    d = unpack(best.x, free, init)
    d = makeup(d, name)
    return name, d, float(best.cost)

if __name__ == '__main__':
    names = [n for n in ORDER if cfg(n) is not None] if len(sys.argv) < 2 else sys.argv[1:]
    with Pool(2) as p:
        out = p.map(fit_one, names, chunksize=1)
    try: rep = json.load(open('fit_params.json'))
    except Exception: rep = {}
    for n, d, cost in out:
        if d is not None: rep[n] = dict(params=d, cost=cost); print(f'{n:22s} cost {cost:8.2f}', flush=True)
    json.dump(rep, open('fit_params.json','w'), indent=1)
