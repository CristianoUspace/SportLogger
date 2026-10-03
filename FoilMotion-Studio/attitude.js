(function(root){
'use strict';
// Sensor convention: X toward nose, Y toward port, Z upward.
// Front view looks from nose toward tail; side view has nose on right.
function update(prefix,angles,acc,gyro,resolve=id=>document.getElementById(id)){
 const axes=['roll','pitch','yaw'];
 axes.forEach((axis,i)=>{
  const get=s=>resolve(prefix+axis+s),a=angles[i],valid=Number.isFinite(a);
  get('Board').setAttribute('transform',`rotate(${valid?(i===1?a:-a):0} 120 70)`);
  get('Board').style.opacity=valid?'1':'.18';
  get('Angle').textContent=valid?a.toFixed(1)+'°':'—';
  get('Values').textContent='Acc '+ 'XYZ'[i]+': '+(Number.isFinite(acc[i])?acc[i].toFixed(3):'—')+' g · Gyro '+ 'XYZ'[i]+': '+(Number.isFinite(gyro[i])?gyro[i].toFixed(2):'—')+' °/s';
 });
}
root.FoilAttitude={update};
})(typeof globalThis!=='undefined'?globalThis:this);
