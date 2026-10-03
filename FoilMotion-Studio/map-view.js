(function(root){
'use strict';
const cache=new Map();let fitted=null,geometry=null;
const merc=(lat,lon)=>{lat=Math.max(-85.05112878,Math.min(85.05112878,lat));return {x:(lon+180)/360,y:(1-Math.asinh(Math.tan(lat*Math.PI/180))/Math.PI)/2};};
const speedColor=v=>Number.isFinite(v)?`hsl(${200-200*Math.max(0,Math.min(1,v/10))},85%,60%)`:'#8295a0';
function fit(route,w,h){
 if(fitted===route&&geometry?.w===w&&geometry?.h===h)return geometry;
 fitted=route;const first=merc(route.points[0].lat,route.points[0].lon);
 const points=route.points.map(p=>{const q=merc(p.lat,p.lon);q.x=first.x+((q.x-first.x+.5+1)%1-.5);return {...p,...q};});
 let x0=Infinity,x1=-Infinity,y0=Infinity,y1=-Infinity;
 for(const p of points){x0=Math.min(x0,p.x);x1=Math.max(x1,p.x);y0=Math.min(y0,p.y);y1=Math.max(y1,p.y);}
 const z=Math.max(0,Math.min(18,Math.floor(Math.log2(Math.min(Math.max(1,w-50)/Math.max(1e-9,x1-x0),Math.max(1,h-50)/Math.max(1e-9,y1-y0))/256))));
 return geometry={points,z,cx:(x0+x1)/2,cy:(y0+y1)/2,w,h,first};
}
function draw(ctx,w,h,s,t,tiles,visible,status=document.getElementById('mapStatus')){
 if(!s?.route){status.textContent='Nessun GPS valido: mappa non caricata.';return;}
 const g=fit(s.route,w,h),world=256*2**g.z,x=v=>w/2+(v-g.cx)*world,y=v=>h/2+(v-g.cy)*world;
 const http=typeof location!=='undefined'&&/^https?:$/.test(location.protocol);
 let failed=false,pending=false;
 if(tiles&&http&&visible){
  const left=g.cx*world-w/2,top=g.cy*world-h/2,n=2**g.z;
  for(let ty=Math.max(0,Math.floor(top/256));ty<=Math.min(n-1,Math.floor((top+h)/256));ty++)for(let tx=Math.floor(left/256);tx<=Math.floor((left+w)/256);tx++){
   const key=`${g.z}/${((tx%n)+n)%n}/${ty}`;let entry=cache.get(key);
   if(!entry){const img=new Image();entry={img,state:'pending'};cache.set(key,entry);img.onload=()=>entry.state='ok';img.onerror=()=>entry.state='error';img.referrerPolicy='strict-origin-when-cross-origin';img.src='https://tile.openstreetmap.org/'+key+'.png';if(cache.size>128)cache.delete(cache.keys().next().value);}
   if(entry.state==='ok')ctx.drawImage(entry.img,tx*256-left,ty*256-top,256,256);else if(entry.state==='error')failed=true;else pending=true;
  }
 }
 status.textContent=!tiles?'Cartografia disattivata · percorso disponibile offline.':!http?'Per la cartografia avvia Avvia_Analisi.cmd (pagina HTTP locale).':failed?'Tile non disponibili · percorso mantenuto. Disattiva/riattiva la cartografia per riprovare.':pending?'Caricamento cartografia…':'OpenStreetMap · zoom '+g.z;
 ctx.lineWidth=3;
 for(let i=1;i<g.points.length;i++){
  const a=g.points[i-1],b=g.points[i];if(b.break)continue;
  ctx.strokeStyle='#75839066';ctx.beginPath();ctx.moveTo(x(a.x),y(a.y));ctx.lineTo(x(b.x),y(b.y));ctx.stroke();
  if(t<a.t)continue;
  const k=Math.min(1,(t-a.t)/Math.max(.000001,b.t-a.t));
  ctx.strokeStyle=speedColor(FoilCore.reading(s,'gps_speed_mps',a.t));ctx.beginPath();ctx.moveTo(x(a.x),y(a.y));ctx.lineTo(x(a.x+(b.x-a.x)*k),y(a.y+(b.y-a.y)*k));ctx.stroke();
 }
 const lat=FoilCore.reading(s,'latitude_deg',t),lon=FoilCore.reading(s,'longitude_deg',t);
 if(lat!==null&&lon!==null){const p=merc(lat,lon);p.x=g.first.x+((p.x-g.first.x+1.5)%1-.5);ctx.beginPath();ctx.arc(x(p.x),y(p.y),6,0,Math.PI*2);ctx.fillStyle='#fff';ctx.fill();ctx.lineWidth=3;ctx.strokeStyle='#071f30';ctx.stroke();}
 ctx.fillStyle='#111';ctx.fillRect(w-38,6,30,22);ctx.fillStyle='#fff';ctx.fillText('N ↑',w-33,21);
}
const api={draw,merc,speedColor,resetErrors(){for(const [k,e] of cache)if(e.state==='error')cache.delete(k);}};
root.FoilMap=api;if(typeof module!=='undefined')module.exports=api;
})(typeof globalThis!=='undefined'?globalThis:this);
