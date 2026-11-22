import math
def fader(label, v, h=150, bip=False, w=30):
    top = (1-v)*(h-18)
    sc = '<i class="pm">+</i><i class="pz">0</i><i class="pn">&minus;</i>' if bip else ''
    return (f'<div class="f" style="width:{w}px"><div class="lb">{label}</div><div class="ft" style="height:{h}px">'
            f'<i class="sc"></i><i class="gr"></i>{sc}<b class="cap" style="top:{top:.0f}px"></b></div></div>')
def leds(items, on, icons=None):
    out=''
    for i,t in enumerate(items):
        ic = f'<svg viewBox="0 0 20 10"><path d="{icons[i]}"/></svg>' if icons else ''
        out += f'<div><i class="{"on" if (i==on or (isinstance(on,set) and i in on)) else ""}"></i>{ic}<span>{t}</span></div>'
    return f'<div class="ll">{out}</div>'
def btn(label='', lit=False, sm=False):
    return f'<div class="b{" lit" if lit else ""}{" sm" if sm else ""}"><i></i></div>' + (f'<div class="bl">{label}</div>' if label else '')
def bcol(label, lit=False):
    return f'<div class="bc"><div class="lb">{label}</div>{btn(lit=lit)}</div>'
def knob(label, v, size=40):
    a=-135+270*v
    return (f'<div class="kc"><div class="lb">{label}</div><div class="kn" style="width:{size}px;height:{size}px">'
            f'<b style="transform:rotate({a}deg)"></b></div><div class="kmm"><span>&minus;</span><span>+</span></div></div>')
def mk(label, v):
    a=-135+270*v
    return f'<div class="mk"><div class="kn" style="width:30px;height:30px"><b style="transform:rotate({a}deg)"></b></div><span>{label}</span></div>'
def mb(label, lit=False):
    return f'<div class="mk"><div class="b{" lit" if lit else ""}" style="width:34px;height:22px"><i></i></div><span>{label}</span></div>'
def sub(*items):
    return '<div class="sub">' + ''.join(items) + '</div>'
W = ["M0 5 Q2.5 0 5 5 T10 5 T15 5 T20 5","M0 9 L5 1 L10 9 L15 1 L20 9","M0 9 L10 1 L10 9 L20 1 L20 9","M0 9 V1 H10 V9 H20 V1","M0 6 H4 V2 H8 V8 H12 V4 H16 V7 H20"]
def lfo(n, wave, off, trig, vals, sync, fh=150):
    return (f'<div class="sec" style="width:250px"><div class="tb"><div class="sy">{btn(sm=True, lit=sync)}<span>SYNC</span></div><h2>LFO {n}</h2></div><div class="row">'
            + fader('Rate',vals[0],fh) + fader('Delay',vals[1],fh) + fader('Fade',vals[2],fh) +
            f'<div class="cl"><div class="lb">Waveform</div>{leds(["","","","",""],wave,W)}{btn()}</div>'
            f'<div class="cl"><div class="lb">Offset</div>{leds(["+","0","&minus;"],off)}{btn()}</div>'
            f'<div class="cl"><div class="lb">Key<br>trig</div>{leds(["On"],0 if trig else -1)}{btn()}</div></div></div>')
def env(points, labels, keyoff_i, w, h=46):
    # points: list of (x,y) in 0..1
    xs=[p[0]*(w-16)+8 for p in points]; ys=[(1-p[1])*(h-14)+8 for p in points]
    path=' '.join(f'{"M" if i==0 else "L"}{x:.1f} {y:.1f}' for i,(x,y) in enumerate(zip(xs,ys)))
    marks=''.join(f'<line x1="{x:.1f}" y1="{h-4}" x2="{x:.1f}" y2="{y:.1f}" class="dl"/><text x="{x:.1f}" y="{h+8}" class="tl">{l}</text>' for x,y,l in zip(xs,ys,labels))
    ko=f'<text x="{xs[keyoff_i]:.1f}" y="6" class="ko">KEY OFF</text><line x1="{xs[keyoff_i]:.1f}" y1="8" x2="{xs[keyoff_i]:.1f}" y2="{h-4}" class="kl"/>'
    return (f'<svg class="env" width="{w}" height="{h+12}" viewBox="0 0 {w} {h+12}"><text x="{xs[0]:.1f}" y="6" class="ko" text-anchor="start" style="text-anchor:start">KEY ON</text>'
            f'<line x1="4" y1="{h-4}" x2="{w-4}" y2="{h-4}" class="ax"/>{ko}<path d="{path}" class="ep"/>{marks}</svg>')

S={}
# ---------- tier A
S['voice'] = ('<div class="sec" style="width:116px"><div class="tb"><h2>Voice</h2></div><div class="vg">'
  + ''.join(f'<div class="vk"><span>{t}</span>{btn(lit=l)}</div>' for t,l in [("Harp A",1),("Harp B",1),("Bass",0),("Chord",1)])
  + '</div><div class="tb2">Active</div><div class="vg">'
  + ''.join(f'<div class="vk"><span>{t}</span>{btn(lit=l)}</div>' for t,l in [("Harp A",1),("Harp B",0),("Bass",0),("Chord",0)])
  + '</div></div>')
S['morph'] = ('<div class="sec dark" style="width:140px"><div class="tb"><h2>Morph</h2></div><div class="row" style="justify-content:center">'
  + fader('Glass',.7,172) + fader('Steel',.35,172) + fader('Wood',.55,172) + fader('Air',.2,172) + '</div></div>')
S['lfo1'] = lfo(1,0,1,True,(.62,.3,.45),True)
pe = env([(0,0),(.08,1),(.3,.55),(.62,.42),(1,0)],["L0","T1","T2","T3","T4"],3,232)
S['pluck'] = ('<div class="sec" style="width:346px"><div class="tb"><h2>Pluck env</h2></div><div class="row">'
  + '<div class="grp"><div class="row">' + fader('Velo',.7,140,True,28) + fader('Hard',.6,140,False,28) + fader('Pos',.35,140,False,28) + '</div></div>'
  + f'<div class="grp">{pe}<div class="row">' + ''.join(fader(n,v,112,False,29) for n,v in [('T1',.15),('L1',.9),('T2',.4),('L2',.55),('T3',.6),('L3',.45),('T4',.7),('Tail',.5)]) + '</div></div></div></div>')
be = '<svg class="env" width="150" height="58" viewBox="0 0 150 58"><line x1="4" y1="50" x2="146" y2="50" class="ax"/><path class="ep" d="M6 48 C20 46 24 12 30 12 C36 12 38 40 46 40 C56 40 60 20 66 20 C72 20 74 44 84 44 C96 44 98 28 104 28 C110 28 114 46 144 47"/><text x="30" y="7" class="ko">BODY</text><text x="104" y="22" class="ko">RING</text></svg>'
S['body'] = ('<div class="sec" style="width:262px"><div class="tb"><h2>Body</h2></div><div class="row">'
  + f'<div class="cl"><div class="lb">Material</div>{leds(["Wood","Glass","Steel","Stone"],1)}{btn()}</div>'
  + f'<div class="grp">{be}<div class="row">' + fader('Size',.6,112) + fader('Damp',.3,112) + fader('Ring',.7,112) + fader('Mix',.5,112) + fader('Tilt',.5,112,True) + '</div></div></div></div>')
ae = env([(0,0),(.1,1),(.32,.62),(.66,.62),(1,0)],["A","D","S","","R"],3,190)
S['amp'] = ('<div class="sec" style="width:300px"><div class="tb"><h2>Amp env</h2></div><div class="row">'
  + fader('Level',.8,140,False,30) + fader('Velo',.6,140,True,30)
  + f'<div class="grp">{ae}<div class="row">' + ''.join(fader(n,v,112,False,32) for n,v in [('Atk',.1),('Dec',.45),('Sus',.62),('Hold',.3),('Rel',.5)]) + '</div></div></div></div>')
# ---------- tier B
S['hand'] = ('<div class="sec" style="width:270px"><div class="tb"><h2>Hand</h2></div><div class="row">'
  + '<div class="cl">' + ''.join(f'<div class="hk"><span>{t}</span>{btn(lit=l)}</div>' for t,l in [("Arp",1),("Gate",0),("Flourish",0),("Notes",1)]) + '</div>'
  + f'<div class="cl"><div class="lb">Direction</div>{leds(["Up","Down","Both","Random"],0)}{btn()}</div>'
  + fader('Speed',.66,150,False,34) + fader('Swing',.3,150,False,34) + fader('Feel',.5,150,True,32) + fader('Human',.25,150,False,34) + '</div></div>')
S['lfo2'] = lfo(2,3,0,False,(.4,.15,.7),False)
S['string'] = ('<div class="sec" style="width:352px"><div class="tb"><h2>String</h2></div><div class="row">'
  + f'<div class="cl" style="width:100px"><div class="lb">Exciter</div><div class="lcd">05: Nail Glass</div><div class="row" style="gap:10px;margin-top:6px">' + knob('Coarse',.5,34) + knob('Fine',.55,34) + '</div></div>'
  + fader('Tone',.65,150) + fader('Stiff',.4,150) + fader('Detune',.5,150,True,36) + fader('Width',.7,150,False,34)
  + f'<div class="cl"><div class="lb">Mod</div>{leds(["LFO 1","LFO 2","Env"],{0,2})}{btn()}</div>' + fader('Amt',.6,150,True) + '</div></div>')
S['filter'] = ('<div class="sec" style="width:262px"><div class="tb"><h2>Filter</h2></div><div class="row">'
  + f'<div class="cl"><div class="lb">Mode</div>{leds(["HPF","BPF","LPF"],2)}{btn()}</div>'
  + fader('Freq',.64,150) + fader('Reso',.3,150) + fader('Env',.6,150,True) + fader('Key',.5,150)
  + f'<div class="cl"><div class="lb">LFO sel</div>{leds(["LFO 1","LFO 2"],1)}{btn()}</div>' + '</div></div>')
S['space'] = ('<div class="sec" style="width:290px"><div class="tb"><h2>Space</h2></div><div class="row">'
  + f'<div class="cl"><div class="lb">Room</div>{leds(["Hall","Plate","Cave","Void"],3)}{btn()}</div>'
  + fader('Size',.75,150) + fader('Tone',.45,150) + fader('Pre',.2,150) + fader('Mix',.38,150)
  + '<div class="cl">' + ''.join(f'<div class="hk"><span>{t}</span>{btn(lit=l)}</div>' for t,l in [("Freeze",0),("Shimmer",1),("Reverse",0)]) + '</div></div></div>')

# strum strings
notes=['C','D','E','F','G','A','B']
strs=''; n=36; chord={1,3,5,8,10,12,15,17,19,22,24,26}
for i in range(n):
    x=14+i*(1150/(n-1)); th=2.6-1.6*i/n; lit=i in chord
    strs+=f'<line x1="{x:.1f}" y1="10" x2="{x:.1f}" y2="146" stroke="{"#8ff7cc" if lit else "#2f7a5a"}" stroke-width="{th:.2f}" opacity="{1 if lit else .8}" style="{"filter:drop-shadow(0 0 3px #72f2bd)" if lit else ""}"/>'
    if lit: strs+=f'<circle cx="{x:.1f}" cy="150" r="2.4" fill="#72f2bd"/>'
    strs+=f'<text x="{x:.1f}" y="164" class="sn">{notes[(i)%7]}{2+i//7}</text>'
S['strings']=strs
SUBS={
 'hand': sub(mk('Ratchet',.3), mk('Octave',.5), mk('Accent',.6), mb('Latch',True), mb('Retrig')),
 'lfo2': sub(mk('Phase',.25), mk('Depth',.55), mk('Smooth',.4), mb('Tempo',True), mb('Invert')),
 'string': sub(mk('Sympath',.45), mk('Buzz',.15), mk('Harmonic',.6), mk('Release',.5), mb('Damper'), mb('Pedal',True)),
 'filter': sub(mk('Drive',.35), mk('Velo',.5), mk('Spread',.3), mb('24 dB',True), mb('Track')),
 'space': sub(mk('Width',.8), mk('Duck',.2), mk('Mod',.4), mk('Low cut',.3), mb('Tail',True)),
}
for k,v in SUBS.items(): S[k]=S[k][:-6]+v+'</div>'
t=open('tpl.html').read()
for k,v in S.items(): t=t.replace('{{'+k+'}}',v)
open('v6.html','w').write(t)
