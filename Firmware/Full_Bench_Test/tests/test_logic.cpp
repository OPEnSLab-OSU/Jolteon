#include "../Logic.h"
#include "../PressureChecks.h"
#include "../SleepDeadline.h"
#include <assert.h>
#include <stdio.h>
#include "test_registration.h"
#include "test_scan.h"
#include "test_bench.h"
int main() {
  testRegistration();
  testScan();
  testBench();
  const auto retained = SleepDeadline::make(1793836798u, 1793836808u, SleepDeadline::railMask);
  assert(retained.valid(86400u));
  assert(!retained.alarmDue(retained.wakeAt - 1, 1));
  assert(retained.alarmDue(retained.wakeAt, 1));
  assert(retained.alarmDue(retained.wakeAt + 1, 3));
  assert(!retained.alarmDue(retained.wakeAt, 0));
  assert(!retained.alarmDue(retained.wakeAt, 0x81));
  assert(!SleepDeadline{}.valid(86400u));
  assert(!SleepDeadline::make(retained.armedAt, retained.armedAt, 3).valid(86400u));
  assert(!SleepDeadline::make(retained.armedAt, retained.armedAt + 86401u, 3).valid(86400u));
  assert(!SleepDeadline::make(retained.armedAt, retained.wakeAt, 8).valid(86400u));
  auto torn = retained;
  torn.wakeAtInverse ^= 1u;
  assert(!torn.valid(86400u));
  torn = retained;
  torn.rails ^= SleepDeadline::rail12;
  assert(!torn.valid(86400u));
  // Fixed calibration vector; CRC4=0. Reserved low-byte bits must
  // not be mistaken for the four CRC bits (legacy driver status is unreliable).
  uint16_t prom[]={0,40127,36924,23317,23282,33464,28312,0xa0};
  assert(pressurePromCrc(prom)==0 && pressurePromValid(prom));
  for (uint8_t word=0;word<7;++word) for (uint8_t bit=0;bit<16;++bit) {
    prom[word]^=uint16_t(1u<<bit); assert(!pressurePromValid(prom)); prom[word]^=uint16_t(1u<<bit);
  }
  prom[7]^=1; assert(!pressurePromValid(prom));
  uint16_t empty[8]={}; assert(!pressurePromValid(empty));
  for (auto &word:empty) word=0xffff;
  assert(!pressurePromValid(empty));
  assert(pressureAdcValid(1) && pressureAdcValid(0xfffffe));
  assert(!pressureAdcValid(0) && !pressureAdcValid(0xffffff) && !pressureAdcValid(0x1000000));
  uint32_t n=0;
  assert(unsignedNumber("4294967295",n,UINT32_MAX) && n==UINT32_MAX);
  assert(!unsignedNumber("4294967296",n,UINT32_MAX)); assert(!unsignedNumber("-1",n,10));
  assert(!unsignedNumber("12junk",n,20)); assert(!unsignedNumber(nullptr,n,20)); assert(!unsignedNumber("1",n,0));
  assert(lineEquals("AT\r\n\r\nOK\r\n","OK")); assert(!lineEquals("NOT OK\r\n","OK"));
  assert(!lineEquals("OKAY\n","OK")); assert(lineEquals("\nERROR","ERROR"));
  assert(registration("\r\n+CEREG: 2,1\r\n")); assert(registration("+CEREG: 0,5,\"1234\"\r\n"));
  assert(!registration("+CEREG: 2,10\r\n")); assert(!registration("+CEREG: 0,0\n"));
  uint8_t bytes[7];
  // Exhaust every date and representative midnight/second boundaries through 2099.
  for (uint32_t day=946684800ul;day<4102444800ul;day+=86400ul) {
    const uint32_t offsets[]={0,59,60,3599,43200,86399};
    for (uint32_t off:offsets) { uint32_t epoch=day+off, decoded=0;
      assert(epochRtc(epoch,bytes)); assert(rtcEpoch(bytes,decoded)); assert(epoch==decoded); }
  }
  assert(!epochRtc(946684799ul,bytes)); assert(!epochRtc(4102444800ul,bytes));
  assert(epochRtc(1709164800ul,bytes)); assert(bytes[4]==0x29 && bytes[5]==2 && bytes[6]==0x24);
  bytes[6]=0x23; assert(!rtcEpoch(bytes,n)); // February 29, non-leap year.
  epochRtc(946684800ul,bytes); bytes[0]=0x6a; assert(!rtcEpoch(bytes,n));
  epochRtc(946684800ul,bytes); bytes[5]=0x81; assert(!rtcEpoch(bytes,n)); // Unsupported century.
  epochRtc(946684800ul,bytes); bytes[2]=0x52; assert(!rtcEpoch(bytes,n)); // Explicitly reject 12h.
  uint16_t seconds=0; uint8_t values=0;
  assert(sdiMeasurement("00023\r\n",seconds,values) && seconds==2 && values==3);
  assert(!sdiMeasurement("00020\r\n",seconds,values)); assert(!sdiMeasurement("0abc3\r\n",seconds,values));
  assert(sdiValueCount("0+1.234-0.001+23\r\n")==3);
  assert(sdiValueCount("0+12345678\r\n")==-1); assert(sdiValueCount("0+1.2.3\r\n")==-1);
  assert(sdiValueCount("0+\r\n")==-1); assert(sdiValueCount("0+1e3\r\n")==-1);
  assert(sdiValueCount("0+1\n")==-1); assert(sdiValueCount("0+1\r\nEXTRA")==-1);
  puts("PASS pressure PROM/ADC guards, RTC/AT/SDI parsers and all 2000-2099 calendar dates");
}
