(function(root){
'use strict';
function timecode(seconds){if(!Number.isFinite(seconds))return '—';const sign=seconds<0?'−':'',ms=Math.round(Math.abs(seconds)*1000);return sign+String(Math.floor(ms/3600000)).padStart(2,'0')+':'+String(Math.floor(ms/60000)%60).padStart(2,'0')+':'+String(Math.floor(ms/1000)%60).padStart(2,'0')+'.'+String(ms%1000).padStart(3,'0');}
// Read atom headers and mvhd only; never load the full video or mdat payload.
async function creationTime(file){
 let count=0;
 async function scan(start,end,depth){
  if(depth>2)return null;
  for(let pos=start;pos+8<=end;){
   if(++count>10000)throw Error('Troppi atom MP4');
   const b=await file.slice(pos,Math.min(pos+40,end)).arrayBuffer(),v=new DataView(b);
   let size=v.getUint32(0),header=8;const type=String.fromCharCode(...new Uint8Array(b,4,4));
   if(size===1){if(b.byteLength<16)throw Error('Atom MP4 incompleto');size=Number(v.getBigUint64(8));header=16;}
   if(size===0)size=end-pos;
   if(!Number.isSafeInteger(size)||size<header||pos+size>end)throw Error('Struttura MP4 non valida');
   if(type==='mvhd'){
    if(size<header+12||b.byteLength<header+12)return null;
    const version=v.getUint8(header);if(version>1)return null;
    const seconds=version===1?Number(v.getBigUint64(header+4)):v.getUint32(header+4);
    const ms=(seconds-2082844800)*1000;
    return seconds&&Number.isFinite(ms)&&ms>=0&&ms<=Date.now()+86400000?ms:null;
   }
   if(type==='moov'){const found=await scan(pos+header,pos+size,depth+1);if(found!==null)return found;}
   pos+=size;
  }return null;
 }return scan(0,file.size,0);
}
const api={timecode,creationTime};root.FoilMedia=api;if(typeof module!=='undefined')module.exports=api;
})(typeof globalThis!=='undefined'?globalThis:this);
