set -e
python3 gen.py
sed 's#</body>#<script src="dump.js"></script></body>#' v6.html > dump.html
timeout 60 google-chrome --headless=new --no-sandbox --disable-gpu --window-size=1600,920 --virtual-time-budget=3000 --dump-dom file:///home/claude/mock6/dump.html 2>/dev/null > dom.txt
python3 -c "
import re,json,html
t=open('dom.txt').read(); m=re.search(r'<pre id=\"J\">(.*?)</pre>',t,re.S); d=json.loads(html.unescape(m.group(1))); json.dump(d,open('layout.json','w'))"
sed 's#</head>#<link rel="stylesheet" href="overlay.css"></head>#' v6.html > ov.html
timeout 90 google-chrome --headless=new --no-sandbox --disable-gpu --hide-scrollbars --default-background-color=00000000 --force-device-scale-factor=2 --window-size=1600,920 --screenshot=ov.png file:///home/claude/mock6/ov.html 2>/dev/null
python3 render.py
python3 -c "
from PIL import Image
b=Image.open('base.png').convert('RGBA'); o=Image.open('ov.png').convert('RGBA')
Image.alpha_composite(b,o).convert('RGB').save('$1')"
