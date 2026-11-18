#include "BoardIO.h"
#include "Indicators.h"
static bool enabled=true, busy, fault;
static uint32_t txUntil, rxUntil, mainRxFrom, mainRxUntil, passUntil;
static bool until(uint32_t now,uint32_t deadline) { return int32_t(deadline-now)>0; }
static void led(Pin pin,bool on) { pinWrite(pin,on==LED_ACTIVE_HIGH); }
void indicatorsBegin() {
  pinOutput(HEARTBEAT,!LED_ACTIVE_HIGH);
  if (ENABLE_EXTRA_LEDS) { pinOutput(TX_LED,!LED_ACTIVE_HIGH); pinOutput(RX_LED,!LED_ACTIVE_HIGH); }
}
void indicateTx() { txUntil=millis()+300; }
void indicateRx() {
  uint32_t now=millis(); rxUntil=now+120;
  mainRxFrom=until(now,txUntil) ? txUntil : now; mainRxUntil=mainRxFrom+120;
}
void indicateBusy(bool active) { busy=active; }
void indicateResult(bool passed) { if (passed) passUntil=millis()+600; else fault=true; }
void indicatorsClear() { fault=false; passUntil=0; }
void heartbeatEnable(bool active) { enabled=active; if (enabled) pinOutput(HEARTBEAT,!LED_ACTIVE_HIGH); }
bool heartbeatEnabled() { return enabled; }
void heartbeatService() {
  uint32_t now=millis(); bool tx=until(now,txUntil), rx=until(now,rxUntil);
  if (ENABLE_EXTRA_LEDS) { led(TX_LED,tx); led(RX_LED,rx); }
  if (!enabled) return;
  bool on;
  if (tx) { uint32_t elapsed=300-(txUntil-now); on=elapsed<70 || (elapsed>=140 && elapsed<210); }
  else if (int32_t(now-mainRxFrom)>=0 && until(now,mainRxUntil)) on=true;
  else if (until(now,passUntil)) { uint32_t elapsed=600-(passUntil-now); on=elapsed<180 || (elapsed>=300 && elapsed<480); }
  else if (busy) on=(now/100)%2;
  else if (fault) { uint32_t phase=now%2000; on=phase<800 && (phase/100)%2==0; }
  else on=(now/500)%2;
  led(HEARTBEAT,on);
}
