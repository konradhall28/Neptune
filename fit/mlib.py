import ctypes, numpy as np, re, os
lib = ctypes.CDLL(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'libmeasure.so'))
lib.measure.argtypes = [ctypes.POINTER(ctypes.c_double), ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.c_double, ctypes.POINTER(ctypes.c_double)]
FIELDS = ['kind','g0','g100','gamma','knee','b0','b100','preHp','prePkF','prePkDb','prePkQ','postPkF','postPkDb','postPkQ','postLp','postLpOrder','s0','s100','knee2','c0','c50','c100']
FS = 192000.0
lib.setWarm.argtypes=[ctypes.c_double]
def set_warm(x): lib.setWarm(x)
def measure(d, drive, freq=1000.0, level=-6.0, fs=FS):
    arr = (ctypes.c_double*22)(*[float(d.get(k, 2.0 if k=='knee2' else 0.0)) for k in FIELDS])
    out = (ctypes.c_double*16)()
    lib.measure(arr, drive, freq, level, fs, out)
    o = list(out)
    return {'fund': o[0], 'H': {h: o[h-1] for h in range(2,16)}, 'thd': o[15]}

def load_targets(path='/tmp/measure.md'):
    txt = open(path).read()
    sec = txt[txt.index('## 4. Saturn 2'):txt.index('## 5. Pro-DS')]
    T = {}; F = {}
    for line in sec.splitlines():
        c = [x.strip() for x in line.strip().strip('|').split('|')]
        if len(c) == 12 and re.match(r'^-?\d', c[1] or 'x'):
            st = c[0]; dr = float(c[1])/100
            T.setdefault(st, {})[dr] = dict(fund=float(c[4]), thd=float(c[5]), H={2:float(c[6]),3:float(c[7]),4:float(c[8]),5:float(c[9]),7:float(c[10])})
        elif len(c) == 7 and re.match(r'^-?\d', c[1] or 'x') and c[0] in T:
            F[c[0]] = dict(g100=float(c[1]), t100=float(c[2]), g1k=float(c[3]), t1k=float(c[4]), g5k=float(c[5]), t5k=float(c[6]))
    return T, F
