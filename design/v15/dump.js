document.fonts.ready.then(function(){
const R=e=>{const r=e.getBoundingClientRect();return [Math.round(r.x*2),Math.round(r.y*2),Math.round(r.width*2),Math.round(r.height*2)]};
const o={};
o.ft=[...document.querySelectorAll('.ft')].map(e=>{const c=e.querySelector('.cap');return {r:R(e),cap:R(c),bip:!!e.querySelector('.pm')}});
o.btn=[...document.querySelectorAll('.b')].map(e=>({r:R(e),lit:e.classList.contains('lit'),sm:e.classList.contains('sm'),led:getComputedStyle(e.querySelector('i')||e).display!='none'&&!!e.querySelector('i')}));
o.led=[...document.querySelectorAll('.ll i')].map(e=>({r:R(e),on:e.classList.contains('on')}));
o.kn=[...document.querySelectorAll('.kn')].map(e=>({r:R(e),rot:e.querySelector('b').style.transform}));
o.tb=[...document.querySelectorAll('.tb')].map(R);
o.sec=[...document.querySelectorAll('.sec.dark')].map(R);
o.lcd=[...document.querySelectorAll('.lcd,.lc,.disp')].map(R);
o.arr=[...document.querySelectorAll('.arr')].map(R);
o.hz=[...document.querySelectorAll('.hz')].map(e=>({r:R(e),b:R(e.querySelector('b'))}));
o.panel=R(document.querySelector('.panel')); o.tierb=R(document.querySelector('.tier.tb_'));
o.console=R(document.querySelector('.console')); o.bed=R(document.querySelector('.bed'));
o.np=R(document.querySelector('.np'));
document.title='';const p=document.createElement('pre');p.id='J';p.textContent=JSON.stringify(o);document.body.appendChild(p);
});
