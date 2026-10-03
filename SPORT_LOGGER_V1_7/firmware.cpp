// SPORT LOGGER V1.7 - ESP32-WROOM-32. All mutable state belongs to loop().
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <TinyGPSPlus.h>
#include <Adafruit_BME280.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include "config.h"
#include "core.h"
#include "ui.h"

using namespace Config;
WebServer server(80);
HardwareSerial gnss(2);
TinyGPSPlus gps;
Adafruit_BME280 bme;
Preferences prefs;
Core::Debounce magnet;
Core::TriggerQueue triggers;
uint64_t nowUs() { return esp_timer_get_time(); }
void logEvent(const char *tag,const char *fmt,...) {
  if(!SERIAL_LOG_ENABLED) return;
  char message[256]; va_list args; va_start(args,fmt);
  vsnprintf(message,sizeof message,fmt,args); va_end(args);
  Serial.printf("[%10llu ms] [%-6s] %s\n",(unsigned long long)(nowUs()/1000),tag,message);
}
struct I2cTransfer { uint8_t addr=0,reg=0,tx=0; unsigned got=0,want=0; } lastI2c;
const char *wireResult(uint8_t code) {
  switch(code) {
    case 0:return "OK"; case 1:return "buffer"; case 2:return "NACK/no ACK";
    case 3:return "data NACK"; case 4:return "bus/driver error"; case 5:return "timeout";
    default:return "unknown";
  }
}
void logI2cFailure(const char *tag) {
  logEvent(tag,"I2C addr=0x%02X reg=0x%02X tx=%u (%s), RX=%u/%u bytes; SDA=%d SCL=%d",
    lastI2c.addr,lastI2c.reg,lastI2c.tx,wireResult(lastI2c.tx),lastI2c.got,lastI2c.want,
    digitalRead(SDA_PIN),digitalRead(SCL_PIN));
  // With repeated START, ESP32 defers the transaction to requestFrom(). tx=0 alone is not an ACK.
}
void scanI2c() {
  logEvent("I2C","SCAN begin: SDA=GPIO%d level=%d SCL=GPIO%d level=%d clock=%lu Hz",
    SDA_PIN,digitalRead(SDA_PIN),SCL_PIN,digitalRead(SCL_PIN),(unsigned long)Wire.getClock());
  unsigned found=0,nack=0,errors=0;
  for(uint8_t addr=8;addr<120;addr++) {
    Wire.beginTransmission(addr); uint8_t code=Wire.endTransmission(true);
    if(!code) {
      ++found;
      const char *hint=(addr==0x68||addr==0x69)?"possible MPU6050":(addr==0x76||addr==0x77)?"possible BME/BMP":"other device";
      logEvent("I2C","ACK 0x%02X (%s; chip ID must still be checked)",addr,hint);
      if((addr==0x68||addr==0x69) && addr!=IMU_ADDR)
        logEvent("IMU","Address differs from configured 0x%02X: check AD0 / IMU_ADDR in config.h",IMU_ADDR);
    } else if(code==2) ++nack;
    else { if(errors<4) logEvent("I2C","addr=0x%02X error=%u (%s)",addr,code,wireResult(code)); ++errors; }
    delay(1);
  }
  logEvent("I2C","SCAN end: found=%u no_ACK=%u other_errors=%u",found,nack,errors);
  if(!found) logEvent("I2C","No ACK: check common GND, SDA21/SCL22, swapped wires and pull-ups to 3.3V. Power LED is not a bus test.");
  if(digitalRead(SDA_PIN)==LOW||digitalRead(SCL_PIN)==LOW)
    logEvent("I2C","Bus line LOW at rest: possible short, missing pull-up or device holding bus. Inspect before changing wires (power off).");
}
struct SensorLogState { bool known=false,value=false; uint64_t next=0; } imuLog,bmeLog;
void sensorTransition(const char *tag,bool valid,SensorLogState &s) {
  uint64_t t=nowUs();
  if(s.known && (s.value==valid || t<s.next)) return;
  s.known=true; s.value=valid; s.next=t+2000000;
  logEvent(tag,"Runtime data %s",valid?"OK":"UNAVAILABLE");
  if(!valid) logI2cFailure(tag);
}
bool recording=false, stopping=false, sdOK=false, imuOK=false, bmeOK=false, calibrated=false;
bool attitudeReady=false, calibrating=false, prefsOK=false, imuInitialized=false;
String problem, stopReason, sessionId, bootId, calStatus="Non calibrata";
String sport="Foilscoot", setupNote;
FILE *csv=nullptr;
uint64_t started=0, ended=0, nextSample=0, lastSample=0, lastSlow=0, lastFlush=0;
uint64_t totalBytes=0, freeBytes=0, rows=0, missed=0, triggerWritten=0, triggerAccepted=0;
uint64_t bmeAt=0, ledUntil=0;
int64_t clockEpoch=0; uint64_t clockAt=0; String clockSource="none";
float acc[3]={NAN,NAN,NAN}, gyro[3]={NAN,NAN,NAN}, gyroBias[3]={};
float imuTemp=NAN, pitch=NAN, roll=NAN, yaw=0, offsets[3]={};
float bmeTemp=NAN, humidity=NAN, pressure=NAN, battery=NAN;
uint8_t bmeAddress=0;
unsigned calN=0; double calMean[6]={}, calM2[6]={}; uint64_t calStart=0;
uint32_t batterySum=0; unsigned batteryN=0; uint64_t batteryAt=0;

int64_t utc(uint64_t t) { return clockEpoch ? clockEpoch+(int64_t(t)-int64_t(clockAt))/1000 : 0; }
String u64(uint64_t x) { char b[24]; snprintf(b,sizeof b,"%llu",(unsigned long long)x); return b; }
String i64(int64_t x) { char b[24]; snprintf(b,sizeof b,"%lld",(long long)x); return b; }
bool fresh(bool valid, uint32_t age) { return valid && age<3000; }
float corrected(float v, int axis) { return isfinite(v)?Core::wrap(v-offsets[axis]):NAN; }
void reply(int code, const String &message) {
  if(code>=400) logEvent("HTTP","%s -> %d: %s",server.uri().c_str(),code,message.c_str());
  StaticJsonDocument<256> d; d["message"]=message;
  String out; serializeJson(d,out); server.send(code,"application/json",out);
}
void jsonReply(JsonDocument &d) {
  String out; serializeJson(d,out); server.sendHeader("Cache-Control","no-store");
  server.send(200,"application/json",out);
}
String pathFor(const String &id, const char *suffix) { return "/sessions/"+id+suffix; }
String nativePath(const String &path) { return "/sd"+path; }
bool syncFile(FILE *f) {
  if(fflush(f)!=0) { logEvent("SD","fflush failed errno=%d",errno); return false; }
  if(fsync(fileno(f))!=0) { logEvent("SD","fsync failed errno=%d",errno); return false; }
  if(ferror(f)) { logEvent("SD","File stream reports write error"); return false; }
  return true;
}
bool writeJSON(const String &path, JsonDocument &d) {
  String body; serializeJson(d,body);
  FILE *f=fopen(nativePath(path).c_str(),"wb");
  if(!f) { logEvent("SD","Metadata open failed: %s errno=%d",path.c_str(),errno); return false; }
  bool ok=fwrite(body.c_str(),1,body.length(),f)==body.length();
  ok=syncFile(f)&&ok; if(fclose(f)!=0) ok=false;
  if(!ok) logEvent("SD","Metadata write/sync/close failed: %s",path.c_str());
  return ok;
}
bool readJSON(const String &path, JsonDocument &d) {
  File f=SD.open(path); if(!f) return false;
  auto e=deserializeJson(d,f); f.close(); return !e;
}
void refreshSpace() {
  if(!sdOK) return;
  totalBytes=SD.totalBytes(); uint64_t used=SD.usedBytes();
  if(!totalBytes || used>totalBytes) {
    logEvent("SD","Invalid filesystem space: total=%llu used=%llu",(unsigned long long)totalBytes,(unsigned long long)used);
    sdOK=false; freeBytes=0;
  }
  else freeBytes=totalBytes-used;
}
bool mountSD() {
  logEvent("SD","MOUNT begin: SCK=%d MISO=%d MOSI=%d CS=%d clock=%lu Hz; FAT32; format=OFF",
    SD_SCK,SD_MISO,SD_MOSI,SD_CS,(unsigned long)SD_SPI_HZ);
  uint64_t t=nowUs();
  SD.end(); sdOK=false; totalBytes=freeBytes=0;
  if(!SD.begin(SD_CS,SPI,SD_SPI_HZ,"/sd",8,false)) {
    logEvent("SD","SD.begin FAILED after %llu ms (driver combines card init and filesystem mount)",(unsigned long long)((nowUs()-t)/1000));
    logEvent("SD","Check CS=GPIO%d (not default GPIO5), common GND, SCK/MOSI/MISO and reader power/logic. FAT32 alone does not prove SPI works.",SD_CS);
    logEvent("SD","For driver details: Arduino Tools > Core Debug Level > Verbose, rebuild. Optional next test: SD_SPI_HZ=1000000.");
    return false;
  }
  uint8_t type=SD.cardType();
  logEvent("SD","Card type=%s raw_capacity=%llu MiB",type==CARD_SDHC?"SDHC/SDXC":type==CARD_SD?"SDSC":type==CARD_MMC?"MMC":"NONE/UNKNOWN",
    (unsigned long long)(SD.cardSize()/1048576));
  if(type==CARD_NONE) { logEvent("SD","FAIL: driver reports no card"); return false; }
  if(!SD.exists("/sessions")) {
    if(!SD.mkdir("/sessions")) { logEvent("SD","FAIL: cannot create /sessions (filesystem write)"); return false; }
    logEvent("SD","Created /sessions");
  }
  File dir=SD.open("/sessions"); bool isDir=dir && dir.isDirectory(); if(dir) dir.close();
  if(!isDir) { logEvent("SD","FAIL: /sessions is not a readable directory"); return false; }
  sdOK=true; refreshSpace();
  logEvent("SD","MOUNT %s: filesystem=%llu MiB free=%llu MiB elapsed=%llu ms",sdOK?"OK":"FAILED",
    (unsigned long long)(totalBytes/1048576),(unsigned long long)(freeBytes/1048576),(unsigned long long)((nowUs()-t)/1000));
  return sdOK;
}
void addConfig(JsonDocument &d) {
  d["firmware"]=FIRMWARE_VERSION; d["schema"]=1; d["sport"]=sport; d["setup"]=setupNote;
  d["sample_period_us"]=SAMPLE_US; d["imu_accel_range_g"]=8; d["imu_gyro_range_dps"]=500;
  d["calibrated"]=calibrated; d["battery_cells"]=BATTERY_CELLS;
  d["battery_factor"]=BATTERY_FACTOR; d["divider_top_ohm"]=R_TOP; d["divider_bottom_ohm"]=R_BOTTOM;
  for(int i=0;i<3;i++) { d["gyro_bias_dps"][i]=gyroBias[i]; d["offsets_deg"][i]=offsets[i]; }
  d["axis_convention"]="MPU chip axes; Z relative gyro integral, not compass heading";
}
void finish(const String &reason) {
  if(!recording) return;
  recording=false; stopping=false; ended=nowUs(); stopReason=reason;
  bool ok=true;
  if(csv) { ok=syncFile(csv); if(fclose(csv)!=0) ok=false; csv=nullptr; }
  if(!ok) stopReason="sd_flush_or_close_error";
  StaticJsonDocument<2048> d;
  d["id"]=sessionId; d["state"]="closed"; d["reason"]=stopReason; d["stop_requested_reason"]=reason;
  d["start_mono_us"]=started; d["end_mono_us"]=ended; d["duration_ms"]=(ended-started)/1000;
  d["start_utc_ms"]=utc(started); d["end_utc_ms"]=utc(ended); d["clock_source"]=clockSource;
  d["rows"]=rows; d["missed_samples"]=missed; d["triggers_accepted"]=triggerAccepted;
  d["triggers_written"]=triggerWritten; d["triggers_lost"]=triggers.count;
  d["csv_sync_ok"]=ok;
  // Initial metadata is never overwritten: missing/corrupt final metadata means interrupted.
  bool meta=writeJSON(pathFor(sessionId,".end.json"),d);
  if(!ok || !meta || reason.startsWith("sd_")) {
    sdOK=false; problem=stopReason+(meta?"":"; final_metadata_failed");
  } else problem=reason=="manual" || reason=="magnet" ? "" : reason;
  logEvent("REC","STOP id=%s reason=%s rows=%llu missed=%llu trigger_written=%llu pending_lost=%u CSV_sync=%s metadata=%s",
    sessionId.c_str(),stopReason.c_str(),(unsigned long long)rows,(unsigned long long)missed,
    (unsigned long long)triggerWritten,triggers.count,ok?"OK":"FAIL",meta?"OK":"FAIL");
  triggers.clear(); refreshSpace();
}
void requestStop(const String &reason) {
  logEvent("REC","STOP requested source=%s recording=%d pending_triggers=%u",reason.c_str(),recording,triggers.count);
  if(!recording) return;
  stopReason=reason;
  if(triggers.count) stopping=true; // Drain already accepted trigger events before normal stop.
  else finish(reason);
}

bool regWrite(uint8_t reg,uint8_t value) {
  Wire.beginTransmission(IMU_ADDR); Wire.write(reg); Wire.write(value);
  uint8_t code=Wire.endTransmission();
  if(code) logEvent("IMU","Register write FAILED reg=0x%02X value=0x%02X code=%u (%s)",reg,value,code,wireResult(code));
  return code==0;
}
bool readRegisters(uint8_t addr,uint8_t reg,uint8_t *data,uint8_t n) {
  lastI2c.addr=addr; lastI2c.reg=reg; lastI2c.want=n; lastI2c.got=0;
  Wire.beginTransmission(addr); Wire.write(reg);
  lastI2c.tx=Wire.endTransmission(false);
  if(lastI2c.tx!=0) return false;
  lastI2c.got=Wire.requestFrom(addr,n,(uint8_t)true);
  if(lastI2c.got!=n) return false;
  for(unsigned i=0;i<n;i++) data[i]=Wire.read();
  return true;
}
bool initIMU() {
  logEvent("IMU","INIT begin: configured address=0x%02X",IMU_ADDR);
  uint8_t who=0;
  if(!readRegisters(IMU_ADDR,0x75,&who,1)) { logI2cFailure("IMU"); return false; }
  logEvent("IMU","WHO_AM_I=0x%02X (MPU6050 expected 0x68)",who);
  if((who&0x7e)!=0x68) { logEvent("IMU","FAIL: unexpected chip ID; check module model"); return false; }
  // Wake, DLPF 44 Hz, 100 Hz sensor output, +/-8g, +/-500 deg/s.
  bool ok=regWrite(0x6b,1) && regWrite(0x1a,3) && regWrite(0x19,9) && regWrite(0x1c,0x10) && regWrite(0x1b,8);
  logEvent("IMU","INIT %s: accel +/-8g, gyro +/-500 dps, DLPF 44Hz",ok?"OK":"FAIL");
  return ok;
}
void initBME() {
  bmeAddress=0; bmeOK=false;
  for(uint8_t addr:{uint8_t(0x76),uint8_t(0x77)}) {
    uint8_t id=0;
    logEvent("BME","Probe addr=0x%02X chip ID register=0xD0",addr);
    if(!readRegisters(addr,0xd0,&id,1)) { logI2cFailure("BME"); continue; }
    logEvent("BME","addr=0x%02X chip ID=0x%02X (BME280 expected 0x60)",addr,id);
    if(id!=0x60) { logEvent("BME","Not BME280: BMP280 often reports 0x58 and has no humidity"); continue; }
    if(!bme.begin(addr,&Wire)) { logEvent("BME","Adafruit begin FAILED at 0x%02X",addr); continue; }
    bmeAddress=addr; break;
  }
  if(!bmeAddress) { logEvent("BME","INIT FAILED: no BME280 initialized at 0x76/0x77"); return; }
  bme.setSampling(Adafruit_BME280::MODE_NORMAL,Adafruit_BME280::SAMPLING_X1,
    Adafruit_BME280::SAMPLING_X1,Adafruit_BME280::SAMPLING_X1,Adafruit_BME280::FILTER_OFF,Adafruit_BME280::STANDBY_MS_1000);
  logEvent("BME","INIT OK addr=0x%02X; first runtime measurement within ~1 second",bmeAddress);
}
bool readIMU() {
  uint8_t b[14]; if(!readRegisters(IMU_ADDR,0x3b,b,14)) return false;
  int16_t v[7]; for(int i=0;i<7;i++) v[i]=(int16_t)((uint16_t(b[i*2])<<8)|b[i*2+1]);
  for(int i=0;i<3;i++) { acc[i]=v[i]/4096.f; gyro[i]=v[i+4]/65.5f; }
  imuTemp=v[3]/340.f+36.53f; return true;
}
void calibrationStep(uint64_t t) {
  if(!calibrating) return;
  if(!imuOK || t-calStart>10000000) { calibrating=false; calStatus="Fallita: IMU/timeout"; logEvent("CAL","FAILED: IMU/timeout, samples=%u",calN); return; }
  ++calN;
  for(int i=0;i<6;i++) {
    double x=i<3?acc[i]:gyro[i-3], delta=x-calMean[i];
    calMean[i]+=delta/calN; calM2[i]+=delta*(x-calMean[i]);
  }
  if(calN<150) return;
  calibrating=false; bool valid=true;
  for(int i=0;i<6;i++) if(sqrt(calM2[i]/(calN-1))>(i<3?0.025:0.8)) valid=false;
  double norm=sqrt(calMean[0]*calMean[0]+calMean[1]*calMean[1]+calMean[2]*calMean[2]);
  for(int i=3;i<6;i++) if(fabs(calMean[i])>5) valid=false;
  if(norm<0.85 || norm>1.15) valid=false;
  if(!valid) { calStatus="Fallita: tenere ferma la tavola"; logEvent("CAL","FAILED: motion/bias/gravity check; norm=%.3f g",norm); return; }
  for(int i=0;i<3;i++) gyroBias[i]=calMean[i+3];
  calibrated=true; yaw=0; attitudeReady=false;
  bool saved=prefsOK && prefs.putBytes("bias",gyroBias,sizeof gyroBias)==sizeof gyroBias;
  calStatus=saved?"Calibrata e salvata":"Calibrata; salvataggio NVS fallito";
  logEvent("CAL","OK bias_dps=(%.4f,%.4f,%.4f) NVS=%s",gyroBias[0],gyroBias[1],gyroBias[2],saved?"OK":"FAIL");
}
void sampleIMU(uint64_t t) {
  imuOK=imuInitialized && readIMU();
  if(imuInitialized) sensorTransition("IMU",imuOK,imuLog);
  if(!imuOK) {
    for(int i=0;i<3;i++) acc[i]=gyro[i]=NAN;
    imuTemp=pitch=roll=NAN; attitudeReady=false;
  } else {
    float ar=atan2f(acc[1],acc[2])*RAD_TO_DEG;
    float ap=atan2f(-acc[0],hypotf(acc[1],acc[2]))*RAD_TO_DEG;
    float dt=lastSample?(t-lastSample)/1e6f:0;
    if(!attitudeReady || dt>0.1f) { roll=ar; pitch=ap; attitudeReady=true; }
    else {
      float alpha=0.5f/(0.5f+dt);
      roll=Core::wrap(roll+(gyro[0]-gyroBias[0])*dt);
      pitch=Core::wrap(pitch+(gyro[1]-gyroBias[1])*dt);
      // Trust accelerometer only near gravity; acceleration during pumping can tilt the estimate.
      float norm=sqrtf(acc[0]*acc[0]+acc[1]*acc[1]+acc[2]*acc[2]);
      if(norm>0.8f && norm<1.2f) {
        roll=Core::wrap(roll+(1-alpha)*Core::wrap(ar-roll));
        pitch=Core::wrap(pitch+(1-alpha)*Core::wrap(ap-pitch));
      }
      yaw=Core::wrap(yaw+(gyro[2]-gyroBias[2])*dt);
    }
  }
  lastSample=t; calibrationStep(t);
}
void gpsTick() {
  unsigned limit=2048;
  while(gnss.available() && limit--) gps.encode(gnss.read());
  // Latch once per boot. Never jump timestamps during a recording. GPS may anchor a session begun without UTC.
  if(!clockEpoch && gps.date.isValid() && gps.time.isValid() && gps.date.age()<1500 && gps.time.age()<1500
     && gps.date.year()>=2024 && gps.date.year()<2100) {
    clockAt=nowUs(); clockEpoch=Core::epochMs(gps.date.year(),gps.date.month(),gps.date.day(),
      gps.time.hour(),gps.time.minute(),gps.time.second(),gps.time.centisecond())+gps.time.age();
    clockSource="gps";
  }
}
void slowSensors(uint64_t t) {
  if(BATTERY_ENABLED && t-batteryAt>=25000) {
    batteryAt=t; batterySum+=analogReadMilliVolts(BATTERY_PIN);
    if(++batteryN==32) {
      float mv=batterySum/32.f;
      float v=mv*0.001f*(R_TOP+R_BOTTOM)/R_BOTTOM*BATTERY_FACTOR;
      battery=(mv>=100 && mv<=3000 && v>BATTERY_CELLS*2.0f && v<BATTERY_CELLS*4.5f)?v:NAN;
      batteryN=0; batterySum=0;
    }
  }
  if(t-lastSlow<1000000) return;
  lastSlow=t;
  uint8_t raw[8];
  bmeOK=bmeAddress && readRegisters(bmeAddress,0xf7,raw,8);
  if(bmeOK) {
    bmeTemp=bme.readTemperature(); pressure=bme.readPressure()/100.f; humidity=bme.readHumidity();
    bmeOK=isfinite(bmeTemp)&&isfinite(pressure)&&isfinite(humidity)&&pressure>100&&pressure<1200&&humidity>=0&&humidity<=100;
    if(bmeOK) bmeAt=t;
  }
  if(!bmeOK) bmeTemp=pressure=humidity=NAN;
  if(bmeAddress) sensorTransition("BME",bmeOK,bmeLog);
}

const char HEADER[]="seq,mono_us,elapsed_ms,utc_ms,clock_source,imu_valid,ax_g,ay_g,az_g,gx_raw_dps,gy_raw_dps,gz_raw_dps,gx_dps,gy_dps,gz_dps,imu_temp_c,roll_deg,pitch_deg,z_relative_deg,roll_corrected_deg,pitch_corrected_deg,z_corrected_deg,bme_valid,bme_age_ms,bme_temp_c,pressure_hpa,humidity_pct,battery_v,battery_warning,gps_fix_valid,latitude_deg,longitude_deg,gps_location_age_ms,gps_speed_mps,gps_course_deg,gps_altitude_m,gps_satellites,gps_hdop,gps_utc_ms,gps_time_age_ms,gps_chars,gps_checksum_failed,trigger,trigger_id,trigger_click_mono_us,missed_samples\n";
// Empty numeric fields denote invalid/unavailable values. Never serialize stale fixes as fresh.
void appendNum(String &s,double v,unsigned decimals=3) { s+=','; if(isfinite(v)) s+=String(v,decimals); }
void appendInt(String &s,uint64_t v) { s+=','; s+=u64(v); }
void appendGPS(String &s) {
  bool fix=fresh(gps.location.isValid(),gps.location.age()); appendInt(s,fix);
  appendNum(s,fix?gps.location.lat():NAN,7); appendNum(s,fix?gps.location.lng():NAN,7);
  appendNum(s,gps.location.isValid()?double(gps.location.age()):NAN,0);
  appendNum(s,fix&&fresh(gps.speed.isValid(),gps.speed.age())?gps.speed.mps():NAN);
  appendNum(s,fix&&fresh(gps.course.isValid(),gps.course.age())?gps.course.deg():NAN);
  appendNum(s,fix&&fresh(gps.altitude.isValid(),gps.altitude.age())?gps.altitude.meters():NAN);
  appendNum(s,fresh(gps.satellites.isValid(),gps.satellites.age())?double(gps.satellites.value()):NAN,0);
  appendNum(s,fresh(gps.hdop.isValid(),gps.hdop.age())?gps.hdop.hdop():NAN);
  s+=',';
  if(fresh(gps.date.isValid(),gps.date.age()) && fresh(gps.time.isValid(),gps.time.age()))
    s+=i64(Core::epochMs(gps.date.year(),gps.date.month(),gps.date.day(),gps.time.hour(),gps.time.minute(),gps.time.second(),gps.time.centisecond()));
  appendNum(s,gps.time.isValid()?double(gps.time.age()):NAN,0);
  appendInt(s,gps.charsProcessed()); appendInt(s,gps.failedChecksum());
}
void writeSample(uint64_t t) {
  if(!recording || !csv) return;
  String line;
  if(!line.reserve(768)) { finish("memory_error"); return; }
  line=u64(rows); appendInt(line,t); appendInt(line,(t-started)/1000);
  line+=','; if(utc(t)) line+=i64(utc(t)); line+=','+clockSource;
  appendInt(line,imuOK);
  for(float v:acc) appendNum(line,v,5);
  for(float v:gyro) appendNum(line,v,4);
  for(int i=0;i<3;i++) appendNum(line,gyro[i]-gyroBias[i],4);
  appendNum(line,imuTemp); appendNum(line,roll); appendNum(line,pitch); appendNum(line,imuOK?yaw:NAN);
  appendNum(line,corrected(roll,0)); appendNum(line,corrected(pitch,1)); appendNum(line,imuOK?corrected(yaw,2):NAN);
  appendInt(line,bmeOK); appendNum(line,bmeAt?double((t-bmeAt)/1000):NAN,0);
  appendNum(line,bmeTemp); appendNum(line,pressure); appendNum(line,humidity); appendNum(line,battery);
  appendNum(line,isfinite(battery)?double(battery<BATTERY_CELLS*WARNING_PER_CELL):NAN,0);
  appendGPS(line);
  bool event=triggers.count>0;
  line+=event?",true":",false"; appendInt(line,event?triggerWritten+1:0);
  appendInt(line,event?triggers.front():0); appendInt(line,missed); line+='\n';
  if(fwrite(line.c_str(),1,line.length(),csv)!=line.length() || ferror(csv)) { finish("sd_write_error"); return; }
  ++rows;
  if(event) { ++triggerWritten; triggers.pop(); logEvent("TRIG","Written id=%llu row=%llu",(unsigned long long)triggerWritten,(unsigned long long)(rows-1)); }
  if(stopping && !triggers.count) finish(stopReason);
}
bool startRecordingImpl() {
  if(recording) return true;
  if(calibrating) { problem="calibration_in_progress"; return false; }
  if(!sdOK) { problem="sd_unavailable_remount_required"; return false; }
  refreshSpace();
  if(!sdOK || freeBytes<=SD_RESERVE+65536) { problem="sd_full_or_unavailable"; return false; }
  char id[40];
  do { snprintf(id,sizeof id,"S%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random()); sessionId=id; }
  while(SD.exists(pathFor(sessionId,".csv")) || SD.exists(pathFor(sessionId,".json")));
  started=nowUs(); ended=0; rows=missed=triggerWritten=triggerAccepted=0; triggers.clear();
  stopReason=""; problem=""; stopping=false;
  StaticJsonDocument<2048> d; addConfig(d); d["id"]=sessionId; d["boot_id"]=bootId;
  d["state"]="open"; d["start_mono_us"]=started; d["start_utc_ms"]=utc(started); d["clock_source"]=clockSource;
  if(!writeJSON(pathFor(sessionId,".json"),d)) { sdOK=false; problem="sd_metadata_error"; return false; }
  csv=fopen(nativePath(pathFor(sessionId,".csv")).c_str(),"wb");
  if(!csv) { logEvent("SD","CSV fopen failed errno=%d",errno); sdOK=false; problem="sd_open_error"; return false; }
  recording=true;
  if(fwrite(HEADER,1,strlen(HEADER),csv)!=strlen(HEADER) || !syncFile(csv)) { finish("sd_header_error"); return false; }
  nextSample=nowUs(); lastFlush=nextSample; return true;
}
bool startRecording(const char *source) {
  logEvent("REC","START requested source=%s recording=%d SD=%d IMU=%d BME=%d",source,recording,sdOK,imuOK,bmeOK);
  bool ok=startRecordingImpl();
  logEvent("REC","START %s: %s",ok?"OK":"REJECTED",ok?sessionId.c_str():problem.c_str());
  return ok;
}

void status() {
  StaticJsonDocument<3072> d;
  d["version"]=FIRMWARE_VERSION; d["recording"]=recording; d["stopping"]=stopping; d["session"]=sessionId;
  d["error"]=problem; d["stop_reason"]=stopReason; d["rows"]=rows; d["missed"]=missed;
  d["duration_ms"]=started && (recording || ended>=started)?((recording?nowUs():ended)-started)/1000:0;
  d["sd_ok"]=sdOK; d["sd_free"]=freeBytes; d["sd_total"]=totalBytes;
  d["battery_v"]=battery; d["battery_warning"]=isfinite(battery)&&battery<BATTERY_CELLS*WARNING_PER_CELL;
  d["battery_cells"]=BATTERY_CELLS; d["imu_ok"]=imuOK; d["calibrating"]=calibrating; d["calibration"]=calStatus;
  d["roll"]=corrected(roll,0); d["pitch"]=corrected(pitch,1); d["z"]=imuOK?corrected(yaw,2):NAN;
  for(int i=0;i<3;i++) { d["acc"][i]=acc[i]; d["gyro"][i]=gyro[i]-gyroBias[i]; d["offsets"][i]=offsets[i]; }
  d["imu_temp"]=imuTemp; d["bme_ok"]=bmeOK; d["temperature"]=bmeTemp; d["humidity"]=humidity; d["pressure"]=pressure;
  bool fix=fresh(gps.location.isValid(),gps.location.age()); d["gps_fix"]=fix;
  d["lat"]=fix?gps.location.lat():NAN; d["lon"]=fix?gps.location.lng():NAN;
  d["satellites"]=fresh(gps.satellites.isValid(),gps.satellites.age())?double(gps.satellites.value()):NAN;
  d["speed_mps"]=fix&&fresh(gps.speed.isValid(),gps.speed.age())?gps.speed.mps():NAN;
  d["gps_chars"]=gps.charsProcessed(); d["gps_checksum_failed"]=gps.failedChecksum();
  d["gps_age_ms"]=gps.location.isValid()?double(gps.location.age()):NAN;
  d["utc_ms"]=utc(nowUs()); d["clock_source"]=clockSource; d["trigger_pending"]=triggers.count;
  d["trigger_written"]=triggerWritten; d["sport"]=sport; d["setup"]=setupNote; jsonReply(d);
}
bool idleOnly() { if(recording || calibrating) { reply(409,"Disponibile a registrazione e calibrazione ferme"); return false; } return true; }
bool body(StaticJsonDocument<1024> &d) {
  if(server.arg("plain").length()>1000 || deserializeJson(d,server.arg("plain"))) { reply(400,"JSON non valido"); return false; }
  return true;
}
bool validId(const String &id) {
  if(id.length()!=17 || id[0]!='S') return false;
  for(unsigned i=1;i<id.length();i++) if(!isxdigit((unsigned char)id[i])) return false;
  return true;
}
void files() {
  if(!idleOnly()) return;
  if(!sdOK) { reply(503,"SD non disponibile"); return; }
  int page=server.arg("page").toInt(); if(page<0 || page>100000) { reply(400,"Pagina non valida"); return; }
  DynamicJsonDocument d(16384); auto list=d.createNestedArray("files");
  File dir=SD.open("/sessions"); if(!dir) { reply(503,"Directory non leggibile"); return; }
  unsigned index=0, count=0; bool more=false;
  for(File f=dir.openNextFile();f;f=dir.openNextFile()) {
    String name=f.name(); uint64_t bytes=f.size(); bool isDir=f.isDirectory(); f.close();
    if(isDir || !name.endsWith(".csv")) continue;
    int slash=name.lastIndexOf('/'); if(slash>=0) name=name.substring(slash+1);
    String id=name.substring(0,name.length()-4); if(!validId(id)) continue;
    if(index++<(unsigned)page*20) continue;
    if(count++==20) { more=true; break; }
    DynamicJsonDocument start(2048), end(2048);
    bool hasStart=readJSON(pathFor(id,".json"),start), hasEnd=readJSON(pathFor(id,".end.json"),end);
    auto o=list.createNestedObject(); o["id"]=id; o["bytes"]=bytes;
    o["state"]=hasEnd?"closed":"interrupted";
    o["start_utc_ms"]=hasEnd?end["start_utc_ms"].as<int64_t>():start["start_utc_ms"].as<int64_t>();
    if(hasEnd) { o["end_utc_ms"]=end["end_utc_ms"]; o["duration_ms"]=end["duration_ms"]; o["reason"]=end["reason"]; }
    else { o["end_utc_ms"]=0; o["duration_ms"]=nullptr; o["reason"]=hasStart?"power_loss_or_unclosed":"missing_metadata"; }
    o["sport"]=start["sport"];
  }
  dir.close(); d["page"]=page; d["more"]=more; jsonReply(d);
}
void routes() {
  server.on("/",HTTP_GET,[]{ server.sendHeader("Cache-Control","no-store"); server.send_P(200,"text/html; charset=utf-8",UI); });
  server.on("/api/status",HTTP_GET,status);
  server.on("/api/record",HTTP_POST,[]{
    StaticJsonDocument<1024> d; if(!body(d)) return;
    if(!d["recording"].is<bool>()) { reply(400,"recording deve essere booleano"); return; }
    // Desired state, never blind toggle: retries and stale clients cannot invert it twice.
    if(d["recording"].as<bool>()) {
      if(stopping) { reply(409,"Arresto in corso"); return; }
      if(!startRecording("web")) { reply(409,problem); return; }
    } else requestStop("manual");
    status();
  });
  server.on("/api/trigger",HTTP_POST,[]{
    if(!recording || stopping) { reply(409,"Avviare prima una registrazione"); return; }
    uint64_t t=nowUs(); if(!triggers.push(t)) { reply(429,"Coda trigger piena"); return; }
    ++triggerAccepted; ledUntil=t+150000;
    logEvent("TRIG","Accepted id=%llu queue=%u",(unsigned long long)triggerAccepted,triggers.count);
    StaticJsonDocument<256> d; d["accepted"]=true; d["trigger_id"]=triggerAccepted; d["click_mono_us"]=t; jsonReply(d);
  });
  server.on("/api/calibrate",HTTP_POST,[]{
    if(!idleOnly()) return;
    if(!imuOK) { reply(503,"IMU assente"); return; }
    calibrating=true; calN=0; calStart=nowUs();
    logEvent("CAL","START from web: keep stationary for 3 seconds");
    memset(calMean,0,sizeof calMean); memset(calM2,0,sizeof calM2); calStatus="In corso: ferma per 3 secondi"; reply(202,calStatus);
  });
  server.on("/api/config",HTTP_POST,[]{
    if(!idleOnly()) return;
    StaticJsonDocument<1024> d; if(!body(d)) return;
    const char *s=d["sport"] | "";
    if(strcmp(s,"Foilscoot") && strcmp(s,"Wingfoil") && strcmp(s,"Kite") && strcmp(s,"Generic")) { reply(400,"Sport non valido"); return; }
    if(!d["setup"].is<const char*>() || strlen(d["setup"])>240 || d["offsets"].size()!=3) { reply(400,"Configurazione non valida"); return; }
    float candidate[3];
    for(int i=0;i<3;i++) {
      if(!d["offsets"][i].is<float>()) { reply(400,"Offset numerico richiesto"); return; }
      candidate[i]=d["offsets"][i].as<float>();
      if(!isfinite(candidate[i]) || fabs(candidate[i])>180) { reply(400,"Offset fuori intervallo"); return; }
    }
    sport=s; setupNote=d["setup"].as<String>(); memcpy(offsets,candidate,sizeof offsets);
    bool saved=prefsOK && prefs.putBytes("offset",offsets,sizeof offsets)==sizeof offsets;
    if(prefsOK) { saved=(prefs.putString("sport",sport)>0)&&saved; prefs.putString("setup",setupNote); }
    reply(saved?200:500,saved?"Configurazione salvata":"Applicata in RAM; salvataggio NVS fallito");
  });
  server.on("/api/time",HTTP_POST,[]{
    if(!idleOnly()) return;
    StaticJsonDocument<1024> d; if(!body(d)) return;
    int64_t t=d["utc_ms"] | int64_t(0);
    if(t<1704067200000LL || t>4102444800000LL) { reply(400,"Data non valida"); return; }
    clockEpoch=t; clockAt=nowUs(); clockSource="browser"; reply(200,"Ora del telefono impostata");
  });
  server.on("/api/sd/remount",HTTP_POST,[]{ if(!idleOnly()) return; bool ok=mountSD(); problem=ok?"":"sd_mount_error"; reply(ok?200:503,ok?"SD pronta":"SD non disponibile"); });
  server.on("/api/files",HTTP_GET,files);
  server.on("/api/download",HTTP_GET,[]{
    if(!idleOnly()) return;
    String id=server.arg("id"), kind=server.arg("kind");
    if(!validId(id) || (kind!="csv" && kind!="start" && kind!="end")) { reply(400,"File non valido"); return; }
    if(!sdOK) { reply(503,"SD non disponibile"); return; }
    const char *suffix=kind=="csv"?".csv":kind=="start"?".json":".end.json";
    File f=SD.open(pathFor(id,suffix)); if(!f) { reply(404,"File assente"); return; }
    server.sendHeader("Content-Disposition","attachment; filename=\""+id+suffix+"\"");
    server.streamFile(f,kind=="csv"?"text/csv":"application/json"); f.close();
  });
  server.on("/api/delete",HTTP_POST,[]{
    if(!idleOnly()) return;
    StaticJsonDocument<1024> d; if(!body(d)) return; String id=d["id"] | "";
    if(!validId(id)) { reply(400,"ID non valido"); return; }
    if(!sdOK) { reply(503,"SD non disponibile"); return; }
    // Delete CSV first: if it fails keep its metadata intact.
    String p=pathFor(id,".csv");
    if(SD.exists(p) && !SD.remove(p)) { reply(500,"Cancellazione CSV fallita"); return; }
    bool ok=true;
    for(const char *ext:{".json",".end.json"}) { p=pathFor(id,ext); if(SD.exists(p) && !SD.remove(p)) ok=false; }
    refreshSpace(); reply(ok?200:500,ok?"Sessione cancellata":"CSV cancellato; metadati non rimossi");
  });
  server.onNotFound([]{ reply(404,"Risorsa non trovata"); }); server.begin();
}
void logPins() {
  logEvent("PINS","I2C SDA=%d SCL=%d configured=%lu Hz; MPU address=0x%02X",SDA_PIN,SCL_PIN,(unsigned long)I2C_HZ,IMU_ADDR);
  logEvent("PINS","SD SCK=%d MISO=%d MOSI=%d CS=%d configured=%lu Hz",SD_SCK,SD_MISO,SD_MOSI,SD_CS,(unsigned long)SD_SPI_HZ);
  logEvent("PINS","LED=%d active HIGH; REED=%d INPUT_PULLUP active LOW; ADC battery=%d",LED_PIN,SWITCH_PIN,BATTERY_PIN);
  logEvent("PINS","GPS RX=%d TX=%d baud=%lu; optional, never waited for",GPS_RX,GPS_TX,(unsigned long)GPS_BAUD);
}
void logStatus() {
  logEvent("STATE","IMU init=%d data=%d; BME address=0x%02X data=%d; SD=%d; recording=%d stopping=%d error=%s",
    imuInitialized,imuOK,bmeAddress,bmeOK,sdOK,recording,stopping,problem.length()?problem.c_str():"none");
  logEvent("STATE","GPIO%d raw=%s stable=%s armed=%d; battery=%.3f V; GPS chars=%lu; heap=%lu",
    SWITCH_PIN,digitalRead(SWITCH_PIN)==LOW?"LOW/closed":"HIGH/open",magnet.stable?"closed":"open",magnet.armed,
    battery,(unsigned long)gps.charsProcessed(),(unsigned long)ESP.getFreeHeap());
  if(imuOK) logEvent("IMU","acc[g]=%.3f,%.3f,%.3f gyro_raw[dps]=%.2f,%.2f,%.2f",acc[0],acc[1],acc[2],gyro[0],gyro[1],gyro[2]);
  if(bmeOK) logEvent("BME","T=%.2f C RH=%.2f %% P=%.2f hPa",bmeTemp,humidity,pressure);
  if(recording) logEvent("REC","id=%s rows=%llu missed=%llu trigger_pending=%u",sessionId.c_str(),(unsigned long long)rows,(unsigned long long)missed,triggers.count);
}
void serialHelp() {
  logEvent("HELP","115200 baud. Send s=state, d=pins+I2C scan+chip IDs (idle only), m=remount SD (idle only), h=help. No line-ending required.");
}
void serialCommands() {
  unsigned budget=8;
  while(Serial.available() && budget--) {
    char c=Serial.read(); if(c=='\r'||c=='\n'||c==' ') continue;
    if(c=='s') { logStatus(); continue; }
    if(c=='h'||c=='?') { serialHelp(); continue; }
    if(c!='d'&&c!='m') { logEvent("SERIAL","Unknown command '%c'; send h for help",c); continue; }
    if(recording||calibrating) { logEvent("SERIAL","'%c' rejected: stop recording/calibration first",c); continue; }
    if(c=='m') {
      logEvent("SD","Remount requested from serial");
      problem=mountSD()?"":"sd_mount_error"; logStatus(); continue;
    }
    logPins(); scanI2c();
    for(uint8_t addr:{uint8_t(0x68),uint8_t(0x69),uint8_t(0x76),uint8_t(0x77)}) {
      uint8_t value=0,reg=addr<0x70?0x75:0xd0;
      if(readRegisters(addr,reg,&value,1)) logEvent("PROBE","addr=0x%02X reg=0x%02X ID=0x%02X",addr,reg,value);
      else logI2cFailure("PROBE");
    }
    logStatus();
    logEvent("DIAG","Probe does not reinitialize sensors. After wiring/address changes, restart to apply initialization.");
  }
}
void magnetTick() {
  bool previousStable=magnet.stable,previousArmed=magnet.armed;
  bool action=magnet.update(digitalRead(SWITCH_PIN)==LOW,nowUs()/1000,SWITCH_DEBOUNCE_MS);
  if(magnet.stable!=previousStable) {
    logEvent("MAG","GPIO%d debounced=%s (>= %lu ms)",SWITCH_PIN,
      magnet.stable?"LOW / CONTACT CLOSED":"HIGH / CONTACT OPEN",(unsigned long)SWITCH_DEBOUNCE_MS);
    if(magnet.stable&&!action) logEvent("MAG","Closure ignored: not armed (held since boot). Open contact for >= %lu ms first.",(unsigned long)SWITCH_DEBOUNCE_MS);
  }
  if(magnet.armed&&!previousArmed) logEvent("MAG","Armed: next stable closure will request START/STOP");
  if(action) {
    logEvent("MAG","EVENT accepted -> %s",recording?"STOP":"START");
    if(recording) requestStop("magnet"); else startRecording("magnet");
  }
}
void setup() {
  Serial.begin(115200); delay(300);
  logEvent("BOOT","SPORT LOGGER %s; compiled %s %s; reset_reason=%d",FIRMWARE_VERSION,__DATE__,__TIME__,(int)esp_reset_reason());
  pinMode(LED_PIN,OUTPUT); pinMode(SWITCH_PIN,INPUT_PULLUP);
  logPins();
  logEvent("MAG","BOOT GPIO%d=%s. Contact to GND; held contact at boot does not start recording.",SWITCH_PIN,digitalRead(SWITCH_PIN)==LOW?"LOW/closed":"HIGH/open");
  analogReadResolution(12); analogSetPinAttenuation(BATTERY_PIN,ADC_11db);
  logEvent("BATT","enabled=%d cells=%d divider=%.0f/%.0f ohm factor=%.4f",BATTERY_ENABLED,BATTERY_CELLS,R_TOP,R_BOTTOM,BATTERY_FACTOR);
  bool wireOK=Wire.begin(SDA_PIN,SCL_PIN); bool clockOK=Wire.setClock(I2C_HZ); Wire.setTimeOut(20);
  logEvent("I2C","Wire.begin=%s setClock=%s timeout=20 ms",wireOK?"OK":"FAIL",clockOK?"OK":"FAIL");
  scanI2c();
  imuInitialized=initIMU(); imuOK=imuInitialized; delay(100);
  logEvent("IMU","Startup result=%s",imuInitialized?"OK":"FAILED");
  initBME();
  prefsOK=prefs.begin("sportlogger",false);
  if(prefsOK) {
    calibrated=prefs.getBytesLength("bias")==sizeof gyroBias && prefs.getBytes("bias",gyroBias,sizeof gyroBias)==sizeof gyroBias;
    if(prefs.getBytesLength("offset")==sizeof offsets) prefs.getBytes("offset",offsets,sizeof offsets);
    sport=prefs.getString("sport","Foilscoot"); setupNote=prefs.getString("setup","");
  }
  calStatus=calibrated?"Calibrazione salvata caricata":"Non calibrata";
  logEvent("NVS","Preferences=%s calibration_loaded=%d",prefsOK?"OK":"FAIL",calibrated);
  char b[20]; snprintf(b,sizeof b,"%08lx",(unsigned long)esp_random()); bootId=b;
  SPI.begin(SD_SCK,SD_MISO,SD_MOSI,SD_CS); if(!mountSD()) problem="sd_mount_error";
  gnss.setRxBufferSize(4096); gnss.begin(GPS_BAUD,SERIAL_8N1,GPS_RX,GPS_TX);
  logEvent("GPS","UART started; no GPS connected is allowed");
  WiFi.mode(WIFI_AP); WiFi.setSleep(false);
  bool apOK=WiFi.softAP(SSID,PASSWORD); if(!apOK) problem="wifi_ap_failed";
  logEvent("WIFI","AP=%s SSID=%s IP=%s",apOK?"OK":"FAIL",SSID,WiFi.softAPIP().toString().c_str());
  routes(); nextSample=nowUs();
  logEvent("BOOT","Setup complete; http://%s ; missing peripherals do not block web UI",WiFi.softAPIP().toString().c_str());
  serialHelp(); logStatus();
}
void loop() {
  gpsTick(); uint64_t t=nowUs();
  if(t>=nextSample) {
    uint64_t late=(t-nextSample)/SAMPLE_US;
    if(recording) missed+=late;
    nextSample+=(late+1)*SAMPLE_US;
    sampleIMU(t); writeSample(t);
  }
  slowSensors(nowUs());
  magnetTick(); serialCommands();
  t=nowUs();
  if(recording && t-lastFlush>=uint64_t(FLUSH_MS)*1000) {
    lastFlush=t;
    if(!syncFile(csv)) finish("sd_flush_error");
    else { refreshSpace(); if(!sdOK) finish("sd_space_error"); else if(freeBytes<=SD_RESERVE) finish("sd_full"); }
  }
  // Trigger is a visible dark pulse while recording (normally solid on).
  bool led=recording ? t>=ledUntil : calibrating ? (t/150000)%2 : !sdOK ? (t/250000)%2 : (t%2000000<60000);
  digitalWrite(LED_PIN,led); server.handleClient(); delay(1);
  static uint64_t lastSummary=0;
  if(SERIAL_LOG_ENABLED && SERIAL_STATUS_MS && !recording && !calibrating && nowUs()-lastSummary>=uint64_t(SERIAL_STATUS_MS)*1000) {
    lastSummary=nowUs(); logStatus();
  }
}
