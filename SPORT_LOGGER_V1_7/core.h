#pragma once
#include <stdint.h>
#include <math.h>
namespace Core {
inline float wrap(float x) { while(x>180) x-=360; while(x<=-180) x+=360; return x; }
// Gregorian date to Unix milliseconds, independent of ESP32 time_t width/time zone.
inline int64_t epochMs(int y, unsigned m, unsigned d, unsigned h, unsigned min, unsigned s, unsigned cs=0) {
  y -= m<=2; int era=(y>=0?y:y-399)/400; unsigned yo=y-era*400;
  unsigned doy=(153*(m>2?m-3:m+9)+2)/5+d-1;
  int64_t days=era*146097+(yo*365+yo/4-yo/100+doy)-719468;
  return ((days*24+h)*60+min)*60000+s*1000+cs*10;
}
struct Debounce {
  bool raw=false, stable=false, armed=false; uint64_t changed=0;
  inline bool update(bool closed, uint64_t ms, uint32_t delayMs) {
    if(closed!=raw) { raw=closed; changed=ms; }
    if(ms-changed<delayMs) return false;
    if(!raw) armed=true; // A held magnet at boot must never start recording.
    if(raw==stable) return false;
    stable=raw;
    if(stable && armed) { armed=false; return true; }
    return false;
  }
};
struct TriggerQueue {
  uint64_t times[16]={}; unsigned head=0, count=0;
  inline bool push(uint64_t t) { if(count==16) return false; times[(head+count++)%16]=t; return true; }
  inline uint64_t front() const { return count?times[head]:0; }
  inline void pop() { if(count) { head=(head+1)%16; --count; } }
  inline void clear() { head=count=0; }
};
}

