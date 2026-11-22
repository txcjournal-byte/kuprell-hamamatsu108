import json, os, subprocess, numpy as np
from PIL import Image
OFF = [9000, 9000, 1, 1]
def layout(w, h, **kw):
    d = dict(canvas=[w, h], panel=[0, 0, w, h], tierb=OFF, console=OFF, bed=OFF, lcd=[OFF, OFF], np=OFF,
             tb=[], sec=[], ft=[], btn=[], led=[], kn=[], arr=[], hz=[])
    d.update(kw); return d
def render(d, out, dark=False):
    json.dump(d, open('/tmp/sp_layout.json', 'w'))
    env = dict(os.environ, LAYOUT='/tmp/sp_layout.json', OUT=out, SPRITE='1', DARK='1' if dark else '0')
    subprocess.run(['python3', 'render.py'], env=env, check=True, stdout=subprocess.DEVNULL)
    return np.asarray(Image.open(out).convert('RGB')).astype(np.float32) / 255.0
def sprite(with_, without, objmask):
    # object pixels: opaque colour; elsewhere: black with the darkening as alpha (shadow)
    lw = with_.mean(2); lo = np.maximum(without.mean(2), 1e-3)
    shadow = np.clip(1 - lw / lo, 0, 1)
    a = np.where(objmask, 1.0, shadow)
    rgb = np.where(objmask[..., None], with_, 0.0)
    return Image.fromarray((np.dstack([rgb, a[..., None]]) * 255).astype(np.uint8), 'RGBA')

os.makedirs('sprites', exist_ok=True)
# fader cap (same cap on the light and the dark panel: the shadow is alpha)
W, H = 64, 96
cap = [8, 30, 48, 36]
ft = dict(r=[2, -300, 60, 700], cap=cap, bip=False)
for dark in (0, 1):
    a = render(layout(W, H, ft=[ft]), '/tmp/a.png', dark)
    b = render(layout(W, H, ft=[dict(ft, cap=[9000, 9000, 1, 1])]), '/tmp/b.png', dark)
    m = np.zeros((H, W), bool); m[cap[1]:cap[1] + cap[3], cap[0] - 2:cap[0] + cap[2] + 2] = True
    sprite(a, b, m).save(f'sprites/fadercap{"_dark" if dark else ""}.png')
# knob filmstrip, 61 frames, -135..+135
W = H = 84; frames = []
base = render(layout(W, H), '/tmp/b.png', True)
for i in range(61):
    rot = -135 + 270 * i / 60
    a = render(layout(W, H, kn=[dict(r=[12, 12, 60, 60], rot=f'rotate({rot}deg)')]), '/tmp/a.png', True)
    yy, xx = np.mgrid[0:H, 0:W]; m = np.hypot(xx - 42, yy - 42) < 35
    frames.append(sprite(a, base, m))
strip = Image.new('RGBA', (W, H * len(frames)))
for i, f in enumerate(frames): strip.paste(f, (0, i * H))
strip.save('sprites/knob_strip.png')
print('ok')
