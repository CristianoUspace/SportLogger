(function(root){
'use strict';
function csv(text){
 const rows=[];let row=[],field='',quoted=false;
 for(let i=0;i<text.length;i++){const c=text[i];if(c==='"'){if(quoted&&text[i+1]==='"'){field+='"';i++;}else quoted=!quoted;}else if(c===','&&!quoted){row.push(field);field='';}else if((c==='\n'||c==='\r')&&!quoted){if(c==='\r'&&text[i+1]==='\n')i++;row.push(field);if(row.some(x=>x.trim()))rows.push(row);row=[];field='';}else field+=c;}
 if(quoted)throw Error('CSV: virgolette non chiuse.');if(field||row.length){row.push(field);rows.push(row);}return rows;
}
const number=s=>s===undefined||s===null||String(s).trim()===''?NaN:Number(s);
const finite=s=>Number.isFinite(number(s));
const flag=s=>s===true||s===1||String(s).trim().toLowerCase()==='true'||String(s).trim()==='1';
const BOOLS=new Set(['imu_valid','bme_valid','battery_warning','gps_fix_valid','trigger']);
const GROUPS=['Assetto','Accelerazione','Giroscopio','Ambiente','Batteria','GPS','Diagnostica','Altri'];
const INFO={
 roll_est_deg:['Roll stimato da accelerometro','°','Assetto'],pitch_est_deg:['Pitch stimato da accelerometro','°','Assetto'],
 roll_deg:['Roll firmware','°','Assetto'],pitch_deg:['Pitch firmware','°','Assetto'],z_relative_deg:['Z relativo firmware','°','Assetto'],
 roll_corrected_deg:['Roll corretto','°','Assetto'],pitch_corrected_deg:['Pitch corretto','°','Assetto'],z_corrected_deg:['Z relativo corretto','°','Assetto'],
 imu_temp_c:['Temperatura IMU','°C','Ambiente'],bme_temp_c:['Temperatura BME','°C','Ambiente'],pressure_hpa:['Pressione','hPa','Ambiente'],humidity_pct:['Umidità','%','Ambiente'],
 battery_v:['Batteria','V','Batteria'],battery_warning:['Warning batteria','0/1','Batteria'],
 latitude_deg:['Latitudine','° geo','GPS'],longitude_deg:['Longitudine','° geo','GPS'],gps_speed_mps:['Velocità GPS','m/s','GPS'],
 gps_course_deg:['Rotta sul terreno','° rotta','GPS'],gps_altitude_m:['Quota GPS','m','GPS'],gps_satellites:['Satelliti','sat','GPS'],gps_hdop:['HDOP','HDOP','GPS'],
 imu_valid:['IMU valida','0/1','Diagnostica'],bme_valid:['BME valido','0/1','Diagnostica'],gps_fix_valid:['Fix GPS valido','0/1','Diagnostica'],
 bme_age_ms:['Età BME','ms','Diagnostica'],gps_location_age_ms:['Età posizione GPS','ms','Diagnostica'],gps_time_age_ms:['Età UTC GPS','ms','Diagnostica'],
 missed_samples:['Campioni persi cumulativi','conteggio','Diagnostica'],gps_chars:['Caratteri NMEA','conteggio','Diagnostica'],gps_checksum_failed:['Checksum NMEA errati','conteggio','Diagnostica']
};
function info(name){
 if(INFO[name]){const [label,unit,group]=INFO[name];return {label,unit,group};}
 if(/^(a[xyz]_g|acc_[xyz]_g)$/.test(name))return {label:'Acc '+name.match(/[xyz]/)[0].toUpperCase(),unit:'g',group:'Accelerazione'};
 if(/^(g[xyz](_raw)?_dps|gyro_[xyz]_dps)$/.test(name))return {label:'Gyro '+name.match(/^(?:gyro_|g)([xyz])/)[1].toUpperCase()+(name.includes('_raw')?' grezzo':''),unit:'°/s',group:'Giroscopio'};
 return {label:name,unit:name.endsWith('_deg')?'°':name.endsWith('_g')?'g':name.endsWith('_dps')?'°/s':name.endsWith('_v')?'V':name,group:'Altri'};
}
function decorate(s){
 s.byName=Object.create(null);
 for(const t of s.tracks){Object.assign(t,info(t.name));t.maxGap=t.maxGap||.1;s.byName[t.name]=t;}
 s.rate=s.samples.length>1?(s.samples.length-1)/(s.samples[s.samples.length-1]-s.samples[0]):null;
 s.route=route(s);return s;
}
function parse(text,options={}){
 const rows=csv(text.replace(/^\uFEFF/,''));if(rows.length<2)throw Error('CSV vuoto.');const headers=rows.shift().map(s=>s.trim());
 if(headers.some(h=>!h)||new Set(headers).size!==headers.length)throw Error('CSV: intestazioni vuote o duplicate.');
 const modern=headers.includes('mono_us')&&headers.includes('seq');
 if(!modern&&(!headers.includes('device_us')||!headers.includes('row_type')))throw Error('Formato non riconosciuto: attesi CSV FoilScoot oppure SPORT LOGGER V1.7.');
 const warnings=[];
 if(modern&&rows.length&&rows[rows.length-1].length!==headers.length){rows.pop();warnings.push('Ultima riga incompleta ignorata: possibile interruzione della scrittura.');}
 if(!rows.length)throw Error('Nessun campione completo.');
 if(modern&&rows.some(r=>r.length!==headers.length))throw Error('CSV: riga incompleta nel mezzo del file.');
 const objects=rows.map(r=>Object.fromEntries(headers.map((h,i)=>[h,r[i]??''])));
 return modern?[modernSession(headers,objects,options,warnings)]:legacySessions(headers,objects);
}
function legacySessions(headers,objects){
 const groups=new Map();for(const r of objects){const key=JSON.stringify([r.session_id,r.boot_id]);if(!groups.has(key))groups.set(key,[]);groups.get(key).push(r);}
 const sessions=[];
 for(const [key,rs] of groups){const data=rs.filter(r=>r.row_type==='data'&&finite(r.device_us));if(!data.length)continue;
  for(let i=1;i<data.length;i++)if(+data[i].device_us<=+data[i-1].device_us)throw Error('Timestamp duplicati o non crescenti nella sessione.');
  const first=data[0],offset=finite(first.session_s)?+first.session_s:0,origin=+first.device_us-offset*1e6,time=r=>(+r.device_us-origin)/1e6;
  const excluded=new Set(['session_id','boot_id','row_type','device_us','session_s','received_utc','received_local','received_monotonic_ns','sequence','missed_slots_total','led','event','battery_state']);
  const names=headers.filter(h=>!excluded.has(h)&&rs.some(r=>finite(r[h])));
  const tracks=names.map(name=>({name,maxGap:name==='battery_v'?3:.1,points:rs.filter(r=>finite(r.device_us)&&finite(r[name])).map(r=>({t:time(r),v:+r[name]})).sort((a,b)=>a.t-b.t)}));
  for(const name of ['roll_est_deg','pitch_est_deg']){const points=data.filter(r=>['acc_x_g','acc_y_g','acc_z_g'].every(k=>finite(r[k]))).map(r=>({t:time(r),v:(name.startsWith('roll')?Math.atan2(+r.acc_y_g,+r.acc_z_g):Math.atan2(-r.acc_x_g,Math.hypot(+r.acc_y_g,+r.acc_z_g)))*180/Math.PI}));if(points.length)tracks.unshift({name,points});}
  sessions.push(decorate({key,format:'legacy',label:(first.session_id||'Sessione')+' / '+(first.boot_id||'boot'),start:time(first),end:time(data[data.length-1]),samples:data.map(time),tracks,warnings:[],metadata:null,
   events:rs.filter(r=>r.event&&finite(r.device_us)).map(r=>({t:time(r),name:r.event})),utcPoints:[]}));
 }if(!sessions.length)throw Error('Nessun campione con timestamp valido.');return sessions;
}
function metadataFor(docs,fileName,first,last){
 if(!docs?.length)return null;
 let start=null,end=null,id=null;
 for(const d of docs){
  if(!d||typeof d!=='object'||Array.isArray(d)||typeof d.id!=='string'||!/^S[0-9a-f]{16}$/i.test(d.id)||!['open','closed'].includes(d.state))throw Error('Metadati non riconosciuti: usare i JSON di apertura/chiusura SPORT LOGGER.');
  if(d.schema!==undefined&&d.schema!==1)throw Error('Versione dello schema metadati non supportata.');
  for(const key of ['sport','setup','reason','firmware','clock_source'])if(d[key]!==undefined&&typeof d[key]!=='string')throw Error('Metadati: '+key+' deve essere testo.');
  if(id&&id!==d.id)throw Error('I metadati appartengono a sessioni diverse.');id=d.id;
  if(d.state==='open'){if(start)throw Error('Due metadati di apertura: selezionarne uno solo.');start=d;}else{if(end)throw Error('Due metadati di chiusura: selezionarne uno solo.');end=d;}
 }
 const stem=(fileName||'').replace(/\.csv$/i,'');
 if(/^S[0-9a-f]{16}$/i.test(stem)&&stem!==id)throw Error('ID dei metadati diverso dal CSV caricato.');
 const m={...(start||{}),...(end||{})};
 if(!Number.isSafeInteger(m.start_mono_us)||m.start_mono_us<0||m.start_mono_us>first)throw Error('Inizio nei metadati incompatibile con i timestamp CSV.');
 if(start&&end&&start.start_mono_us!==end.start_mono_us)throw Error('Metadati di apertura e chiusura incoerenti.');
 if(end&&(!Number.isSafeInteger(end.end_mono_us)||end.end_mono_us<last))throw Error('Fine nei metadati incompatibile con il CSV.');
 return m;
}
function modernSession(headers,rs,options,warnings){
 for(let i=0;i<rs.length;i++){
  const t=number(rs[i].mono_us),seq=number(rs[i].seq);
  if(!Number.isSafeInteger(t)||t<0||!Number.isSafeInteger(seq)||seq<0)throw Error('CSV: timestamp o sequenza non validi alla riga '+(i+2)+'.');
  if(i&&t<=+rs[i-1].mono_us)throw Error('Timestamp duplicati o non crescenti nella sessione.');
  if(i&&seq<=+rs[i-1].seq)throw Error('Sequenza non crescente: non concatenare sessioni V1.7.');
 }
 const first=rs[0],last=rs[rs.length-1],metadata=metadataFor(options.metadata,options.fileName,+first.mono_us,+last.mono_us);
 if(finite(first.elapsed_ms)&&+first.elapsed_ms<0)throw Error('Tempo di sessione negativo.');
 if(metadata&&finite(first.elapsed_ms)){
  const delta=+first.mono_us-+first.elapsed_ms*1000-metadata.start_mono_us;
  if(delta<0||delta>=1000)throw Error('Origine nei metadati incompatibile con elapsed_ms del CSV.');
 }
 const origin=metadata?metadata.start_mono_us:+first.mono_us-(finite(first.elapsed_ms)?+first.elapsed_ms*1000:0);
 const time=r=>(+r.mono_us-origin)/1e6,samples=rs.map(time);
 const excluded=new Set(['seq','mono_us','elapsed_ms','utc_ms','clock_source','gps_utc_ms','trigger','trigger_id','trigger_click_mono_us']);
 const imuFields=/^(a[xyz]_g|g[xyz](_raw)?_dps|imu_temp_c|roll_deg|pitch_deg|z_relative_deg|roll_corrected_deg|pitch_corrected_deg|z_corrected_deg)$/;
 function value(r,name){
  if(BOOLS.has(name)){if(String(r[name]??'').trim()==='')return NaN;return flag(r[name])?1:0;}
  if(imuFields.test(name)&&!flag(r.imu_valid))return NaN;
  if(['bme_temp_c','pressure_hpa','humidity_pct'].includes(name)&&(!flag(r.bme_valid)||(finite(r.bme_age_ms)&&+r.bme_age_ms>=3000)))return NaN;
  if(['latitude_deg','longitude_deg','gps_speed_mps','gps_course_deg','gps_altitude_m'].includes(name)&&(!flag(r.gps_fix_valid)||(finite(r.gps_location_age_ms)&&+r.gps_location_age_ms>=3000)))return NaN;
  const v=number(r[name]);
  if(name==='latitude_deg'&&Math.abs(v)>90||name==='longitude_deg'&&Math.abs(v)>180)return NaN;
  return v;
 }
 const names=headers.filter(h=>!excluded.has(h));
 const tracks=names.map(name=>({name,points:rs.map((r,i)=>({t:samples[i],v:value(r,name)}))}));
 const events=[];let badTriggers=0,previousId=0,sequenceGaps=0;
 for(let i=0;i<rs.length;i++){
  const r=rs[i];if(i&&+r.seq>+rs[i-1].seq+1)sequenceGaps+=+r.seq-+rs[i-1].seq-1;
  if(!flag(r.trigger))continue;
  const click=number(r.trigger_click_mono_us),id=number(r.trigger_id);
  const validClick=Number.isSafeInteger(click)&&click>0&&click>=origin-1000&&click<=+r.mono_us;
  if(!validClick)badTriggers++;
  events.push({t:validClick?Math.max(0,(click-origin)/1e6):samples[i],sampleT:samples[i],id:finite(id)?id:null,
   kind:'trigger',name:'Trigger '+(finite(id)?id:events.length+1)+(validClick?' · ricezione ESP':' · campione (tempo ricezione assente)'),basis:validClick?'click':'sample'});
  if(Number.isFinite(id)&&previousId&&id<=previousId)warnings.push('ID trigger non crescente: verificare gli eventi del CSV.');
  if(Number.isFinite(id))previousId=id;
 }
 events.sort((a,b)=>a.t-b.t);
 if(badTriggers)warnings.push(badTriggers+' trigger senza tempo di ricezione valido: usato il tempo del campione.');
 if(sequenceGaps)warnings.push(sequenceGaps+' righe mancanti nella sequenza CSV.');
 if(!metadata)warnings.push('Metadati non caricati: chiusura non verificata; origine ricavata da elapsed_ms (precisione ~1 ms).');
 else if(metadata.state!=='closed')warnings.push('Solo metadati iniziali: chiusura della sessione non verificata.');
 else {
  if(metadata.csv_sync_ok===false)warnings.push('Il firmware segnala un errore di sincronizzazione del CSV.');
  if(finite(metadata.rows)&&metadata.rows!==rs.length)warnings.push('Numero righe diverso dal riepilogo finale.');
  if(number(metadata.triggers_lost)>0)warnings.push(metadata.triggers_lost+' trigger non scritti secondo i metadati.');
 }
 const utcPoints=rs.map((r,i)=>({t:samples[i],v:number(r.utc_ms)}));
 const name=options.fileName||metadata?.id||'SPORT LOGGER';
 return decorate({key:metadata?.id||name,format:'sport-logger-1.7',label:name,start:Math.min(0,samples[0]),end:samples[samples.length-1],samples,tracks,events,warnings,metadata,origin,utcPoints,sequenceGaps});
}
function lowerBound(points,t){let lo=0,hi=points.length;while(lo<hi){const m=(lo+hi)>>1;if(points[m].t<t)lo=m+1;else hi=m;}return lo;}
function valueAt(track,t){
 if(!track?.points.length)return null;
 let i=lowerBound(track.points,t);if(i===track.points.length||track.points[i].t>t+1e-7)--i;
 if(i<0)return null;const p=track.points[i];
 return t-p.t>(track.maxGap||.1)+1e-7||!Number.isFinite(p.v)?null:p.v;
}
function reading(s,name,t){return valueAt(s?.byName[name],t);}
function utcAt(s,t){
 const v=valueAt({points:s?.utcPoints||[],maxGap:.1},t);if(v&&v>0&&v<=8640000000000000)return {ms:v,source:'CSV'};
 const m=s?.metadata;
 const reconstructed=m&&number(m.start_utc_ms)>0?+m.start_utc_ms+t*1000:NaN;
 return Number.isFinite(reconstructed)&&reconstructed>0&&reconstructed<=8640000000000000?{ms:reconstructed,source:'ricostruito dai metadati'}:null;
}
function route(s){
 const lat=s.byName.latitude_deg,lon=s.byName.longitude_deg;if(!lat||!lon)return null;
 const points=[];let previous=null,broken=true,lastT=null,base=null;
 for(let i=0;i<lat.points.length;i++){
  const a=lat.points[i],b=lon.points[i];
  if(!Number.isFinite(a.v)||!Number.isFinite(b?.v)){broken=true;lastT=null;continue;}
  if(lastT!==null&&a.t-lastT>3)broken=true;lastT=a.t;
  if(!base)base={lat:a.v,lon:b.v};
  if(!broken&&previous&&previous.lat===a.v&&previous.lon===b.v)continue;
  const p={t:a.t,lat:a.v,lon:b.v,break:broken,...project(base,a.v,b.v)};
  points.push(p);previous=p;broken=false;
 }
 return points.length?{base,points}:null;
}
function project(base,lat,lon){return {x:((lon-base.lon+540)%360-180)*Math.PI/180*6371000*Math.cos(base.lat*Math.PI/180),y:(lat-base.lat)*Math.PI/180*6371000};}
function mapping(anchors){if(!anchors.length)throw Error('Seleziona un hook.');let slope=1,offset=anchors[0].video-anchors[0].data;if(anchors.length>1){const a=anchors[0],b=anchors[1];if(Math.abs(b.data-a.data)<.01)throw Error('Hook dati troppo vicini.');slope=(b.video-a.video)/(b.data-a.data);if(!Number.isFinite(slope)||slope<=0)throw Error('Gli hook devono avere lo stesso ordine temporale.');offset=a.video-slope*a.data;}return {slope,offset,toVideo:t=>slope*t+offset,toData:t=>(t-offset)/slope};}
function nearest(xs,t,dir){let lo=0,hi=xs.length;while(lo<hi){const m=(lo+hi)>>1;if(xs[m]<=t+1e-7)lo=m+1;else hi=m;}if(dir>0)return xs[Math.min(xs.length-1,lo)];let i=Math.max(0,lo-1);if(xs[i]>=t-1e-7)i=Math.max(0,i-1);return xs[i];}
const api={parse,mapping,nearest,info,GROUPS,lowerBound,valueAt,reading,utcAt,project};if(typeof module!=='undefined')module.exports=api;root.FoilCore=api;
})(typeof globalThis!=='undefined'?globalThis:this);
