#!/usr/bin/env python3
"""Grid-searches the four kRidges phases (desert/src/world.cpp) so the
--terrain-report constraints hold, with a Python mirror of duneHeight.
Usage: python3 tune_phases.py [steps]   (default 10 phase values per layer)
Prints the feasible phase sets best first: palm slope, rise behind, clearance.
The result is checked afterwards by the real `pxe_desert --terrain-report`."""
import math, itertools, sys

TAU = 2 * math.pi
# heading, wavelength, height, windward, meander, meanderLength, swellLength, seed
L = [(200, 110, 6.5, .75, .10, 150, 190, 0), (214, 48, 2.4, .75, .12, 90, 110, 1),
     (186, 26, .8, .75, .15, 60, 70, 2), (196, 260, 5.0, .75, .08, 400, 500, 3)]

def layer_h(l, ph, x, z):
    hd, lam, H, k, m, ml, sl, seed = l
    w = (math.sin(math.radians(hd)), math.cos(math.radians(hd)))
    c = (w[1], -w[0])
    a = x * w[0] + z * w[1]; s = x * c[0] + z * c[1]
    u = a / lam + m * math.sin(TAU * s / ml + 1.3 * seed + .4) + ph
    u -= math.floor(u)
    sw = .75 + .25 * math.sin(TAU * s / sl + 2.1 * seed + 1)
    if u < k:
        t = u / k; p = t * t * (1.5 - .5 * t)
    else:
        v = 1 - (u - k) / (1 - k); p = v * v
    return H * sw * p

def height(phs, x, z):
    return sum(layer_h(l, p, x, z) for l, p in zip(L, phs))

def ground(phs, x, z):  # the drawn 0.5 m grid, as groundHeight
    cell = .5; half = 120.
    gx = (x + half) / cell; gz = (z + half) / cell
    i = math.floor(gx); j = math.floor(gz); fx = gx - i; fz = gz - j
    c = lambda di, dj: height(phs, -half + (i + di) * cell, -half + (j + dj) * cell)
    h00 = c(0, 0); h11 = c(1, 1)
    if fx >= fz:
        h10 = c(1, 0); return h00 + fx * (h10 - h00) + fz * (h11 - h10)
    h01 = c(0, 1); return h00 + fz * (h01 - h00) + fx * (h11 - h01)

def grad(phs, x, z, e=1e-3):
    return ((height(phs, x + e, z) - height(phs, x - e, z)) / (2 * e),
            (height(phs, x, z + e) - height(phs, x, z - e)) / (2 * e))

def check(phs):
    gx, gz = grad(phs, 0, 0)
    slope = math.degrees(math.atan(math.hypot(gx, gz)))
    if slope > 15 or -gz > 0: return None
    base = ground(phs, 0, 0)
    rise = max(ground(phs, 0, 5 + .25 * i) for i in range(221)) - base
    if rise < 2: return None
    eye = ground(phs, 0, -25) + 1.7
    clear = min(eye + (base + .5 - eye) * (k / 100) - ground(phs, 0, -25 + .25 * k) for k in range(101))
    if clear < .1: return None
    return slope, rise, clear

if __name__ == "__main__":
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 10
    vals = [i / n for i in range(n)]
    out = []
    for phs in itertools.product(vals, repeat=4):
        r = check(phs)
        if r: out.append((phs, r))
    print(len(out), "feasible of", n ** 4)
    out.sort(key=lambda o: -o[1][1])
    for phs, (s, r, c) in out[:30]:
        print(phs, "slope %.1f rise %.2f clear %.2f" % (s, r, c))
