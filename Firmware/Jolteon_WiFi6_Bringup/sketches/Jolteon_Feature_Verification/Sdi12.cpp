#include "BoardIO.h"
#include "Sdi12.h"
#include "Telemetry.h"
#include "Indicators.h"
#include <string.h>
static const uint32_t BIT_US=833;
static bool waitBit(uint32_t deadline) {
  while (int32_t(micros()-deadline)<0) {}
  return uint32_t(micros()-deadline)<200; // Reject gross ISR-induced timing slips.
}
static void receiveMode() { pinInput(SDI_PIN); pinWrite(SDI_DIR,false); }
bool sdi12Power(bool on) {
  if (!J18_REVISED_SDI12 || !SDI_HAS_POWER_ENABLE) return false;
  pinOutput(SDI_POWER_EN,on); if (on) waitService(SDI_POWER_SETTLE_MS);
  return pinRead(SDI_POWER_EN)==on;
}
bool sdi12Exchange(const char *command,char *response,size_t capacity,const char *&error) {
  error="Revised DIR-controlled circuit not selected in config.h.";
  if (!J18_REVISED_SDI12 || !command || !response || capacity<4) return false;
  response[0]=0; size_t n=strlen(command);
  if (!n || n>32 || command[n-1]!='!') { error="Expected at most 32 ASCII characters ending in !."; return false; }
  for (size_t i=0;i<n;++i) if (command[i]<32 || command[i]>126) { error="Invalid SDI-12 character."; return false; }
  receiveMode();
  if (pinRead(SDI_PIN)) { error="SDI idle must be LOW; check field-bus polarity/loading."; return false; }
  telemetryPause(true);
  struct Release { ~Release() { receiveMode(); telemetryPause(false); } } release;
  pinOutput(SDI_PIN,false); pinWrite(SDI_DIR,true); indicateTx();
  pinWrite(SDI_PIN,true); waitService(13); pinWrite(SDI_PIN,false); waitService(9);
  for (size_t i=0;i<n;++i) {
    uint8_t byte=command[i],parity=0; uint32_t tick=micros();
    pinWrite(SDI_PIN,true); // Inverted start bit.
    for (unsigned bit=0;bit<7;++bit) {
      tick+=BIT_US; if (!waitBit(tick)) { error="Transmit timing overrun."; return false; }
      bool one=(byte>>bit)&1; parity^=uint8_t(one); pinWrite(SDI_PIN,!one);
    }
    tick+=BIT_US; if (!waitBit(tick)) { error="Transmit parity timing overrun."; return false; }
    pinWrite(SDI_PIN,!parity);
    tick+=BIT_US; if (!waitBit(tick)) { error="Transmit stop timing overrun."; return false; }
    pinWrite(SDI_PIN,false); tick+=BIT_US;
    if (!waitBit(tick)) { error="Transmit frame timing overrun."; return false; }
    heartbeatService();
  }
  receiveMode(); uint32_t overall=millis(); size_t used=0;
  while (uint32_t(millis()-overall)<1500) {
    uint32_t waitStart=millis();
    while (!pinRead(SDI_PIN)) {
      if (uint32_t(millis()-waitStart)>(used ? 30u : 200u)) { error="Missing SDI response/start bit or CRLF terminator."; return false; }
      heartbeatService();
    }
    uint32_t tick=micros()+BIT_US+BIT_US/2; uint8_t byte=0,parity=0;
    for (unsigned bit=0;bit<7;++bit) {
      if (!waitBit(tick)) { error="Receive sampling overrun."; return false; }
      bool one=!pinRead(SDI_PIN); byte|=uint8_t(one)<<bit; parity^=uint8_t(one); tick+=BIT_US;
    }
    if (!waitBit(tick) || (!pinRead(SDI_PIN))!=bool(parity)) { error="SDI even-parity error."; return false; }
    tick+=BIT_US;
    if (!waitBit(tick) || pinRead(SDI_PIN)) { error="SDI stop-bit/framing error."; return false; }
    if (used+1>=capacity) { error="SDI response buffer overflow."; return false; }
    if ((byte<32 && byte!='\r' && byte!='\n') || byte>126) { error="Invalid SDI response character."; return false; }
    response[used++]=char(byte); response[used]=0; indicateRx();
    if (used>=2 && response[used-2]=='\r' && response[used-1]=='\n') {
      if (used<3 || (command[0]!='?' && response[0]!=command[0])) { error="Response address does not match the command."; return false; }
      error=nullptr; return true;
    }
    if (!waitBit(tick+BIT_US/2)) { error="Receive frame timing overrun."; return false; }
  }
  error="SDI transaction deadline exceeded."; return false;
}
