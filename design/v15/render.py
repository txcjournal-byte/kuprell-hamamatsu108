import json, math, numpy as np
from scipy import ndimage as ndi
from skimage.draw import polygon as skpoly
from PIL import Image

import os
D = json.load(open(os.environ.get('LAYOUT', 'layout.json')))
STATIC = os.environ.get('STATIC') == '1'
SPRITE = os.environ.get('SPRITE') == '1'
W, H = D.get('canvas', [3200, 1840])
rng = np.random.default_rng(7)

Hm = np.zeros((H, W), np.float32)          # height
A = np.zeros((H, W, 3), np.float32)        # albedo
MT = np.zeros((H, W), np.float32)          # metalness 0..1
SP = np.full((H, W), 40.0, np.float32)     # spec power
SK = np.full((H, W), 0.3, np.float32)      # spec strength
BR = np.zeros((H, W), np.int8)             # brushing: 1 horiz, 2 vert
E = np.zeros((H, W, 3), np.float32)        # emission

def rect_mask(x, y, w, h):
    m = np.zeros((H, W), bool); m[max(y,0):y+h, max(x,0):x+w] = True; return m

def bevel_profile(mask, bev, rounded=True):
    d = ndi.distance_transform_edt(mask)
    t = np.clip(d / bev, 0, 1)
    return np.sqrt(1 - (1 - t) ** 2) if rounded else t

def setmat(m, alb, metal=0.0, sp=40, sk=0.3, br=0):
    A[m] = alb; MT[m] = metal; SP[m] = sp; SK[m] = sk; BR[m] = br

def rrect_sdf(x, y, w, h, r, pad=0):
    """local window sdf of rounded rect. returns (slice_y, slice_x, sdf)"""
    x0, y0, x1, y1 = max(int(x - pad), 0), max(int(y - pad), 0), min(int(x + w + pad), W), min(int(y + h + pad), H)
    x0, y0 = min(x0, W), min(y0, H); x1, y1 = max(x1, x0), max(y1, y0)
    yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32) + 0.5
    cx, cy, hw, hh = x + w / 2, y + h / 2, w / 2, h / 2
    qx = np.abs(xx - cx) - hw + r; qy = np.abs(yy - cy) - hh + r
    sdf = np.hypot(np.maximum(qx, 0), np.maximum(qy, 0)) + np.minimum(np.maximum(qx, qy), 0) - r
    return (slice(y0, y1), slice(x0, x1)), sdf, xx, yy

def raise_rrect(x, y, w, h, r, height, bev, base=None, alb=(.05,.05,.05), metal=0, sp=40, sk=.3, br=0, pillow=0.0, mode='set'):
    sl, sdf, xx, yy = rrect_sdf(x, y, w, h, r, 2)
    inside = sdf < 0
    t = np.clip(-sdf / bev, 0, 1); prof = np.sqrt(1 - (1 - t) ** 2)
    if pillow:
        nx = (xx - (x + w / 2)) / (w / 2); ny = (yy - (y + h / 2)) / (h / 2)
        prof = prof * (1 - pillow * (nx ** 2 + ny ** 2) * 0.5)
    b = Hm[sl] if base is None else np.full(sdf.shape, base, np.float32)
    newh = b + height * prof
    if mode == 'set':
        Hm[sl] = np.where(inside, newh if base is not None or height >= 0 else newh, Hm[sl])
    for arr, val in ((A, alb),):
        sub = arr[sl]; sub[inside] = val
    MT[sl][inside] = metal; SP[sl][inside] = sp; SK[sl][inside] = sk; BR[sl][inside] = br
    return sl, inside, sdf, xx, yy

# ---------------- background: dark cast concrete
noise = ndi.gaussian_filter(rng.standard_normal((H, W)).astype(np.float32), 1.2)
coarse = ndi.gaussian_filter(rng.standard_normal(((H + 7) // 8, (W + 7) // 8)).astype(np.float32), 3)
coarse = np.kron(coarse, np.ones((8, 8), np.float32))[:H, :W]
A[:] = (0.035 + 0.012 * coarse[..., None] + 0.01 * noise[..., None]) * np.array([1, 1.02, 1.0])
Hm[:] = 0.6 * noise + 2 * coarse
SP[:] = 8; SK[:] = 0.05

# ---------------- cheeks (angled side slabs)
for side in ((0, 1) if not SPRITE else ()):
    pts_x = np.array([0, 144, 144, 104, 104, 0], float); pts_y = np.array([0, 0, .61 * H, .79 * H, H, H])
    if side: pts_x = W - pts_x
    rr, cc = skpoly(pts_y, pts_x, (H, W)); m = np.zeros((H, W), bool); m[rr, cc] = True
    prof = bevel_profile(m, 22)
    Hm[m] = 70 * prof[m] + 0.8 * noise[m]
    setmat(m, (.08, .08, .085), metal=1, sp=60, sk=.5, br=2)

# ---------------- main panel: light brushed aluminium, tier B dark graphite inset
px, py, pw, ph = D['panel']
m = rect_mask(px, py, pw, ph); prof = bevel_profile(m, 10)
Hm[m] = 40 + 10 * prof[m]
setmat(m, (.05, .05, .053) if os.environ.get('DARK') == '1' else (.27, .27, .265), metal=1, sp=50, sk=.65, br=1)
tx, ty, tw, th = D['tierb']
m2 = rect_mask(tx + 4, ty, tw - 8, th - 4); prof2 = bevel_profile(m2, 8)
Hm[m2] = 44 + 4 * prof2[m2]
setmat(m2, (.05, .05, .053), metal=1, sp=70, sk=.55, br=1)

# title bars: recessed black glass strips
for (x, y, w, h) in D['tb']:
    sl, sdf, xx, yy = rrect_sdf(x, y, w, h, 3, 2); ins = sdf < 0
    Hm[sl][ins] = Hm[sl][ins] - 4 * np.clip(-sdf[ins] / 3, 0, 1)
    A[sl][ins] = (.012, .013, .013); MT[sl][ins] = 0; SP[sl][ins] = 300; SK[sl][ins] = .9; BR[sl][ins] = 0
# morph well
for (x, y, w, h) in D['sec']:
    sl, sdf, xx, yy = rrect_sdf(x, y + 44, w, h - 44, 4, 2); ins = sdf < 0
    Hm[sl][ins] -= 6 * np.clip(-sdf[ins] / 5, 0, 1)
    A[sl][ins] = (.05, .05, .052); MT[sl][ins] = 1; SK[sl][ins] = .5; BR[sl][ins] = 1

# ---------------- console: dark graphite slab, tilted towards the player
cx_, cy_, cw_, ch_ = D['console']
m = rect_mask(cx_, cy_, cw_, ch_); prof = bevel_profile(m, 16)
yy = np.arange(H, dtype=np.float32)[:, None].repeat(W, 1)
tilt = 70 - 26 * np.clip((yy - cy_) / ch_, 0, 1)
Hm[m] = (tilt * prof)[m] + 8
setmat(m, (.075, .075, .078), metal=1, sp=60, sk=.6, br=1)
# display recess
x, y, w, h = D['lcd'][1]
sl, sdf, xx, yy2 = rrect_sdf(x - 8, y - 8, w + 16, h + 16, 6, 2); ins = sdf < 0
Hm[sl][ins] = Hm[sl][ins] - 18 * np.clip(-sdf[ins] / 8, 0, 1)
A[sl][ins] = (.01, .01, .01); MT[sl][ins] = 0; SP[sl][ins] = 400; SK[sl][ins] = 1; BR[sl][ins] = 0
# logo plate: slightly raised black glass slab
x, y, w, h = D['np']
sl, inside, sdf, _, _ = raise_rrect(x - 18, y - 10, w + 30, h + 14, 6, 5, 4, alb=(.012, .012, .013), sp=300, sk=.9)

# ---------------- string bed: deep well with glass floor and real strings
bx, by, bw, bh = D['bed']
m = rect_mask(bx, by, bw, bh)
d = ndi.distance_transform_edt(m)
Hm[m] = -30 + 0 * d[m]
wall = m & (d < 14); Hm[wall] = (-30 + 30 * (1 - d / 14))[wall]
A[m] = (.004, .012, .009); MT[m] = 0; SP[m] = 200; SK[m] = .12; BR[m] = 0
# subtle phosphor floor glow
yy3, xx3 = np.mgrid[by:by + bh, bx:bx + bw]
E[by:by + bh, bx:bx + bw] += (np.exp(-((yy3 - by) / 220.0) ** 2) * 0.0)[..., None] * np.array([.3, 1, .7])
n = 36; chord = {1, 3, 5, 8, 10, 12, 15, 17, 19, 22, 24, 26}
TOP = [0,0,30,0,0,60,0,0,18,0,0,44,0,0,0,26,0,0,70,0,0,0,36,0,0,14,0,0,52,0,0,0,0,0,0,0]
BOT = [40,40,70,40,40,40,90,40,40,55,40,40,40,120,40,40,40,62,40,40,40,40,96,40,40,40,40,40,40,76,40,40,40,40,40,40]
MUTE = {7, 21}
for i in range(n if not (STATIC or SPRITE) else 0):
    sx = bx + 40 + (14 + i * (1150 / (n - 1))) * 2; th = (2.6 - 1.6 * i / n) * 2.2
    y0, y1 = by + 32, by + 304
    ha, hb = y0 + 8 + TOP[i], y1 - BOT[i]          # the two handles: bend (top) and pluck point (bottom)
    xs = np.arange(int(sx - th - 2), int(sx + th + 3)); r = np.abs(xs + .5 - sx) / th
    ok = r < 1; prof = np.sqrt(np.clip(1 - r ** 2, 0, 1))
    lit = i in chord and i not in MUTE
    for xi, pr in zip(xs[ok], prof[ok]):
        Hm[y0:y1, xi] = -14 + 10 * pr
        A[y0:y1, xi] = (.55, .55, .52) if i not in MUTE else (.18, .18, .17); MT[y0:y1, xi] = 1; SP[y0:y1, xi] = 200; SK[y0:y1, xi] = 1; BR[y0:y1, xi] = 0
        if lit: E[ha:hb, xi] += np.array([.25, 1.0, .68]) * (0.12 + 0.22 * pr)
    for py_ in (y0 - 6, y1 + 2):
        raise_rrect(sx - 7, py_, 14, 8, 3, 6, 3, base=-30, alb=(.5, .5, .48), metal=1, sp=120, sk=.9)
    if i not in MUTE:
        for hy in (ha, hb):   # handle caps riding on the string
            raise_rrect(sx - 11, hy - 7, 22, 14, 4, 22, 4, base=-20, alb=(.06, .06, .065), sp=120, sk=.5)
            A[hy - 1:hy + 2, int(sx) - 8:int(sx) + 9] = (.25, .9, .65) if lit else (.6, .6, .58)
            if lit: E[hy - 1:hy + 2, int(sx) - 8:int(sx) + 9] += np.array([.15, .6, .4])
    if lit:
        cyy = y1 + 16; yyl, xxl = np.mgrid[cyy - 8:cyy + 9, int(sx) - 8:int(sx) + 9]
        rr2 = np.hypot(xxl - sx, yyl - cyy) / 5.0; ok2 = rr2 < 1
        E[cyy - 8:cyy + 9, int(sx) - 8:int(sx) + 9][ok2] += np.array([.2, .7, .5])

# ---------------- faders
for f in D['ft']:
    x, y, w, h = f['r']
    # frame (the .gr), raised satin black
    gx, gw = x + 12, w - 24
    sl, ins, sdf, xx, yy = raise_rrect(gx, y, gw, h, 4, 7, 3, alb=(.03, .03, .032), sp=60, sk=.35)
    # slot
    sl, sdf, xx, yy = rrect_sdf(gx + 14, y + 10, gw - 28, h - 20, 4, 1); ins = sdf < 0
    Hm[sl][ins] = Hm[sl][ins] - 22 * np.clip(-sdf[ins] / 3, 0, 1); A[sl][ins] = (.004, .004, .004); SK[sl][ins] = .05
    if STATIC: continue
    # cap
    cx0, cy0, cw0, chh = f['cap']
    base = 48
    sl, ins, sdf, xx, yy = raise_rrect(cx0 - 2, cy0, cw0 + 4, chh, 5, 34, 6, base=base, alb=(.03, .03, .03), sp=90, sk=.45)
    rib = np.sin((yy - cy0) / 6.0 * 2 * math.pi) * 1.2
    centre = np.abs(yy - (cy0 + chh / 2)) < 2.2
    Hm[sl][ins] += np.where(centre, 0, rib)[ins]
    A[sl][ins & centre] = (.92, .91, .88)
    # cap drop shadow handled by shadow pass (height)

# ---------------- buttons
for b in D['btn']:
    if STATIC: b = dict(b, lit=False)
    x, y, w, h = b['r']
    big = not b['sm']
    sl, ins, sdf, xx, yy = raise_rrect(x - 2, y - 2, w + 4, h + 4, 7 if big else 4, 22 if big else 12, 6 if big else 4,
                                      alb=(.035, .035, .037), sp=25, sk=.06, pillow=.25)
    # fine rubber texture
    Hm[sl][ins] += 0.35 * noise[sl][ins]
    if b['led']:
        cx, cy = x + w / 2, y + h / 2
        yy2, xx2 = np.mgrid[int(cy) - 10:int(cy) + 11, int(cx) - 10:int(cx) + 11]
        r = np.hypot(xx2 + .5 - cx, yy2 + .5 - cy) / 6.5; ok = r < 1
        s2 = (slice(int(cy) - 10, int(cy) + 11), slice(int(cx) - 10, int(cx) + 11))
        Hm[s2][ok] += (2.5 * np.sqrt(1 - r[ok] ** 2))
        A[s2][ok] = (.2, .02, .02) if b['lit'] else (.12, .015, .015); MT[s2][ok] = 0; SP[s2][ok] = 400; SK[s2][ok] = 1
        if b['lit']: E[s2][ok] += np.array([1.6, .18, .15]) * (0.6 + 0.4 * np.sqrt(1 - r[ok] ** 2))[:, None]

# ---------------- LEDs (panel lists)
for l in D['led']:
    if STATIC: l = dict(l, on=False)
    x, y, w, h = l['r']; cx, cy = x + w / 2, y + h / 2
    R0 = 11
    sl = (slice(int(cy) - R0 - 2, int(cy) + R0 + 3), slice(int(cx) - R0 - 2, int(cx) + R0 + 3))
    yy2, xx2 = np.mgrid[sl[0], sl[1]]
    r = np.hypot(xx2 + .5 - cx, yy2 + .5 - cy)
    bez = r < R0; dome = r < 7.5
    Hm[sl][bez] += 4
    A[sl][bez] = (.02, .02, .02); MT[sl][bez] = 0; SK[sl][bez] = .5; SP[sl][bez] = 120
    Hm[sl][dome] += 3 * np.sqrt(np.clip(1 - (r[dome] / 7.5) ** 2, 0, 1))
    A[sl][dome] = (.22, .03, .03) if l['on'] else (.10, .015, .015); SP[sl][dome] = 500; SK[sl][dome] = 1
    if l['on']: E[sl][dome] += np.array([1.7, .2, .16]) * (0.55 + 0.45 * np.sqrt(np.clip(1 - (r[dome] / 7.5) ** 2, 0, 1)))[:, None]

# ---------------- knobs
for k in D['kn']:
    if STATIC: continue
    x, y, w, h = k['r']; cx, cy = x + w / 2, y + h / 2; R0 = w / 2 + 4
    rot = float(k['rot'].replace('rotate(', '').replace('deg)', '') or 0)
    sl = (slice(int(cy - R0 - 3), int(cy + R0 + 4)), slice(int(cx - R0 - 3), int(cx + R0 + 4)))
    yy2, xx2 = np.mgrid[sl[0], sl[1]].astype(np.float32)
    dx, dy = xx2 + .5 - cx, yy2 + .5 - cy; r = np.hypot(dx, dy); a = np.arctan2(dy, dx)
    knurl = 0.9 * np.clip(np.cos(a * 36) * 1.5, -1, 1)
    body = r < R0 + knurl * (r > R0 - 6)
    top = r < R0 * 0.68
    Hm[sl][body] = 48 + 40 * np.clip((R0 - r[body]) / 3, 0, 1)
    A[sl][body] = (.02, .02, .022); MT[sl][body] = 0; SP[sl][body] = 140; SK[sl][body] = .5
    Hm[sl][top] = 90 + 2 * (1 - (r[top] / (R0 * .68)) ** 2) + 0.25 * np.sin(r[top] * 1.9)
    A[sl][top] = (.42, .42, .41); MT[sl][top] = 1; SP[sl][top] = 80; SK[sl][top] = .75
    ang = math.radians(rot - 90)
    px_ = dx * math.cos(ang) + dy * math.sin(ang); py_ = -dx * math.sin(ang) + dy * math.cos(ang)
    ptr = (np.abs(py_) < 3.0) & (px_ > R0 * .12) & (px_ < R0 * .64)
    Hm[sl][ptr] -= 2.5; A[sl][ptr] = (.01, .01, .01); MT[sl][ptr] = 0; SK[sl][ptr] = 0.05

# arrows + horizontal slider
for (x, y, w, h) in D['arr']:
    raise_rrect(x, y, w, h, 6, 24, 6, alb=(.035, .035, .037), sp=25, sk=.06, pillow=.25)
for hz in D['hz']:
    x, y, w, h = hz['r']
    sl, sdf, xx, yy = rrect_sdf(x, y, w, h, 4, 1); ins = sdf < 0
    Hm[sl][ins] -= 12 * np.clip(-sdf[ins] / 3, 0, 1); A[sl][ins] = (.005, .005, .005)
    if STATIC: continue
    x, y, w, h = hz['b']
    sl, ins, sdf, xx, yy = raise_rrect(x, y, w, h, 4, 30, 5, alb=(.03, .03, .03), sp=90, sk=.45)
    c = np.abs(xx - (x + w / 2)) < 2; A[sl][ins & c] = (.92, .91, .88)

# ======================= shading
N = 1.0
gy, gx = np.gradient(Hm)
# brushed micro normals
brush_h = ndi.gaussian_filter(rng.standard_normal((H, W)).astype(np.float32), (0.5, 60)) * 1.0
brush_v = ndi.gaussian_filter(rng.standard_normal((H, W)).astype(np.float32), (60, 0.5)) * 1.0
gy = gy + np.where(BR == 1, brush_h, 0); gx = gx + np.where(BR == 2, brush_v, 0)
nx, ny, nz = -gx, -gy, np.ones_like(gx) * 1.0
ln = np.sqrt(nx ** 2 + ny ** 2 + nz ** 2); nx /= ln; ny /= ln; nz /= ln

L = np.array([-0.42, -0.55, 0.72]); L /= np.linalg.norm(L)
# heightfield soft shadows
sh = np.ones((H, W), np.float32)
ys, xs = np.mgrid[0:H, 0:W]
for t in list(range(2, 30, 2)) + list(range(30, 200, 8)):
    dx = int(round(L[0] / L[2] * t * L[2] / np.hypot(L[0], L[1]) * np.hypot(L[0], L[1]) / L[2] * L[2] * 0 + L[0] * t / math.hypot(L[0], L[1])))
    dy = int(round(L[1] * t / math.hypot(L[0], L[1])))
    rise = t * L[2] / math.hypot(L[0], L[1])
    sx = np.clip(xs + dx, 0, W - 1); sy = np.clip(ys + dy, 0, H - 1)
    occ = Hm[sy, sx] - (Hm + rise)
    sh = np.minimum(sh, np.clip(1 - occ / (0.6 * t + 2), 0, 1))
sh = ndi.gaussian_filter(sh, 1.2)
# ambient occlusion from heightfield
ao = np.clip(1 + (Hm - ndi.gaussian_filter(Hm, 10)) / 20, 0.35, 1.0) * np.clip(1 + (Hm - ndi.gaussian_filter(Hm, 40)) / 60, 0.5, 1.0)

ndl = np.clip(nx * L[0] + ny * L[1] + nz * L[2], 0, 1)
hvx, hvy, hvz = L[0], L[1], L[2] + 1; hl = math.sqrt(hvx ** 2 + hvy ** 2 + hvz ** 2); hvx /= hl; hvy /= hl; hvz /= hl
ndh = np.clip(nx * hvx + ny * hvy + nz * hvz, 0, 1)
spec = ndh ** SP * (SP + 8) / 60
# environment: studio softbox above-left + soft wide fill, read by the reflected vector
rx, ry, rz = 2 * nz * nx, 2 * nz * ny, 2 * nz * nz - 1
yy_n = ys / H; xx_n = xs / W
envv = 0.10 + 0.25 * np.clip(rz, 0, 1)
envv = envv + 1.6 * np.exp(-((rx + 0.30) ** 2 + (ry + 0.38) ** 2) / 0.05)
# large diagonal sheen across the metal, as if a window reflects in it
sheen = np.exp(-(((xx_n * 0.75 + yy_n) - 0.55) / 0.22) ** 2) * 0.35 + np.exp(-(((xx_n * 0.75 + yy_n) - 1.25) / 0.10) ** 2) * 0.15
envv = envv + sheen
mint_floor = np.exp(-((ry - 0.6) ** 2) / 0.05) * 0.25
ndv = np.clip(nz, 0, 1); fres = 0.04 + 0.96 * (1 - ndv) ** 5

key = np.array([1.0, .97, .93]) * 2.4
amb = np.array([.55, .6, .62]) * 0.35
diff = A * (ndl * sh)[..., None] * key + A * ao[..., None] * amb
metal = MT[..., None]
env_col = envv[..., None] * np.array([1, .99, .97]) + mint_floor[..., None] * np.array([.3, 1, .7])
refl_metal = A * env_col * SK[..., None] * 1.15 * (0.35 + 0.65 * ao[..., None])
refl_diel = env_col * (fres * 0.6 + 0.05 * SK)[..., None] * SK[..., None] * ao[..., None] * 1.2
spec_c = (spec * sh * SK)[..., None] * np.where(metal > .5, A * 3 + .2, 1.0) * 1.4
col = metal * (diff * 0.35 + refl_metal) + (1 - metal) * (diff + refl_diel) + spec_c
col = col + E
# bloom of emissive
bl = ndi.gaussian_filter(E, (5, 5, 0)) * 0.45 + ndi.gaussian_filter(E, (22, 22, 0)) * 0.15
col = col + bl
# mint haze from the string bed upward
haze = np.exp(-np.clip(by - ys, 0, None) / 160.0) * (ys < by) * 0.0
col += haze[..., None] * np.array([.3, 1, .7])
# vignette
vig = 1 - (0 if SPRITE else 0.35) * (((xx_n - .5) * 1.2) ** 2 + ((yy_n - .45) * 1.3) ** 2)
col *= vig[..., None]
# filmic
col = col * 1.1
col = (col * (2.51 * col + 0.03)) / (col * (2.43 * col + 0.59) + 0.14)
col = np.clip(col, 0, 1) ** (1 / 2.2)
grain = rng.standard_normal((H, W, 1)).astype(np.float32) * (0 if SPRITE else 0.008)
col = np.clip(col + grain, 0, 1)
Image.fromarray((col * 255).astype(np.uint8)).save(os.environ.get('OUT', 'base.png'))
print('done')
