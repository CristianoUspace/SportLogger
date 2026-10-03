#pragma once
#include <Arduino.h>
const char UI[] PROGMEM=R"HTML(<!doctype html>
<html lang="it"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SPORT LOGGER 1.7</title><style>
:root{color-scheme:dark;font:16px system-ui,sans-serif;background:#0b1420;color:#edf4ff}*{box-sizing:border-box}
body{margin:auto;padding:18px;max-width:920px}h1{font-size:24px;margin:6px 0}h2{font-size:18px;margin-top:0}
.sub,small{color:#a8bdd3}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(270px,1fr));gap:14px}
section{background:#152436;border:1px solid #304559;border-radius:15px;padding:18px;margin:14px 0}
.grid section{margin:0}button,a.link{display:inline-block;border:0;border-radius:9px;padding:13px 16px;font:inherit;font-weight:650;background:#9be6cc;color:#09271e;cursor:pointer;text-decoration:none;min-height:46px;margin:4px 4px 4px 0}
button.secondary,a.link{background:#2b4159;color:#edf4ff}button.danger{background:#ffb4a9;color:#420b07}
button:disabled{opacity:.4;cursor:default}input,select,textarea{font:inherit;padding:10px;border:1px solid #496179;background:#0b1420;color:inherit;border-radius:7px;width:100%;margin:5px 0 10px}label{display:block}
.metrics{display:flex;gap:20px;flex-wrap:wrap}.value{font-size:26px;font-weight:650}.warn{color:#ffbd7d}.ok{color:#9be6cc}
#notice{min-height:24px;color:#ffce91;white-space:pre-wrap}#connection{font-weight:bold}.level{position:relative;width:180px;height:180px;border:2px solid #7592ae;border-radius:50%;margin:16px auto;background:linear-gradient(transparent 49.5%,#496179 50%,transparent 50.5%),linear-gradient(90deg,transparent 49.5%,#496179 50%,transparent 50.5%)}
.level:before{content:'';position:absolute;inset:35%;border:1px solid #7592ae;border-radius:50%}#bubble{position:absolute;left:50%;top:50%;width:20px;height:20px;border-radius:50%;background:#9be6cc;transform:translate(-50%,-50%);transition:translate .2s}
.angles{display:flex;justify-content:space-between;font-variant-numeric:tabular-nums;gap:8px}pre{white-space:pre-wrap;overflow-wrap:anywhere;font-size:13px}.session{border-top:1px solid #496179;padding:14px 0;overflow-wrap:anywhere}.offsets{display:flex;gap:10px}.offsets label{flex:1;min-width:0}

.boardViews{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:10px;margin:16px 0}.boardView{min-width:0;background:#0c1720;border:1px solid #304553;border-radius:10px;padding:12px;text-align:center}.boardView h3{font-size:13px;margin:0;color:#b4c9d5}.boardView svg{width:100%;display:block;max-height:150px}.boardView output{font-size:25px;font-weight:650;color:#9be6cc;font-variant-numeric:tabular-nums}.boardView p{font-size:12px;line-height:1.6}.boardView small{font-size:10px;letter-spacing:0;color:#a8bdd3}.boardView svg g{transform-box:view-box}.boardNote{color:#a8bdd3;font-size:12px}
@media(max-width:600px){.boardViews{grid-template-columns:1fr}.boardView svg{max-height:130px}}

.grid{grid-template-columns:1fr}
</style></head><body>
<h1>SPORT LOGGER <small>V1.7 · HMI 1.8</small></h1><div class="sub">Foilscoot · Wingfoil · Kite</div>
<section><div id="connection">Connessione…</div><p id="state">In attesa dello stato ESP32</p>
<div class="metrics"><div><small>Durata</small><div class="value" id="duration">—</div></div><div><small>Batteria</small><div class="value" id="battery">—</div></div><div><small>SD libera</small><div class="value" id="space">—</div></div></div>
<p id="session" class="sub"></p><button id="record" disabled>Avvia registrazione</button><button id="trigger" class="secondary" disabled>Trigger video</button>
<p id="notice" role="status" aria-live="polite"></p><small>Il trigger genera un impulso scuro sul LED. Tenere il LED nell’inquadratura per sincronizzare il video.</small></section>
<div class="grid"><section><h2>Assetto IMU</h2><div class="boardViews"><article class="boardView"><h3>Roll · frontale</h3><svg viewBox="0 0 240 140" role="img" aria-label="Roll · frontale"><path d="M15 70 H225 M120 12 V128" stroke="#466171" stroke-dasharray="4 5"/><g id="boardrollBoard" fill="#51dac7" stroke="#b6ffee" stroke-width="2"><path d="M42 65 Q120 79 198 65 L191 76 Q120 88 49 76Z"/><path d="M120 80 V105 M96 106 H144" fill="none"/></g></svg><output id="boardrollAngle">—</output><p id="boardrollValues">Acc / Gyro —</p><small>Vista dalla prua · asse X</small></article><article class="boardView"><h3>Pitch · laterale</h3><svg viewBox="0 0 240 140" role="img" aria-label="Pitch · laterale"><path d="M15 70 H225 M120 12 V128" stroke="#466171" stroke-dasharray="4 5"/><g id="boardpitchBoard" fill="#51dac7" stroke="#b6ffee" stroke-width="2"><path d="M35 72 Q125 78 200 57 Q207 66 190 72 Q115 87 35 79Z"/><path d="M108 82 V105 M85 106 H139" fill="none"/></g></svg><output id="boardpitchAngle">—</output><p id="boardpitchValues">Acc / Gyro —</p><small>Prua a destra · asse Y</small></article><article class="boardView"><h3>Yaw · dall’alto</h3><svg viewBox="0 0 240 140" role="img" aria-label="Yaw · dall’alto"><path d="M15 70 H225 M120 12 V128" stroke="#466171" stroke-dasharray="4 5"/><g id="boardyawBoard" fill="#51dac7" stroke="#b6ffee" stroke-width="2"><path d="M120 20 Q158 35 145 106 Q120 118 95 106 Q82 35 120 20Z"/><path d="M120 29 V97 M110 43 L120 29 L130 43" fill="none"/></g></svg><output id="boardyawAngle">—</output><p id="boardyawValues">Acc / Gyro —</p><small>Prua in alto · asse Z</small></article></div><p class="boardNote">X verso prua, Y verso sinistra, Z verso l’alto. Yaw relativo: deriva nel tempo e non indica il Nord. Accelerazione comprensiva di gravità.</p><pre id="imu">—</pre>
<button id="calibrate" class="secondary" disabled>Calibra IMU</button><p id="calibration" class="sub"></p><small>Tenere immobile per 3 secondi. Calibra il giroscopio; non azzera l’inclinazione.</small></section>
<section><h2>Ambiente e GNSS</h2><pre id="environment">—</pre><pre id="gps">—</pre><p id="clock" class="sub"></p><button id="time" class="secondary" disabled>Usa ora del telefono</button><p><small>Senza GPS/ora del telefono i tempi restano relativi. UTC GPS stimato da NMEA, senza PPS.</small></p></section></div>
<section><h2>Sessione e correzioni</h2><label>Sport<select id="sport"><option>Foilscoot</option><option>Wingfoil</option><option>Kite</option><option>Generic</option></select></label>
<label>Setup (foil, stab, mast, tavola, vela…)<textarea id="setup" rows="2" maxlength="240" placeholder="Pulse 1990 · Vert 220 · mast 70 · Pump Carbon"></textarea></label>
<p><small>Correzione applicata = angolo misurato − offset. Salvata sul dispositivo; modificabile a registrazione ferma.</small></p>
<div class="offsets"><label>Roll °<input id="or" type="number" min="-180" max="180" step="0.1" value="0"></label><label>Pitch °<input id="op" type="number" min="-180" max="180" step="0.1" value="0"></label><label>Z °<input id="oz" type="number" min="-180" max="180" step="0.1" value="0"></label></div>
<button id="save" class="secondary" disabled>Salva configurazione</button></section>
<section><h2>Registrazioni CSV</h2><p><small>Download e cancellazione disponibili a registrazione ferma. I metadati JSON conservano setup, calibrazione e motivo di arresto.</small></p>
<button id="refresh" class="secondary" disabled>Apri directory</button><button id="remount" class="secondary" disabled>Rileggi SD</button><div id="files"></div>
<button id="prev" class="secondary" hidden>Precedenti</button><button id="next" class="secondary" hidden>Successive</button></section>
<script>
(function(root){
'use strict';
// Sensor convention: X toward nose, Y toward port, Z upward.
// Front view looks from nose toward tail; side view has nose on right.
function update(prefix,angles,acc,gyro){
 const axes=['roll','pitch','yaw'];
 axes.forEach((axis,i)=>{
  const get=s=>document.getElementById(prefix+axis+s),a=angles[i],valid=Number.isFinite(a);
  get('Board').setAttribute('transform',`rotate(${valid?(i===1?a:-a):0} 120 70)`);
  get('Board').style.opacity=valid?'1':'.18';
  get('Angle').textContent=valid?a.toFixed(1)+'°':'—';
  get('Values').textContent='Acc '+ 'XYZ'[i]+': '+(Number.isFinite(acc[i])?acc[i].toFixed(3):'—')+' g · Gyro '+ 'XYZ'[i]+': '+(Number.isFinite(gyro[i])?gyro[i].toFixed(2):'—')+' °/s';
 });
}
root.FoilAttitude={update};
})(typeof globalThis!=='undefined'?globalThis:this);

'use strict';
const $=id=>document.getElementById(id);let state=null,busy=false,online=false,dirty=false,page=0,configLoaded=false;
const fmt=(n,d=1)=>n==null||!Number.isFinite(n)?'—':n.toFixed(d);
const duration=ms=>ms==null?'—':`${Math.floor(ms/3600000)}:${String(Math.floor(ms/60000)%60).padStart(2,'0')}:${String(Math.floor(ms/1000)%60).padStart(2,'0')}`;
const date=t=>t?new Date(t).toLocaleString():'UTC non disponibile';
async function api(path,data){const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),8000);try{
 const r=await fetch(path,{cache:'no-store',signal:controller.signal,...(data===undefined?{}:{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(data)})});
 const j=await r.json();if(!r.ok)throw Error(j.message||`HTTP ${r.status}`);return j;
 }finally{clearTimeout(timer)}}
function controls(){const locked=!online||busy||!state;const idle=state&&!state.recording&&!state.calibrating;
 $('record').disabled=locked||state?.stopping||state?.calibrating||(!state?.recording&&!state?.sd_ok);
 $('trigger').disabled=locked||!state?.recording||state?.stopping;
 for(const id of ['calibrate','time','save','refresh','remount','prev','next'])$(id).disabled=locked||!idle;
 for(const id of ['sport','setup','or','op','oz'])$(id).disabled=locked||!idle;
 for(const el of document.querySelectorAll('#files button'))el.disabled=locked||!idle;
}
function render(s){state=s;online=true;$('connection').textContent='ESP32 connesso';$('connection').className='ok';
 $('record').textContent=s.stopping?'Arresto in corso…':s.recording?'■ Ferma registrazione':'▶ Avvia registrazione';
 $('record').className=s.recording?'danger':'';$('state').textContent=(s.recording?'● REGISTRAZIONE':'Pronto')+(s.error?' · '+s.error:'')+(s.stop_reason&&!s.recording?' · Stop: '+s.stop_reason:'');
 $('duration').textContent=duration(s.duration_ms);$('battery').textContent=fmt(s.battery_v,2)+' V';$('battery').className='value'+(s.battery_warning?' warn':'');
 $('space').textContent=s.sd_ok?fmt(s.sd_free/1048576,0)+' MB':'Non pronta';
 $('session').textContent=`${s.session||'Nessuna sessione'} · ${s.rows} righe · ${s.missed} campioni persi · Trigger ${s.trigger_written} (+${s.trigger_pending})`;
 FoilAttitude.update('board',s.imu_ok?[s.roll,s.pitch,s.z]:[null,null,null],s.imu_ok?s.acc:[],s.imu_ok?s.gyro:[]);
 $('imu').textContent=`IMU: ${s.imu_ok?'OK':'NON DISPONIBILE'}\nAcc [g]: ${s.acc.map(x=>fmt(x,3)).join(' / ')}\nGyro [°/s]: ${s.gyro.map(x=>fmt(x,2)).join(' / ')}\nTemperatura: ${fmt(s.imu_temp)} °C`;
 $('calibration').textContent=s.calibration;
 $('environment').textContent=`BME: ${s.bme_ok?'OK':'NON DISPONIBILE'}\n${fmt(s.temperature)} °C · ${fmt(s.humidity)} % UR\n${fmt(s.pressure)} hPa`;
 $('gps').textContent=`GNSS: ${s.gps_fix?'FIX':'Attesa / fix scaduto'}\nLat ${fmt(s.lat,7)}\nLon ${fmt(s.lon,7)}\nSatelliti ${fmt(s.satellites,0)} · ${fmt(s.speed_mps)} m/s\nEtà posizione: ${fmt(s.gps_age_ms,0)} ms\nCaratteri NMEA: ${s.gps_chars} · checksum errati: ${s.gps_checksum_failed}`;
 $('clock').textContent=`Ora: ${date(s.utc_ms)} · fonte ${s.clock_source}`;
 if(!dirty){$('sport').value=s.sport;$('setup').value=s.setup;['or','op','oz'].forEach((id,i)=>$(id).value=s.offsets[i]);configLoaded=true;}
 controls();
}
async function poll(){try{render(await api('/api/status'))}catch(e){online=false;$('connection').textContent='Connessione persa — stato non confermato';$('connection').className='warn';controls()}finally{setTimeout(poll,500)}}
async function action(fn){if(busy)return;busy=true;controls();try{await fn()}catch(e){$('notice').textContent=e.name==='AbortError'?'Risposta non ricevuta: verificare lo stato prima di ripetere il comando.':e.message}finally{busy=false;controls()}}
$('record').onclick=()=>action(async()=>{const s=await api('/api/record',{recording:!state.recording});render(s);$('notice').textContent=s.recording?'Registrazione avviata':'Registrazione fermata'});
$('trigger').onclick=()=>action(async()=>{const j=await api('/api/trigger',{});$('notice').textContent=`Trigger ${j.trigger_id} accettato; in attesa di scrittura`});
$('calibrate').onclick=()=>action(async()=>{$('notice').textContent=(await api('/api/calibrate',{})).message});
$('time').onclick=()=>action(async()=>{$('notice').textContent=(await api('/api/time',{utc_ms:Date.now()})).message});
for(const id of ['sport','setup','or','op','oz'])$(id).addEventListener('input',()=>dirty=true);
$('save').onclick=()=>action(async()=>{const offsets=['or','op','oz'].map(id=>Number($(id).value));if(!configLoaded||offsets.some(x=>!Number.isFinite(x)||Math.abs(x)>180))throw Error('Offset non valido');
 $('notice').textContent=(await api('/api/config',{sport:$('sport').value,setup:$('setup').value,offsets})).message;dirty=false;});
function text(tag,value){const e=document.createElement(tag);e.textContent=value;return e}
async function files(){const d=await api('/api/files?page='+page);$('files').replaceChildren();
 if(!d.files.length)$('files').append(text('p','Nessuna registrazione in questa pagina.'));
 for(const f of d.files){const box=document.createElement('div');box.className='session';box.append(text('b',f.id+' · '+(f.sport||'')));
 box.append(text('p',`Inizio: ${date(f.start_utc_ms)}\nFine: ${date(f.end_utc_ms)}\nDurata: ${duration(f.duration_ms)} · ${fmt(f.bytes/1024,1)} KB · ${f.state} · ${f.reason}`));
 for(const [kind,label] of [['csv','CSV ↓'],['start','Metadati ↓'],...(f.state==='closed'?[['end','Chiusura ↓']]:[])]){const a=document.createElement('a');a.className='link';a.textContent=label;a.href='/api/download?id='+encodeURIComponent(f.id)+'&kind='+kind;box.append(a)}
 const del=text('button','Elimina');del.className='danger';del.onclick=()=>{if(confirm('Eliminare definitivamente '+f.id+' e i metadati?'))action(async()=>{await api('/api/delete',{id:f.id});await files()})};box.append(del);$('files').append(box);}
 $('prev').hidden=page===0;$('next').hidden=!d.more;controls();}
$('refresh').onclick=()=>action(async()=>{page=0;await files()});$('prev').onclick=()=>action(async()=>{page=Math.max(0,page-1);await files()});$('next').onclick=()=>action(async()=>{page++;await files()});
$('remount').onclick=()=>action(async()=>{$('notice').textContent=(await api('/api/sd/remount',{})).message;render(await api('/api/status'))});
poll();
</script></body></html>)HTML";
