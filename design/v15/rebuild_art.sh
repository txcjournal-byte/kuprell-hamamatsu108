set -e
python3 gen.py
sed 's#</body>#<script src="dump2.js"></script></body>#' v6.html > dump2.html
timeout 60 google-chrome --headless=new --no-sandbox --disable-gpu --window-size=1600,920 --virtual-time-budget=3000 --dump-dom file://$PWD/dump2.html 2>/dev/null > dom2.txt
python3 -c "
import re,json,html
t=open('dom2.txt').read(); m=re.search(r'<pre id=\"J\">(.*?)</pre>',t,re.S); d=json.loads(html.unescape(m.group(1))); json.dump(d,open('controls.json','w'))"
sed 's#</body>#<script src="dump.js"></script></body>#' v6.html > dump.html
timeout 60 google-chrome --headless=new --no-sandbox --disable-gpu --window-size=1600,920 --virtual-time-budget=3000 --dump-dom file://$PWD/dump.html 2>/dev/null > dom.txt
python3 -c "
import re,json,html
t=open('dom.txt').read(); m=re.search(r'<pre id=\"J\">(.*?)</pre>',t,re.S); d=json.loads(html.unescape(m.group(1))); json.dump(d,open('layout.json','w'))"
sed 's#</head>#<link rel="stylesheet" href="overlay.css"><link rel="stylesheet" href="overlay_static.css"></head>#' v6.html > ovs.html
timeout 90 google-chrome --headless=new --no-sandbox --disable-gpu --hide-scrollbars --default-background-color=00000000 --force-device-scale-factor=2 --window-size=1600,920 --screenshot=ovs.png file://$PWD/ovs.html 2>/dev/null
STATIC=1 OUT=static.png python3 render.py >/dev/null
A=/home/claude/kuprell-hamamatsu108/plugins/hamamatsu108/art
python3 -c "
from PIL import Image
b=Image.open('static.png').convert('RGBA'); o=Image.open('ovs.png').convert('RGBA')
Image.alpha_composite(b,o).convert('RGB').save('$A/console_bg.jpg', quality=95, subsampling=0)"
python3 - <<'PY'
import json
d=json.load(open('controls.json'))['ctl']
def esc(s): return s.replace('\\','\\\\').replace('"','\\"').replace('\u2212','-')
out=['#pragma once','// generated from design/v15 (controls.json): every control of the one-page console, in 1600 x 920 design pixels','namespace hama::console','{',
'    struct Item { const char* text; float x, y, w, h; };',
'    struct Def { const char* kind; const char* section; const char* label; float x, y, w, h; bool bipolar; int numItems; Item items[6]; };',
'    inline const Def defs[] = {']
for c in d:
    items=c.get('items',[])
    if 'r' in c: x,y,w,h=c['r']
    else:
        xs=[i['r'][0] for i in items]; ys=[i['r'][1] for i in items]
        x,y=min(xs),min(ys); w=max(i['r'][0]+i['r'][2] for i in items)-x; h=max(i['r'][1]+i['r'][3] for i in items)-y
    its=', '.join('{ "%s", %d, %d, %d, %d }'%(esc(i['t']),*i['r']) for i in items) or '{ "", 0, 0, 0, 0 }'
    out.append('        { "%s", "%s", "%s", %d, %d, %d, %d, %s, %d, { %s } },'%(c['k'],esc(c['s']),esc(c['l']),x,y,w,h,'true' if c.get('bip') else 'false',len(items),its))
out+=['    };','    inline constexpr int numDefs = (int) (sizeof (defs) / sizeof (defs[0]));','}','']
open('/home/claude/kuprell-hamamatsu108/plugins/hamamatsu108/source/ConsoleLayout.h','w').write('\n'.join(out))
PY
echo art-done
