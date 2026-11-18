#include "Telemetry.h"
#include "Protocol.h"
#include "Indicators.h"
#include <assert.h>
#include <deque>
#include <string>
#include <vector>
#include <iostream>

static uint32_t now;
uint32_t millis() { return now; }
FakeSerial Serial;
Sercom *SERCOM4=nullptr,*SERCOM5=nullptr;
RawUart nrfUart(nullptr,3,NRF_TX,NRF_RX),lteUart(nullptr,1,LTE_TX,LTE_RX);
static std::deque<unsigned char> fakeRx;
static std::vector<std::string> sent;
static bool pinState[2][32];
bool RawUart::write(const char *text) { if (!active) return false; sent.emplace_back(text); return true; }
int RawUart::read() { if (fakeRx.empty()) return -1; int c=fakeRx.front(); fakeRx.pop_front(); return c; }
void pinWrite(Pin pin,bool high) { pinState[pin.port][pin.bit]=high; }
void pinOutput(Pin pin,bool high) { pinWrite(pin,high); }
static void feed(const std::string &line) { for (unsigned char c:line) telemetryFeed(c); }
static std::string ack(const std::string &command,const std::string &overrideTag="") {
  size_t begin=command.find('"')+1,end=command.rfind('"');
  std::string payload=command.substr(begin,end-begin),tag=payload.substr(0,payload.find(' '));
  if (!overrideTag.empty()) tag=overrideTag;
  return "ok tx both len="+std::to_string(payload.size())+" accepted=1 uart=source ble=queued wifi=unavailable ble_ret=0 wifi_ret=0 tag="+tag+"\r\n";
}
static void protocolTests() {
  uint32_t crc=0xfffffffful;
  for (const char *p="123456789"; *p; ++p) crc=crc32Byte(crc,uint8_t(*p));
  assert((crc^0xfffffffful)==0xcbf43926ul);
  assert(responseLine("AT\r\nOK\r\n","OK"));
  assert(!responseLine("AT OK\r\nMODEM_OK\r\n","OK"));
  assert(!responsePrefix("wt tx ble BLE TX: 5 bytes\r\n","BLE TX: "));
  assert(responsePrefix("BLE TX: 5 bytes\r\n","BLE TX: "));
  const char *json="{\"type\":\"wifi_scan\",\"status\":0,\"running\":false,\"aps\":[{\"ssid\":\"a\\\"b\",\"rssi\":-62}]}";
  assert(jsonFieldEquals(json,"type","\"wifi_scan\""));
  assert(jsonFieldEquals(json,"running","false"));
  assert(!jsonFieldEquals(json,"running","true"));
  for (size_t i=0;i<strlen(json);++i) assert(!jsonFieldEquals(std::string(json,i).c_str(),"status","0"));
  assert(!jsonFieldEquals("echo {\"status\":0}","status","0"));
  assert(!jsonFieldEquals("{\"status\":0}garbage","status","0"));
  assert(!jsonFieldEquals("{\"status\":0,\"status\":1}","status","0"));
  assert(!jsonFieldEquals("{\"status\":00}","status","0"));
  assert(!jsonFieldEquals("{\"status\":0,}","status","0"));
  assert(!jsonFieldEquals("{\"status\":0,\"bad\":\"\\u\"}","status","0"));
  uint32_t number;
  assert(parseUnsigned("0x2000",number,8192) && number==8192);
  assert(!parseUnsigned("-1",number,UINT32_MAX));
  assert(!parseUnsigned("4294967296",number,UINT32_MAX));
  assert(!parseUnsigned("999999999999999999999999",number,UINT32_MAX));
  assert(!parseUnsigned("8garbage",number,100));
  assert(networkRegistered("AT+CEREG?\r\n+CEREG: 2,5,\"1234\"\r\nOK\r\n"));
  assert(!networkRegistered("+CEREG: 2,2\r\nOK\r\n"));
  assert(!networkRegistered("+CEREG: 2,5garbage\r\nOK\r\n"));
  assert(!networkRegistered("echo +CEREG: 2,5\r\nOK\r\n"));
  assert(!networkRegistered("+CEREG: 2,5\r\nERROR\r\n"));
  assert(bridgeAck("ok tx both len=8 accepted=1 uart=source ble=queued wifi=unavailable tag=J18#7",8,"J18#7"));
  assert(!bridgeAck("ok tx both len=8 accepted=0 uart=source tag=J18#7",8,"J18#7"));
  assert(!bridgeAck("ok tx both len=8 accepted=1 uart=source tag=J18#6",8,"J18#7"));
  assert(!bridgeAck("ok tx both len=8 accepted=1 uart=queued tag=J18#7",8,"J18#7"));
  uint8_t clock[]={0x59,0x59,0x23,7,0x29,0x02,0x00};
  assert(rtcCalendarValid(clock)); // Feb 29, 2000.
  clock[5]=0x82; assert(!rtcCalendarValid(clock)); // 2100 is not leap.
  clock[5]=0x02; clock[0]=0x1a; assert(!rtcCalendarValid(clock));
  clock[0]=0; clock[2]=0x72; assert(rtcCalendarValid(clock)); // 12 PM in 12-hour mode.
  clock[2]=0x40; assert(!rtcCalendarValid(clock));
}
static void telemetryTests() {
  nrfUart.active=true; report.write("alpha"); telemetryService(); assert(sent.size()==1);
  std::string first=sent.back(); report.write("bravo");
  now+=TELEMETRY_ACK_MS+1; telemetryService(); now+=TELEMETRY_RETRY_MS+1; telemetryService();
  assert(sent.size()==2 && sent.back()==first); // Appending bytes must not change a retry.
  feed(ack(first,"J18#999")); telemetryService(); assert(sent.size()==2);
  feed("\x1b[32m"+ack(first)+"\x1b[0m"); now+=81; telemetryService();
  assert(sent.size()==3 && sent.back()!=first && sent.back().find("bravo")!=std::string::npos);
  std::string second=sent.back();
  feed(ack(first)); telemetryService(); assert(sent.size()==3); // Same-length stale ACK cannot pop next chunk.
  feed(ack(second)); now+=81; telemetryService(); assert(sent.size()==3);
  telemetryPause(true);
  feed("BLE UART TX: j18 summary\r\n"); feed("WIFI UART TX: j18 all\r\n");
  char command[384]; RemoteSource source;
  assert(telemetryCommand(command,sizeof(command),source) && !strcmp(command,"summary") && source==REMOTE_BLE);
  assert(!telemetryCommand(command,sizeof(command),source));
  feed("echo BLE UART TX: j18 all\r\n"); assert(!telemetryCommand(command,sizeof(command),source));
  feed(std::string(400,'X')+"\r\nWIFI UART TX: j18 battery\r\n");
  assert(telemetryCommand(command,sizeof(command),source) && source==REMOTE_WIFI && !strcmp(command,"battery"));
  telemetryPause(false);
  assert(!telemetryTarget("256.1.2.3","5000"));
  assert(!telemetryTarget("1.2.3","5000"));
  assert(!telemetryTarget("01.2.3.4","5000"));
  assert(!telemetryTarget("0.0.0.0","5000"));
  assert(!telemetryTarget("1.2.3.4","0"));
  assert(telemetryTarget("192.168.1.20","5000"));
  nrfUart.active=false; report.write("offline backlog"); Serial.connected=true;
  telemetryService(); assert(Serial.output.find("offline backlog")!=std::string::npos);
  Serial.connected=false; telemetryMode("off"); telemetryMode("auto");
  for (unsigned i=0;i<7000;++i) report.write('N');
  Serial.connected=true; Serial.output.clear(); telemetryStatus();
  assert(Serial.output.find("dropped=0 ")==std::string::npos && Serial.output.find("remote_drop=1")!=std::string::npos);
  while (Serial.output.size()<6143) telemetryService(); // Bounded ring can replay after wrapping.
  telemetryMode("off"); Serial.connected=false; telemetryMode("auto");
  now=UINT32_MAX-1000; assert(telemetryTarget("0.0.0.0","0")); nrfUart.active=true;
  telemetryService(); std::string wrap=sent.back();
  now+=TELEMETRY_ACK_MS+1; telemetryService(); now+=TELEMETRY_RETRY_MS+1; telemetryService();
  assert(sent.back()==wrap); feed(ack(wrap)); telemetryService();
}
static void indicatorTests() {
  Serial.connected=false; nrfUart.active=false; now=200000; indicatorsBegin(); indicatorsClear();
  indicateTx(); heartbeatService(); assert(pinState[HEARTBEAT.port][HEARTBEAT.bit]);
  if (ENABLE_EXTRA_LEDS) assert(pinState[TX_LED.port][TX_LED.bit]);
  now+=80; heartbeatService(); assert(!pinState[HEARTBEAT.port][HEARTBEAT.bit]);
  now+=70; heartbeatService(); assert(pinState[HEARTBEAT.port][HEARTBEAT.bit]);
  indicateRx(); heartbeatService(); if (ENABLE_EXTRA_LEDS) assert(pinState[RX_LED.port][RX_LED.bit]);
  now=202000; indicateResult(false); heartbeatService(); assert(pinState[HEARTBEAT.port][HEARTBEAT.bit]);
  now+=150; heartbeatService(); assert(!pinState[HEARTBEAT.port][HEARTBEAT.bit]);
  now+=50; heartbeatService(); assert(pinState[HEARTBEAT.port][HEARTBEAT.bit]);
  heartbeatEnable(false); pinState[HEARTBEAT.port][HEARTBEAT.bit]=false;
  indicateTx(); heartbeatService(); assert(!pinState[HEARTBEAT.port][HEARTBEAT.bit]);
}
int main() {
  protocolTests(); telemetryTests(); indicatorTests();
  std::cout<<"PASS: strict protocol/calendar parsing; frozen retries; stale ACK rejection; remote-source capture; overflow/replay; clock wrap; client-free LED patterns\n";
}
