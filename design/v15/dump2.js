document.fonts.ready.then(function(){
const R=e=>{const r=e.getBoundingClientRect();return [Math.round(r.x),Math.round(r.y),Math.round(r.width),Math.round(r.height)]};
const T=e=>e?e.textContent.replace(/\s+/g,' ').trim():'';
const sec=e=>{const s=e.closest('.sec');if(s)return T(s.querySelector('.tb h2'));if(e.closest('.console'))return 'CONSOLE';if(e.closest('.bed'))return 'STRINGS';return ''};
const o={ctl:[]};
document.querySelectorAll('.f').forEach(f=>{const ft=f.querySelector('.ft');o.ctl.push({k:'fader',s:sec(f),l:T(f.querySelector('.lb')),r:R(ft),bip:!!ft.querySelector('.pm')})});
document.querySelectorAll('.cs .ft').forEach(ft=>o.ctl.push({k:'fader',s:'CONSOLE',l:'Volume',r:R(ft),bip:false}));
document.querySelectorAll('.kn').forEach(k=>{const p=k.closest('.mk,.kc');o.ctl.push({k:'knob',s:sec(k),l:T(p.querySelector('span,.lb')),r:R(k)})});
document.querySelectorAll('.b').forEach(b=>{let l='';const p=b.closest('.mk,.vk,.hk,.cs,.bc');
  if(b.closest('.tb'))l='Sync';else if(p&&p.classList.contains('cs')){l=T(b.previousElementSibling);}else if(p)l=T(p.querySelector('span,.lb,.t'));
  else{const cl=b.closest('.cl');l=(cl?T(cl.querySelector('.lb')):'')+' select'}
  if(b.closest('.vg')){const vgs=[...b.closest('.sec').querySelectorAll('.vg')];l=(vgs.indexOf(b.closest('.vg'))?'Active ':'')+l}
  o.ctl.push({k:'button',s:sec(b),l:l,r:R(b),led:!!b.querySelector('i')})});
document.querySelectorAll('.ll').forEach(ll=>{const cl=ll.closest('.cl');const items=[...ll.querySelectorAll(':scope > div')];
  o.ctl.push({k:'leds',s:sec(ll),l:T(cl.querySelector('.lb')),items:items.map(d=>({t:T(d.querySelector('span')),r:R(d.querySelector('i'))}))})});
document.querySelectorAll('.arr').forEach((a,i)=>o.ctl.push({k:'button',s:'CONSOLE',l:i?'Down':'Up',r:R(a),led:false}));
document.querySelectorAll('.hz').forEach(h=>o.ctl.push({k:'hfader',s:'CONSOLE',l:'Speed',r:R(h)}));
document.querySelectorAll('.lcd').forEach(e=>o.ctl.push({k:'screen',s:sec(e),l:'Exciter',r:R(e)}));
document.querySelectorAll('.lc').forEach((e,i)=>o.ctl.push({k:'screen',s:'CONSOLE',l:i?'Status':'Preset',r:R(e)}));
o.ctl.push({k:'strings',s:'STRINGS',l:'Bed',r:R(document.querySelector('.bed svg'))});
o.ctl.push({k:'label',s:'STRINGS',l:'Chord',r:R(document.querySelector('.chord'))});
const p=document.createElement('pre');p.id='J';p.textContent=JSON.stringify(o);document.body.appendChild(p);
});
