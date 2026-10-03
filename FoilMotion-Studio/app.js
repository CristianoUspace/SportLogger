'use strict';
const $=id=>globalThis.FoilDock?FoilDock.get(id):document.getElementById(id),video=$('video'),C=FoilCore;
let sessions=[],s=null,dt=0,playing=false,last=0,anchors=[],map=null,file=null,url=null,wave=null,active='data',view=0,manual=false,loadId=0;
let csvText='',csvFileName='',csvLoadId=0,metadataDocs=[];
const colors=['#55dccb','#ffbb66','#77a9ff','#ef7aa8','#c7a0ff','#c6dd73','#ffa58c'];
const visible=new Set();const ui={};
const tell=t=>$('notice').textContent=t;
const clamp=(x,a,b)=>Math.max(a,Math.min(b,x));
const duration=()=>Number.isFinite(video.duration)?video.duration:0;
const linked=()=>$('linked').checked&&map&&s&&duration()>0;
const fmt=FoilMedia.timecode;
let videoStartMs=null;
function bounds(){return linked()?[Math.max(s.start,map.toData(0)),Math.min(s.end,map.toData(duration()))]:[s?.start||0,s?.end||0];}
function pause(){if(linked()&&!video.paused)dt=clamp(map.toData(video.currentTime),...bounds());playing=false;video.pause();}
function seek(kind,t){pause();if(kind==='data'&&s){dt=clamp(t,...bounds());if(linked())video.currentTime=clamp(map.toVideo(dt),0,duration());}else if(kind==='video'&&duration()){if(linked()){dt=clamp(map.toData(t),...bounds());video.currentTime=map.toVideo(dt);}else video.currentTime=clamp(t,0,duration());}manual=false;render();}
async function play(kind){
 if(linked()){
  const [a,b]=bounds();if(b<=a){tell('Gli intervalli sincronizzati non si sovrappongono.');return;}
  dt=clamp(map.toData(video.currentTime),a,b);
  if(dt>=b-.002){tell('Fine dell’intervallo comune. Sposta un cursore o premi Stop per tornare all’inizio.');return;}
  try{video.playbackRate=+$('speed').value*map.slope;await video.play();playing=true;}catch(e){tell('Playback non disponibile: '+e.message);}
 }else if(kind==='video'&&duration()){video.playbackRate=+$('speed').value;try{await video.play();}catch(e){tell(e.message);}}
 else if(s){if(dt>=s.end)dt=s.start;playing=true;}manual=false;
}
function action(kind,a){active=kind;if(a==='play')return play(kind);if(a==='pause')return pause();if(a==='stop')return seek(kind,kind==='data'?bounds()[0]:linked()?map.toVideo(bounds()[0]):0);let t=kind==='data'?dt:video.currentTime;if(a==='back'||a==='forward'){const dir=a==='back'?-1:1;if(kind==='data'&&s){t=C.nearest(s.samples,t,dir);}else t+=dir/(+$('fps').value||30);}else t+=a==='rrw'?-5:5;seek(kind,t);}
for(const kind of ['data','video']){const host=$(kind+'Controls');for(const [a,label] of [['rrw','−5 s'],['back','◀ |'],['play','▶ Play'],['pause','Ⅱ Pausa'],['stop','■ Stop'],['forward','| ▶'],['ffw','+5 s']]){const b=document.createElement('button');b.textContent=label;b.title=({back:'Passo precedente',forward:'Passo successivo',rrw:'Riavvolgi 5 secondi',ffw:'Avanza 5 secondi'})[a]||label;b.onclick=()=>action(kind,a);host.append(b);}const out=document.createElement('output'),slider=document.createElement('input');slider.type='range';slider.className='seek';slider.min=0;slider.max=1;slider.step=.001;slider.value=0;slider.setAttribute('aria-label',kind==='data'?'Posizione dati':'Posizione video');slider.oninput=()=>seek(kind,+slider.value);host.append(out,slider);ui[kind]={host,out,slider};host.closest('.panel').addEventListener('pointerdown',()=>active=kind);}
function updateMap(){try{map=anchors.length?C.mapping(anchors):null;$('map').textContent=map?'video = dati × '+map.slope.toFixed(6)+' '+(map.offset>=0?'+ ':'− ')+Math.abs(map.offset).toFixed(3)+' s':'Nessun hook';}catch(e){map=null;tell(e.message);}$('hooks').textContent=anchors.map((a,i)=>`Hook ${i+1}: dati ${fmt(a.data)} ↔ video ${fmt(a.video)}`).join('   ·   ');$('linked').disabled=!s||!duration();$('syncHere').disabled=!s||!duration();if(!map)$('linked').checked=false;}
$('addHook').onclick=()=>{pause();if(!s||!duration())return tell('Carica sia CSV sia video per creare un hook.');if(anchors.length===2)return tell('Sono già presenti due hook. Cancella gli hook per ridefinirli.');const next=[...anchors,{data:dt,video:video.currentTime}];try{const m=C.mapping(next);if(m.slope<.1||m.slope>10)throw Error('Rapporto dei tempi non plausibile: verifica gli hook.');anchors=next;updateMap();}catch(e){tell(e.message);}};
function syncHere(){
 pause();if(!s||!duration())return tell('Carica sia CSV sia video.');
 anchors=[{data:dt,video:video.currentTime}];updateMap();$('linked').checked=true;
 tell('Sincronizzati questi istanti: dati '+fmt(dt)+' ↔ video '+fmt(video.currentTime)+'. Play riparte da qui.');render();
}
$('syncHere').onclick=syncHere;
$('clearHooks').onclick=()=>{pause();anchors=[];updateMap();};
$('linked').onchange=()=>{
 pause();if(!$('linked').checked)return;
 if(!map){syncHere();return;}
 if(Math.abs(map.toVideo(dt)-video.currentTime)>.05){
  $('linked').checked=false;
  tell('I cursori non corrispondono agli hook salvati. Premi «Sincronizza questi istanti» per abbinare l’evento dati al frame video attuale.');return;
 }
 const [a,b]=bounds();if(b<=a){$('linked').checked=false;tell('Nessuna sovrapposizione tra dati e video.');return;}
 // Preserve the selected video frame, including its sub-frame seek position.
 dt=map.toData(video.currentTime);render();
};
function selectSession(){
 pause();s=sessions[+$('session').value];dt=s.samples[0];anchors=[];updateMap();visible.clear();
 $('traces').replaceChildren();$('channels').replaceChildren();const groups={};
 for(const name of C.GROUPS){const box=document.createElement('fieldset'),legend=document.createElement('legend');legend.textContent=name;box.append(legend);groups[name]=box;}
 s.tracks.forEach((track,i)=>{
  track.color=colors[i%colors.length];const primary=s.format==='legacy'?/^(roll_est|pitch_est|acc_)/.test(track.name):/^(roll_corrected_deg|pitch_corrected_deg|z_corrected_deg|a[xyz]_g)$/.test(track.name);
  const available=track.points.some(p=>Number.isFinite(p.v));if(primary&&available)visible.add(track.name);
  const label=document.createElement('label'),cb=document.createElement('input');cb.type='checkbox';cb.checked=primary&&available;cb.disabled=!available;
  cb.onchange=()=>{cb.checked?visible.add(track.name):visible.delete(track.name);render();};label.style.color=available?track.color:'#71818b';label.title=track.name;
  label.append(cb,document.createTextNode(' '+track.label+' ['+track.unit+']'+(available?'':' · assente')));
  (primary?$('traces'):groups[track.group]).append(label);
 });
 for(const group of Object.values(groups))if(group.children.length>1)$('channels').append(group);
 $('events').replaceChildren(...(s.events.length?s.events.map((e,i)=>new Option(fmt(e.t)+' · '+e.name,i)):[new Option('Nessun evento','')]));
 $('eventGo').disabled=!s.events.length;$('events').disabled=!s.events.length;
 $('metadata').disabled=s.format==='legacy';$('clearMetadata').disabled=!metadataDocs.length;
 $('formatBadge').textContent=s.format==='legacy'?'FoilScoot precedente':'SPORT LOGGER V1.7';
 $('quality').textContent=s.warnings.join(' ');$('quality').hidden=!s.warnings.length;
 $('sessionInfo').textContent=`${s.samples.length.toLocaleString('it')} campioni · ${fmt(s.end-s.start)} · ${s.rate?s.rate.toFixed(2)+' Hz effettivi':'campione singolo'} · ${s.events.length} eventi`;
 $('attitudeHelp').textContent=s.format==='legacy'?'Roll e pitch stimati dall’accelerometro: le accelerazioni dinamiche alterano la stima.':'Angoli calcolati dal firmware; corretti = offset utente sottratti. Z è relativo, soggetto a deriva e non indica il Nord.';
 const m=s.metadata;$('metadataInfo').textContent=m?`${m.sport||'Sport non specificato'} · ${m.setup||'Setup non specificato'} · ${m.state==='closed'?'Chiusura: '+(m.reason||'non specificata'):'Solo apertura'}${Number.isFinite(m.duration_ms)?' · durata sessione '+fmt(m.duration_ms/1000):''}`:'Metadati opzionali: seleziona insieme i JSON di apertura e chiusura della stessa sessione.';
 if(m){const stamp=v=>Number.isFinite(v)&&v>0&&v<=8640000000000000?new Date(v).toISOString():'non disponibile';$('metadataInfo').textContent+='\nInizio UTC: '+stamp(m.start_utc_ms)+' · Fine UTC: '+stamp(m.end_utc_ms);}
 manual=false;eventInfo();render();
}
function installSessions(parsed){sessions=parsed;$('session').replaceChildren(...sessions.map((entry,i)=>new Option(entry.label,i)));$('csvName').textContent=csvFileName;selectSession();}
$('csv').onchange=async e=>{
 const f=e.target.files[0];if(!f)return;const token=++csvLoadId;
 try{if(f.size>100*1024*1024)throw Error('CSV oltre 100 MB: usare una registrazione più breve per questo player locale.');const text=await f.text();if(token!==csvLoadId)return;
  const parsed=C.parse(text,{fileName:f.name});csvText=text;csvFileName=f.name;metadataDocs=[];installSessions(parsed);tell('CSV caricato. Scegli le tracce oppure un trigger per allineare il video.');
 }catch(err){tell(err.message);}e.target.value='';
};$('session').onchange=selectSession;
$('metadata').onchange=async e=>{
 const files=[...e.target.files],token=csvLoadId;if(!files.length)return;
 try{if(!s||s.format==='legacy')throw Error('Apri prima un CSV SPORT LOGGER V1.7.');if(files.length>2||files.some(f=>f.size>1000000))throw Error('Seleziona al massimo i due piccoli JSON di apertura e chiusura.');
  const docs=await Promise.all(files.map(async f=>JSON.parse(await f.text())));if(token!==csvLoadId)return;
  const parsed=C.parse(csvText,{fileName:csvFileName,metadata:docs});metadataDocs=docs;installSessions(parsed);tell('Metadati applicati. Origine temporale precisa aggiornata: gli hook precedenti sono stati azzerati.');
 }catch(err){tell(err.message);}e.target.value='';
};
$('clearMetadata').onclick=()=>{if(!csvText)return;const parsed=C.parse(csvText,{fileName:csvFileName});metadataDocs=[];installSessions(parsed);tell('Metadati rimossi; hook azzerati.');};
function eventInfo(){const e=s?.events[+$('events').value];$('eventInfo').textContent=e?(e.kind==='trigger'?'Trigger '+e.id+' · evento '+fmt(e.t)+' · campione '+fmt(e.sampleT)+' · riferimento '+(e.basis==='click'?'ricezione sull’ESP32':'campione CSV'):e.name+' · '+fmt(e.t)):'Nessun evento registrato.';}
$('events').onchange=eventInfo;
$('eventGo').onclick=()=>{const e=s?.events[+$('events').value];if(e){seek('data',e.t);eventInfo();}};
$('movie').onchange=async e=>{const f=e.target.files[0];if(!f)return;pause();loadId++;const token=loadId;videoStartMs=null;$('videoStart').value='';$('videoTimeStatus').textContent='Senza ora verificata mostriamo il tempo trascorso.';$('videoMetadata').textContent='Lettura metadati MP4…';file=f;wave=null;anchors=[];if(url)URL.revokeObjectURL(url);url=URL.createObjectURL(f);video.src=url;$('videoName').textContent=f.name;$('videoEmpty').hidden=true;$('audioStatus').textContent='Audio disponibile durante il playback. Calcola la forma d’onda per cercare gli impulsi.';updateMap();e.target.value='';try{const stamp=await FoilMedia.creationTime(f);if(token===loadId)$('videoMetadata').textContent=stamp?'Creazione contenitore: '+new Date(stamp).toISOString()+' — potrebbe essere l’esportazione, non l’inizio ripresa. Non applicata automaticamente.':'Data di creazione MP4 assente/non utilizzabile.';}catch(err){if(token===loadId)$('videoMetadata').textContent='Data MP4 non leggibile. Puoi indicare un inizio verificato.'}};video.onloadedmetadata=()=>{updateMap();render();};video.onerror=()=>tell('Video non decodificabile dal browser. Usa una copia MP4 con video H.264 e audio AAC.');video.onended=()=>{playing=false;};
$('volume').oninput=()=>video.volume=+$('volume').value;$('mute').onchange=()=>video.muted=$('mute').checked;
$('speed').onchange=()=>{pause();tell('Velocità impostata a '+$('speed').value+'×. Premi Play per riprendere.');};
$('decode').onclick=async()=>{if(!file)return tell('Apri prima un video.');if(file.size>350*1024*1024)return tell('Forma d’onda limitata a video sotto 350 MB. L’ascolto resta disponibile.');const token=loadId;$('decode').disabled=true;$('audioStatus').textContent='Decodifica audio in corso…';let ctx;try{ctx=new AudioContext();const buffer=await ctx.decodeAudioData(await file.arrayBuffer());if(token!==loadId)return;const count=Math.min(60000,Math.ceil(buffer.duration*100)),peaks=new Float32Array(count);for(let ch=0;ch<buffer.numberOfChannels;ch++){const data=buffer.getChannelData(ch);for(let i=0;i<count;i++){let p=0;const a=Math.floor(i*data.length/count),b=Math.floor((i+1)*data.length/count);for(let j=a;j<b;j++)p=Math.max(p,Math.abs(data[j]));peaks[i]=Math.max(peaks[i],p);}}wave={peaks,duration:buffer.duration};$('audioStatus').textContent='Forma d’onda pronta · clic per cercare · audio attivo durante Play.';}catch(e){if(token===loadId)$('audioStatus').textContent='Il browser non estrae l’audio da questo contenitore. Puoi comunque ascoltarlo durante Play.';}finally{if(ctx)await ctx.close();$('decode').disabled=false;render();}};
function span(){return !s?10:$('window').value==='all'?Math.max(.01,s.end-s.start):+$('window').value;}
$('window').onchange=()=>{manual=false;render();};$('zoomY').oninput=render;$('resetZoom').onclick=()=>{$('window').value='10';$('zoomY').value=1;manual=false;render();};
function surface(id){const canvas=$(id),r=canvas.getBoundingClientRect(),dpr=canvas.ownerDocument?.defaultView?.devicePixelRatio||devicePixelRatio||1;if(canvas.width!==Math.round(r.width*dpr)||canvas.height!==Math.round(r.height*dpr)){canvas.width=Math.round(r.width*dpr);canvas.height=Math.round(r.height*dpr);}const ctx=canvas.getContext('2d');ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,r.width,r.height);ctx.font='11px system-ui';return {ctx,w:r.width,h:r.height};}
function plot(){
 const tracks=s?s.tracks.filter(t=>visible.has(t.name)):[],groups=[...new Set(tracks.map(t=>t.unit))];
 $('plot').style.height=Math.max(350,groups.length*100)+'px';
 const {ctx,w,h}=surface('plot');ctx.fillStyle='#8ea4b3';if(!s){ctx.fillText('Apri un CSV per esplorare le tracce.',25,45);return;}
 const width=span();if(!manual)view=clamp(dt-width*.4,s.start,Math.max(s.start,s.end-width));
 const end=view+width,left=65,right=w-15,x=t=>left+(t-view)/width*(right-left),lane=(h-35)/Math.max(1,groups.length);
 for(let g=0;g<groups.length;g++){
  const ts=tracks.filter(t=>t.unit===groups[g]);let min=Infinity,max=-Infinity;
  const ranges=ts.map(t=>({track:t,a:Math.max(0,C.lowerBound(t.points,view)-1),b:Math.min(t.points.length,C.lowerBound(t.points,end)+1)}));
  for(const {track,a,b} of ranges)for(let i=a;i<b;i++){const p=track.points[i];if(Number.isFinite(p.v)&&p.t>=view&&p.t<=end){min=Math.min(min,p.v);max=Math.max(max,p.v);}}
  if(!Number.isFinite(min)){min=-1;max=1;}
  const mid=(min+max)/2,range=Math.max(.05,(max-min)*.6)/(+$('zoomY').value),top=g*lane+12,bottom=(g+1)*lane-8,y=v=>bottom-(v-(mid-range))/(range*2)*(bottom-top);
  ctx.strokeStyle='#243845';ctx.fillStyle='#90a6b5';
  for(let k=0;k<3;k++){const v=mid-range+k*range,yy=y(v);ctx.beginPath();ctx.moveTo(left,yy);ctx.lineTo(right,yy);ctx.stroke();ctx.fillText(v.toFixed(2),5,yy+3);}ctx.fillText(groups[g],6,top+12);
  ctx.save();ctx.beginPath();ctx.rect(left,top,right-left,bottom-top);ctx.clip();
  for(const {track,a,b} of ranges){ctx.strokeStyle=track.color;ctx.lineWidth=1.5;ctx.beginPath();let prev=null;
   for(let i=a;i<b;i++){const p=track.points[i];if(!Number.isFinite(p.v)){prev=null;continue;}
    const wrapped=prev&&['°','° rotta'].includes(track.unit)&&Math.abs(p.v-prev.v)>180;
    if(!prev||p.t-prev.t>track.maxGap||wrapped){ctx.moveTo(x(p.t),y(p.v));ctx.fillStyle=track.color;ctx.fillRect(x(p.t)-1,y(p.v)-1,2,2);}else ctx.lineTo(x(p.t),y(p.v));prev=p;
   }ctx.stroke();
  }ctx.restore();
 }
 for(let k=0;k<=5;k++){const t=view+width*k/5;ctx.fillStyle='#8ea4b3';ctx.fillText(t.toFixed(2),clamp(x(t)-12,0,w-42),h-9);}
 for(const e of s.events)if(e.t>=view&&e.t<=end){ctx.fillStyle='#ffbe6688';ctx.fillRect(x(e.t),0,1,h-30);}
 if(dt>=view&&dt<=end){ctx.fillStyle='#ffffff';ctx.fillRect(x(dt),0,1,h-30);}
 if(!groups.length){ctx.fillStyle='#90a6b5';ctx.fillText('Seleziona almeno una traccia.',70,50);}
}
const nfmt=(v,d=1)=>Number.isFinite(v)?v.toFixed(d):'—';
function at(...names){const name=names.find(n=>s?.byName[n]);return name?C.reading(s,name,dt):null;}
function telemetry(){
 if(!s)return;
 const modern=s.format!=='legacy',roll=at('roll_corrected_deg','roll_deg','roll_est_deg'),pitch=at('pitch_corrected_deg','pitch_deg','pitch_est_deg'),z=at('z_corrected_deg','z_relative_deg');
 FoilAttitude.update('board',[roll,pitch,z],['x','y','z'].map(a=>at('a'+a+'_g','acc_'+a+'_g')),['x','y','z'].map(a=>at('g'+a+'_dps','gyro_'+a+'_dps')),$);
 $('attitudeValues').textContent='Roll '+nfmt(roll)+'° · Pitch '+nfmt(pitch)+'°'+(modern?'\nZ relativo '+nfmt(z)+'°':'');
 $('imuValues').textContent='Acc [g] '+['x','y','z'].map(a=>nfmt(at('a'+a+'_g','acc_'+a+'_g'),3)).join(' / ')+(modern?'\nIMU: '+(at('imu_valid')===1?'valida':'non disponibile'):'\nAngoli stimati da accelerometro');
 $('bmeValues').textContent=nfmt(at('bme_temp_c'))+' °C · '+nfmt(at('humidity_pct'))+' %\n'+nfmt(at('pressure_hpa'))+' hPa';
 $('bmeState').textContent=modern?(at('bme_valid')===1&&at('bme_temp_c')!==null?'Misura valida · età '+nfmt(at('bme_age_ms'),0)+' ms':'BME non disponibile'):'BME non presente nel formato precedente';
 const battery=at('battery_v'),warning=at('battery_warning');$('batteryValue').textContent=nfmt(battery,3)+' V';$('batteryValue').className=warning===1?'warning':'';
 $('batteryState').textContent=battery===null?'Misura non disponibile':warning===1?'Batteria sotto soglia registrata':warning===0?'Nessun warning registrato':'Soglia non disponibile nel CSV';
 const lat=at('latitude_deg'),lon=at('longitude_deg'),fix=lat!==null&&lon!==null;
 $('gpsValues').textContent=fix?nfmt(at('gps_speed_mps'),2)+' m/s\n'+nfmt(lat,6)+', '+nfmt(lon,6):'Posizione non disponibile';
 $('gpsState').textContent=modern?'Satelliti '+nfmt(at('gps_satellites'),0)+' · HDOP '+nfmt(at('gps_hdop'),2)+'\nQuota '+nfmt(at('gps_altitude_m'))+' m · rotta '+nfmt(at('gps_course_deg'))+'°':'GPS non presente nel formato precedente';
 const utc=C.utcAt(s,dt),missed=at('missed_samples');
 $('cursorUtc').textContent='Cursore '+fmt(dt)+' · '+(utc?'UTC '+new Date(utc.ms).toISOString()+' ('+utc.source+')':'UTC non disponibile')+(missed!==null?' · campioni persi '+nfmt(missed,0):'');
}
function drawRoute(){
 const {ctx,w,h}=surface('route'),r=$('route').getBoundingClientRect();
 const win=$('route').ownerDocument?.defaultView||globalThis;FoilMap.draw(ctx,w,h,s,dt,$('mapTiles').checked,r.top<(win.innerHeight||1000)&&r.top+h>0,$('mapStatus'));
}
$('mapTiles').onchange=()=>{FoilMap.resetErrors();render();};
$('videoStart').onchange=()=>{const value=$('videoStart').value.trim(),parsed=Date.parse(value);videoStartMs=/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(?:\.\d+)?(?:Z|[+-]\d{2}:\d{2})$/.test(value)&&Number.isFinite(parsed)?parsed:null;$('videoTimeStatus').textContent=videoStartMs!==null?'Ora indicata dall’utente · mostrata in UTC. Gli hook restano invariati.':value?'Ora non valida: includi data, secondi e fuso orario.':'Senza ora verificata mostriamo il tempo trascorso.';};
function hoverTime(kind,t,e){
 const tooltip=$('timeTooltip'),owner=e.target?.ownerDocument||document,win=owner.defaultView||globalThis;if(tooltip.ownerDocument!==owner)owner.body.append(tooltip);let text=(kind==='data'?'Dati ':'Video ')+fmt(t);
 const dataT=kind==='data'?t:map?map.toData(t):null;
 const utc=dataT!==null&&s?C.utcAt(s,dataT):null;
 if(utc)text+='\nUTC dati '+new Date(utc.ms).toISOString()+(kind==='video'?' · da hook':'');
 if(kind==='data'&&map)text+='\nVideo da hook '+fmt(map.toVideo(t));
 if(kind==='video'&&videoStartMs!==null)text+='\nUTC video '+new Date(videoStartMs+t*1000).toISOString()+' · inizio manuale';
 tooltip.textContent=text;tooltip.hidden=false;tooltip.style.left=Math.max(8,Math.min(e.clientX+12,(win.innerWidth||1000)-345))+'px';tooltip.style.top=Math.max(8,Math.min(e.clientY+15,(win.innerHeight||800)-110))+'px';
}
for(const kind of ['data','video']){ui[kind].slider.onpointermove=e=>{if(ui[kind].slider.disabled)return;const r=ui[kind].slider.getBoundingClientRect(),fraction=clamp((e.clientX-r.left-8)/Math.max(1,r.width-16),0,1);hoverTime(kind,+ui[kind].slider.min+fraction*(+ui[kind].slider.max-+ui[kind].slider.min),e);};ui[kind].slider.onpointerleave=()=>$('timeTooltip').hidden=true;}
$('plot').addEventListener('pointermove',e=>{if(!s)return;const r=$('plot').getBoundingClientRect(),t=view+clamp((e.clientX-r.left-65)/(r.width-80),0,1)*span();if(t>=s.start&&t<=s.end)hoverTime('data',t,e);else $('timeTooltip').hidden=true;});
$('plot').addEventListener('pointerleave',()=>$('timeTooltip').hidden=true);
function drawWave(){const {ctx,w,h}=surface('wave');ctx.strokeStyle='#51dac7';ctx.beginPath();if(wave){for(let i=0;i<w;i++){const a=Math.floor(i/w*wave.peaks.length),b=Math.max(a+1,Math.floor((i+1)/w*wave.peaks.length));let p=0;for(let j=a;j<b;j++)p=Math.max(p,wave.peaks[j]||0);ctx.moveTo(i,h/2-p*h*.43);ctx.lineTo(i,h/2+p*h*.43);}}else{ctx.moveTo(0,h/2);ctx.lineTo(w,h/2);ctx.fillStyle='#8ea4b3';ctx.fillText('Traccia audio · calcola la forma d’onda',18,25);}ctx.stroke();if(duration()){const x=video.currentTime/duration()*w;ctx.fillStyle='#fff';ctx.fillRect(x,0,1,h);ctx.fillStyle='#8ea4b3';ctx.fillText('0 s',8,h-6);ctx.fillText(fmt(duration()),Math.max(50,w-90),h-6);}}
function render(){for(const kind of ['data','video']){const u=ui[kind],enabled=kind==='data'?!!s:duration()>0,t=kind==='data'?dt:video.currentTime,end=kind==='data'?s?.end||0:duration();u.slider.min=kind==='data'?s?.start||0:0;u.slider.max=end||1;u.slider.value=t||0;u.slider.disabled=!enabled;u.host.querySelectorAll('button').forEach(b=>b.disabled=!enabled);u.out.textContent=fmt(t)+' / '+fmt(end);}plot();drawWave();telemetry();drawRoute();if(globalThis.FoilDock)FoilDock.status('Dati '+fmt(dt)+' · Video '+fmt(video.currentTime)+' · '+(linked()?'Sincrono':'Cursori indipendenti'));}
let drag=null;$('plot').onpointerdown=e=>{if(!s)return;pause();drag={x:e.clientX,view};$('plot').setPointerCapture(e.pointerId);};$('plot').onpointermove=e=>{if(!drag)return;const delta=e.clientX-drag.x;if(Math.abs(delta)>3){manual=true;view=clamp(drag.view-delta/($('plot').clientWidth-80)*span(),s.start,Math.max(s.start,s.end-span()));render();}};$('plot').onpointerup=e=>{if(!drag)return;if(Math.abs(e.clientX-drag.x)<4){const r=$('plot').getBoundingClientRect();seek('data',view+clamp((e.clientX-r.left-65)/(r.width-80),0,1)*span());}drag=null;};$('plot').onwheel=e=>{e.preventDefault();const options=[...$('window').options],i=$('window').selectedIndex;$('window').selectedIndex=clamp(i+(e.deltaY>0?1:-1),0,options.length-1);manual=false;render();};$('wave').onclick=e=>{const r=$('wave').getBoundingClientRect();seek('video',(e.clientX-r.left)/r.width*duration());};
document.onkeydown=e=>{if(/INPUT|SELECT|BUTTON|TEXTAREA/.test(e.target.tagName))return;if(e.code==='Space'){e.preventDefault();if(playing||!video.paused)pause();else play(active);}if(e.key==='ArrowLeft'||e.key==='ArrowRight'){e.preventDefault();action(active,e.key==='ArrowLeft'?'back':'forward');}};
let painted=0;function tick(now){const elapsed=last?(now-last)/1000:0;last=now;if(linked()&&!video.paused){dt=map.toData(video.currentTime);if(dt>=bounds()[1]){seek('data',bounds()[1]);}}else if(playing&&s&&!linked()){dt=Math.min(s.end,dt+elapsed*+$('speed').value);if(dt>=s.end)playing=false;}if(now-painted>33){render();painted=now;}}updateMap();
if(globalThis.FoilDock)FoilDock.init({tick:()=>tick(performance.now()),beforeMove:pause,afterMove:render,keydown:document.onkeydown,tell,play:key=>play(key==='video'||!s?'video':'data'),activate:key=>active=key==='video'?'video':'data',pause});
else{const loop=now=>{tick(now);requestAnimationFrame(loop);};requestAnimationFrame(loop);}

