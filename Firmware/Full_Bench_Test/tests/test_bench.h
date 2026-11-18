#pragma once
#include "../BenchLogic.h"
#include "../BenchSettings.h"
#include <assert.h>
inline void testBench() {
  uint32_t seconds=0;
  assert(benchDuration("10s",nullptr,seconds) && seconds==10);
  assert(benchDuration("1","min",seconds) && seconds==60);
  assert(benchDuration("24h",nullptr,seconds) && seconds==86400);
  assert(!benchDuration("25h",nullptr,seconds)); assert(!benchDuration("0s",nullptr,seconds));
  assert(!benchDuration("1.5m",nullptr,seconds)); assert(!benchDuration("10s","min",seconds));
  assert(!benchDuration("4294967295h",nullptr,seconds)); assert(!benchDuration("10junk",nullptr,seconds));
  uint32_t epoch=0; assert(benchIsoEpoch("2000-01-01T00:00:00Z",epoch) && epoch==946684800);
  assert(benchIsoEpoch("2024-02-29T00:00:00Z",epoch) && epoch==1709164800);
  assert(!benchIsoEpoch("2025-02-29T00:00:00Z",epoch)); assert(!benchIsoEpoch("2026-11-04T24:00:00Z",epoch));
  assert(!benchIsoEpoch("2026-11-04T00:00:00",epoch)); assert(!benchIsoEpoch("2026-11-04T00:00:00+00:00",epoch));
  assert(benchGpioMode("\r\n+UGPIOC:\r\n16,2\r\n23,10\r\nOK\r\n",16)==2);
  assert(benchGpioMode("+UGPIOC: 23,10\r\nOK\r\n",23)==10);
  assert(benchGpioMode("+UGPIOC:\r\n16,255\r\n23,10\r\n",16)==255);
  assert(benchGpioMode("+UGPIOC:\r\n16,2junk\r\n",16)==-1);
  long rtt=0;
  assert(benchPingReply("+UUPING: 1,32,\"\",\"1.1.1.1\",55,123\r\n",rtt) && rtt==123);
  assert(benchPingReply("+UUPING: 1,32,\"host\",\"1.1.1.1\",55,-1",rtt) && rtt==-1);
  assert(!benchPingReply("OK",rtt)); assert(!benchPingReply("+UUPINGER: 12",rtt));
  assert(!benchPingReply("+UUPING: 1,32,\"\",\"1.1.1.1\",55,abc",rtt));
  assert(!benchPingReply("+UUPING: 2,32,\"\",\"1.1.1.1\",55,23",rtt));
  uint8_t r=0,g=0,b=0;
  assert(benchRgb("r",r,g,b) && r==255 && !g && !b);
  assert(benchRgb("255,255,0",r,g,b) && r==255 && g==255 && !b);
  assert(benchRgb("0,0,0",r,g,b) && !r && !g && !b);
  assert(!benchRgb("256,0,0",r,g,b)); assert(!benchRgb("-1,0,0",r,g,b));
  assert(!benchRgb("255,255",r,g,b)); assert(!benchRgb("1,2,3,4",r,g,b));
  assert(!benchRgb("1,,3",r,g,b)); assert(!benchRgb("wat",r,g,b));
  BenchStats stats; for (unsigned n:{0,1,2,3,4}) stats.add(uint16_t(n));
  assert(stats.count==5 && stats.low==0 && stats.high==4 && stats.mean==2 && fabs(stats.deviation()-sqrt(2.5))<1e-9);
  StaticJsonDocument<1024> json;
  for (const char *text:{"{\"seconds\":10}","{\"SleepInterval\":{\"minutes\":1}}","{\"sleep_interval\":{\"hours\":24}}"}) {
    assert(!deserializeJson(json,text)); assert(benchSleepSeconds(json.as<JsonObjectConst>(),seconds));
  }
  for (const char *text:{"{}","{\"seconds\":0}","{\"seconds\":2}","{\"seconds\":-1}","{\"seconds\":\"10\"}","{\"days\":4294967295}","{\"hours\":24,\"seconds\":1}","{\"SleepInterval\":[]}"}) {
    assert(!deserializeJson(json,text)); assert(!benchSleepSeconds(json.as<JsonObjectConst>(),seconds));
  }
  assert(benchRootFile("hypnos_config.json") && !benchRootFile("../mqtt_creds.json"));
  assert(benchSampleName("JLT00000.JSON") && !benchSampleName("mqtt_creds.json") && !benchSampleName("JLT0000x.JSON"));
  puts("PASS duration/UTC/GPIO/ping/RGB/settings parsers, ADC statistics and sample-file boundaries");
}
