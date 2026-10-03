(function(root){
'use strict';
const doc=root.document,nodes=new Map([...doc.querySelectorAll('[id]')].map(n=>[n.id,n]));
const get=id=>nodes.get(id)||doc.getElementById(id);
let records=[],callbacks={},closing=false;
function button(document,label,fn){const b=document.createElement('button');b.type='button';b.textContent=label;b.onclick=fn;return b;}
function notice(record,text){record.message.textContent=text;callbacks.tell(text);}
function move(record,parent){
 // A transfer pauses the one shared clock; no second media player is created.
 callbacks.beforeMove();
 const video=get('video'),ownsVideo=record.panel.contains(video),position=video.currentTime;
 parent.append(record.panel);
 if(ownsVideo&&Number.isFinite(position)){
  try{video.currentTime=position;}catch(_){video.addEventListener('loadedmetadata',()=>{video.currentTime=position;},{once:true});}
 }
 get('timeTooltip').hidden=true;doc.body.append(get('timeTooltip'));
 callbacks.afterMove();
}
function dock(record,close=true){
 const win=record.win;if(!win)return;record.win=null;
 move(record,record.slot);record.placeholder.remove();record.detach.textContent='↗ Finestra';record.message.textContent='';
 if(close&&!win.closed)win.close();
}
function frame(win,record){
 function next(){if(closing||win.closed||(record&&record.win!==win))return;callbacks.tick();win.requestAnimationFrame(next);}
 win.requestAnimationFrame(next);
}
function detach(record){
 if(record.win&&!record.win.closed){record.win.focus();return;}
 let win;
 try{win=root.open('about:blank','_blank','popup=yes,width=960,height=720,resizable=yes,scrollbars=yes');}catch(_){win=null;}
 if(!win){notice(record,'Finestra bloccata: consenti i popup per FoilMotion Studio e riprova.');return;}
 try{
  const d=win.document;d.title=record.title+' · FoilMotion Studio';d.documentElement.lang='it';
  const meta=d.createElement('meta');meta.name='viewport';meta.content='width=device-width,initial-scale=1';d.head.append(meta);
  const ref=d.createElement('meta');ref.name='referrer';ref.content='strict-origin-when-cross-origin';d.head.append(ref);
  const css=d.createElement('link');css.rel='stylesheet';css.href=new URL('style.css',doc.baseURI).href;d.head.append(css);
  d.body.className='detachedStudio';
  const bar=d.createElement('nav');bar.className='windowBar';
  const title=d.createElement('b');title.textContent=record.title;
  bar.append(title,button(d,'▶ Play',()=>callbacks.play(record.key)),button(d,'Ⅱ Pausa',()=>callbacks.pause()),button(d,'↙ Riaggancia',()=>dock(record)));
  const state=d.createElement('output');state.className='windowState';bar.append(state);record.state=state;
  const content=d.createElement('main');content.className='detachedContent';
  d.body.append(bar,content);
  d.onkeydown=callbacks.keydown;
  win.addEventListener('focus',()=>callbacks.activate?.(record.key));
  record.win=win;
  record.placeholder=doc.createElement('div');record.placeholder.className='dockPlaceholder';
  const label=doc.createElement('p');label.textContent=record.title+' · aperto in un’altra finestra';
  record.placeholder.append(label,button(doc,'Mostra finestra',()=>win.focus()),button(doc,'Riaggancia',()=>dock(record)));
  record.slot.append(record.placeholder);record.detach.textContent='↙ Riaggancia';
  win.addEventListener('pagehide',()=>{if(!closing&&record.win===win)dock(record,false);});
  move(record,content);record.message.textContent='Stessa sessione della finestra principale · trasferimento in pausa.';
  frame(win,record);win.focus();
 }catch(err){
  if(record.win===win)dock(record);else win.close();
  notice(record,'Impossibile disancorare: '+err.message);
 }
}
function init(options){
 callbacks=options;const workspace=get('workspace');let dragged=null;
 records=[...workspace.querySelectorAll('[data-dock]')].map(panel=>{
  const slot=doc.createElement('div');slot.className='dockSlot'+(panel.dataset.dock==='data'?' dockWide':'');panel.before(slot);slot.append(panel);
  const record={panel,slot,title:panel.dataset.title,key:panel.dataset.dock,win:null};slot.dataset.key=record.key;
  const bar=doc.createElement('div');bar.className='dockBar';
  const grip=button(doc,'⠿ Sposta',()=>{});grip.className='dockGrip';grip.draggable=true;grip.title='Trascina su un altro riquadro per spostarlo; oppure usa le frecce';
  grip.addEventListener('dragstart',e=>{dragged=record;e.dataTransfer.setData('text/plain',record.key);e.dataTransfer.effectAllowed='move';});
  grip.addEventListener('dragend',()=>{dragged=null;for(const r of records)r.slot.classList.remove('dropTarget');});
  slot.addEventListener('dragover',e=>{if(dragged&&dragged!==record&&!dragged.win){e.preventDefault();slot.classList.add('dropTarget');}});
  slot.addEventListener('dragleave',()=>slot.classList.remove('dropTarget'));
  slot.addEventListener('drop',e=>{e.preventDefault();slot.classList.remove('dropTarget');if(dragged&&dragged!==record&&!dragged.win){callbacks.beforeMove();slot.before(dragged.slot);callbacks.afterMove();}dragged=null;});
  const shift=direction=>{if(record.win)return;const adjacent=direction<0?slot.previousElementSibling:slot.nextElementSibling;if(adjacent){callbacks.beforeMove();direction<0?adjacent.before(slot):adjacent.after(slot);callbacks.afterMove();}};
  const wide=button(doc,'↔ Larghezza',()=>{slot.classList.toggle('dockWide');callbacks.afterMove();});wide.title='Alterna una colonna e tutta la larghezza';
  record.detach=button(doc,'↗ Finestra',()=>record.win?dock(record):detach(record));
  bar.append(grip,button(doc,'←',()=>shift(-1)),button(doc,'→',()=>shift(1)),wide,record.detach);
  record.message=doc.createElement('p');record.message.className='dockMessage';panel.prepend(bar,record.message);
  return record;
 });
 get('dockAll').onclick=()=>{for(const r of records)dock(r);};
 get('resetLayout').onclick=()=>{callbacks.beforeMove();for(const r of records){dock(r);workspace.append(r.slot);r.slot.classList.toggle('dockWide',r.key==='data');}callbacks.afterMove();};
 root.addEventListener('pagehide',()=>{closing=true;for(const r of records)if(r.win&&!r.win.closed)r.win.close();});
 frame(root);
}
function status(text){for(const r of records)if(r.win&&!r.win.closed)r.state.textContent=text;}
root.FoilDock={get,init,status};
})(window);
