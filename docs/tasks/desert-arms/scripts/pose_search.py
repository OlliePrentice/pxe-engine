"""One-off pose search for desert-arms (not part of the build).

With real proportions (shoulders 0.38 m apart, 0.22 below and 0.05 behind the
eye, upper arm 0.30, forearm 0.26), searches wrist positions, elbow swings
and wrist bends for poses whose index knuckle is about 20% up the view, whose
hand centre is a quarter of the width in, whose forearm leaves through the
bottom edge with 0.17 m near clearance, and ranks them by how high on screen
the bracer's gauge (on the forearm 9.6 cm behind the wrist) can sit. It showed
that with a natural wrist bend (40 degrees or less) the gauge can't sit
much above the bottom edge, which is why build_arms.py shortens the glove
cuff and puts the gauge at the bracer's wrist end. Approximate: the real
numbers are build_arms.py's checks.

    /Applications/Blender.app/Contents/MacOS/Blender -b -P docs/tasks/desert-arms/scripts/pose_search.py
(Blender's Python has numpy.)
"""
import math, itertools
import numpy as np
S = np.array([0.19, 0.05, -0.22]); U, F = 0.30, 0.26
TAN = math.tan(math.radians(30)); AS = 16/9
def ndc(p):
    d = -p[1]
    return (-p[0]/(d*TAN*AS), p[2]/(d*TAN), d) if d > 1e-6 else None
def elbow(W, sw):
    d = W - S; L = np.linalg.norm(d); d /= L
    a = (U*U - F*F + L*L)/(2*L); r2 = U*U - a*a
    if r2 < 0: return None
    r = math.sqrt(r2)
    down = np.array([0,0,-1.]) - d*np.dot([0,0,-1.], d); down /= np.linalg.norm(down)
    out = np.cross(d, down); out = out if out[0] > 0 else -out
    s = math.radians(sw)
    return S + d*a + (down*math.cos(s) + out*math.sin(s))*r
res = []
for wx, wd, wz, sw, flex in itertools.product(np.arange(0.17, 0.25, 0.01), np.arange(0.30, 0.40, 0.01), np.arange(-0.15, -0.09, 0.005), range(0, 75, 5), (25, 30, 35, 40)):
    W = np.array([wx, -wd, wz]); E = elbow(W, sw)
    if E is None or E[2] > -0.25 or E[0] > 0.32: continue
    af = (W - E)/np.linalg.norm(W - E)
    fp = math.degrees(math.asin(af[2]))
    hp = math.radians(fp - flex)
    yaw = math.radians(4)
    f = np.array([-math.sin(yaw)*math.cos(hp), -math.cos(yaw)*math.cos(hp), math.sin(hp)])
    K = W + f*0.084 + np.array([0,0,1.0])*0.012   # knuckle top, roughly
    kq = ndc(K)
    if not (-0.66 <= kq[1] <= -0.54): continue
    # gauge: centre s=-0.08, half-length 0.016 across-forearm layout -> elbow end s=-0.096; top 5 cm toward screen-up
    c = W - af*0.096
    Np = np.array([0, -0.577, 1.0]); Np = Np - af*np.dot(Np, af); Np /= np.linalg.norm(Np)
    g = ndc(c + Np*0.048)[1]
    # hand centre x approx: wrist + f*0.06
    hc = ndc(W + f*0.06)[0]
    if not (0.42 <= -hc <= 0.58): continue
    nearest = 9
    for t in np.linspace(0, 1, 30):
        for A, B, r in ((E, W, 0.05), (S, E, 0.056)):
            cc = A + (B - A)*t
            ax = (B - A)/np.linalg.norm(B - A)
            u = np.cross(ax, [1,0,0]); u /= np.linalg.norm(u); v = np.cross(ax, u)
            for k in range(12):
                gg = 2*math.pi*k/12
                q = ndc(cc + (u*math.cos(gg) + v*math.sin(gg))*r)
                if q and abs(q[0]) <= 1 and abs(q[1]) <= 1: nearest = min(nearest, q[2])
    if nearest < 0.17: continue
    ex = None
    for t in np.linspace(0, 1, 200):
        q = ndc(W + (E - W)*t)
        if q is None or abs(q[0]) > 1 or abs(q[1]) > 1: ex = q; break
    if ex is None or ex[1] >= -1 or abs(ex[0]) > 1: continue
    res.append((round(g,3), round(wx,3), round(wd,3), round(wz,3), sw, flex, E.round(3), "fp", round(fp,1), "wristndc", [round(x,2) for x in ndc(W)[:2]], "k", round(kq[1],2), "hc", round(hc,2), "near", round(nearest,3), "exit", round(ex[0],2)))
res.sort(key=lambda r: -r[0])
for r in res[:20]: print(r)
print(len(res))
