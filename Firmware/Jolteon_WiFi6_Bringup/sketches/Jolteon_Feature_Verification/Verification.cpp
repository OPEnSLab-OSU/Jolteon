#include "Verification.h"
#include "BoardIO.h"
#include "Telemetry.h"
#include "Protocol.h"
#include "Indicators.h"
#include "Sdi12.h"
#include "src/SdFat/SdFat.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#ifndef J18_CORE_USB_LEDS_DISABLED
#error Use the included build launcher: J18BuildOverrides.h must also disable Zero USB LEDs in the core.
#endif

enum Outcome : uint8_t { NOT_RUN, PASS, FAIL, BLOCKED, INFO };
enum TestId : uint8_t { DEVICE_INFO, BOOT_IMAGE, MAP0, MAP1, NRF_POWER, NRF_LINK, NRF_IRQ_TEST, WIFI_SCAN, WIFI_NET, WIFI_PEER, BLE_READY, BLE_TX, BLE_PEER, LTE_AT, LTE_SIM, LTE_NET, LTE_DATA, I2C_ROOT, MUX_TEST, RTC_TEST, RTC_SQW, RADIO_SPI, RADIO_RF, SD_MOUNT, SD_WRITE, BATTERY, GPIO_TEST, SPI_LOOP0, SPI_LOOP1, SDI_TEST, POWER_CHECK, COUNT };
static const char *names[COUNT] = {"Device/boot diagnostics","Bootloader image CRC","SPI0 hardware map","SPI1 hardware map","WT02 power control","WT02 UART","WT02 IRQ","Wi-Fi scan","Wi-Fi association/IP","Wi-Fi roundtrip","BLE ready","BLE notify queued","BLE roundtrip","LTE AT identity","LTE SIM","LTE registration","LTE data session","I2C root bus","I2C mux","RTC clock","RTC IRQ/SQW","RFM69 registers","RFM69 RF/IRQ","SD mount","SD file roundtrip","Battery ADC","GPIO loopback","SPI0 loopback","SPI1 loopback","SDI-12","Power/charging measurements"};
static Outcome outcomes[COUNT];
static uint32_t testedAt[COUNT],runNumber;
static char reply[3072], commandLine[384];
static uint16_t commandLength;
static bool discardLongLine, nrfOn, usbWasOpen;
static bool baselineRan;
static RemoteSource commandSource=REMOTE_NONE;
static RawUart *bridge;
static const char *outcomeName(Outcome result) {
  static const char *text[] = {"NOT RUN","PASS","FAIL","BLOCKED","INFO"}; return text[result];
}
static void record(TestId id, Outcome result, const char *detail) {
  outcomes[id] = result;
  testedAt[id]=millis();
  if (result==FAIL || result==PASS) indicateResult(result==PASS);
  report.print('['); report.print(outcomeName(result)); report.print("] ");
  report.print(names[id]); report.print(": "); report.println(detail);
  telemetryService();
}
static void hexByte(uint8_t value) { if (value < 16) report.print('0'); report.print(value, HEX); }
static void printPin(Pin p) { report.print(p.port ? "PB" : "PA"); if (p.bit < 10) report.print('0'); report.print(p.bit); }
static bool number(const char *text, uint32_t &value, uint32_t maximum) {
  return parseUnsigned(text,value,maximum);
}
static void summary() {
  report.print(F("\nRun ")); report.print(runNumber);
  report.println(F(" results (INFO/BLOCKED/NOT RUN do not certify a feature):"));
  uint8_t totals[5]={0};
  for (uint8_t i = 0; i < COUNT; ++i) {
    ++totals[outcomes[i]];
    report.print(outcomeName(outcomes[i])); report.print(" | "); report.print(names[i]);
    report.print(F(" | t_ms=")); report.println(testedAt[i]);
  }
  report.print(F("UART errors/drop: nRF ")); report.print(nrfUart.errors); report.print('/'); report.print(nrfUart.dropped);
  report.print(F("; LTE ")); report.print(lteUart.errors); report.print('/'); report.println(lteUart.dropped);
  telemetryStatus();
  report.print(F("Counts PASS/FAIL/BLOCKED/INFO/NOT_RUN: "));
  report.print(totals[PASS]); report.print('/'); report.print(totals[FAIL]); report.print('/');
  report.print(totals[BLOCKED]); report.print('/'); report.print(totals[INFO]); report.print('/'); report.println(totals[NOT_RUN]);
}
static void deviceInfo() {
  uint32_t flashBytes=NVMCTRL->PARAM.bit.NVMP*(8ul<<NVMCTRL->PARAM.bit.PSZ);
  uint8_t cause=PM->RCAUSE.reg,protection=(*reinterpret_cast<volatile uint32_t *>(0x00804000))&7;
  static const uint16_t bootSizes[]={32768,16384,8192,4096,2048,1024,512,0};
  const uint32_t *vectors=reinterpret_cast<const uint32_t *>(0x2000);
  report.print(F("DSU DID=0x")); report.print(DSU->DID.reg,HEX);
  report.print(F(" flash=")); report.print(flashBytes); report.print(F(" clock=")); report.println(SystemCoreClock);
  report.print(F("Reset cause=0x")); report.print(cause,HEX);
  report.print(F(" POR/BOD12/BOD33/EXT/WDT/SYST="));
  const uint8_t masks[]={1,2,4,16,32,64};
  for (uint8_t mask:masks) { report.print(bool(cause&mask)); report.print(' '); } report.println();
  report.print(F("BOOTPROT bytes=")); report.println(bootSizes[protection]);
  if (bootSizes[protection]!=8192) report.println(F("BOOTPROT differs from the tooling's 8192-byte setting; review protection before future flashing."));
  report.print(F("App vectors @0x2000: SP=0x")); report.print(vectors[0],HEX); report.print(F(" reset=0x")); report.println(vectors[1],HEX);
  report.print(F("USB console=")); report.println(bool(Serial));
  report.print(F("nRF EN state=")); report.print(pinRead(NRF_EN)); report.print(F(" auto power verified=")); report.println(NRF_POWER_PATH_VERIFIED);
  bool valid=flashBytes==262144 && vectors[0]>0x20000000 && vectors[0]<=0x20008000 && (vectors[1]&1) && (vectors[1]&~1ul)>=0x2000 && (vectors[1]&~1ul)<flashBytes;
  record(DEVICE_INFO,valid ? INFO : FAIL,"Read-only device/reset/vector/fuse report; neither silicon package nor physical rail integrity is inferred. Expected BOOTPROT=8192.");
}
static void bootImageTest() {
  if (!EXPECTED_BOOTLOADER_BYTES || EXPECTED_BOOTLOADER_BYTES>8192) { record(BOOT_IMAGE,BLOCKED,"Configure a verified bootloader byte count/CRC in config.h; no flash is written by this check."); return; }
  // Address zero is readable flash on this MCU. Load the base through volatile
  // storage so the compiler does not treat a constant null pointer as a trap.
  volatile uintptr_t base=0;
  const volatile uint8_t *flash=reinterpret_cast<const volatile uint8_t *>(base);
  uint32_t crc=0xfffffffful;
  for (uint32_t i=0;i<EXPECTED_BOOTLOADER_BYTES;++i) {
    crc=crc32Byte(crc,flash[i]);
    if (!(i&127)) { heartbeatService(); telemetryService(); }
  }
  crc^=0xfffffffful;
  report.print(F("Bootloader bytes=")); report.print(EXPECTED_BOOTLOADER_BYTES); report.print(F(" CRC32=0x")); report.print(crc,HEX); report.print(F(" expected=0x")); report.println(EXPECTED_BOOTLOADER_CRC32,HEX);
  record(BOOT_IMAGE,crc==EXPECTED_BOOTLOADER_CRC32 ? PASS : FAIL,"Read-only check of configured image bytes; mismatch means a different/modified image. Confirm config before diagnosing corruption.");
}
static void mappingTests() {
  record(MAP0, spi0.mappingValid() ? PASS : FAIL, spi0.mappingValid() ? "Revised PB13 SCK/PB11 MISO/PB12 MOSI; radio CS PB10." : "EXPECTED on current PCB: PB10 is PAD2, cannot be hardware SCK. Swap SCK PB10 with radio CS PB13.");
  record(MAP1, spi1.mappingValid() ? PASS : FAIL, spi1.mappingValid() ? "Revised PB17 SCK/PB22 MISO/PB16 MOSI." : "EXPECTED on current PCB: PB16 is PAD0, cannot be hardware SCK. Use SCK PB17/MISO PB22/MOSI PB16.");
}
static bool spiBegin(BoardSpi &bus, TestId id) {
  if (!bus.begin()) { record(id, BLOCKED, "Unsupported hardware pin map. 'spi soft' enables a separately labeled software workaround."); return false; }
  report.println(bus.software ? F("Transport: SOFTWARE SPI (does not fix hardware-map FAIL).") : F("Transport: hardware SPI."));
  return true;
}
static bool terminal(const char *text, const char *word) {
  return responseLine(text,word);
}
static bool jsonValue(const char *key, const char *value) {
  return jsonFieldEquals(reply,key,value);
}
static bool transact(RawUart &uart, const char *line, uint32_t timeout, const char *signature, bool at) {
  reply[0] = 0;
  if (!uart.active) return false;
  telemetryPause(true);
  if (&uart==&nrfUart) {
    for (unsigned i=0;i<2048;++i) { int c=uart.read(); if (c<0) break; telemetryFeed(uint8_t(c)); }
  } else uart.discard();
  uint32_t errors = uart.errors, drops = uart.dropped;
  if (!uart.write(line) || !uart.write(at ? "\r" : "\r\n")) { telemetryPause(false); return false; }
  report.print(F("TX> ")); report.println(line);
  size_t used = 0; bool match = false, overflow = false;
  uint32_t start = millis(), last = start;
  bool escape=false;
  while (uint32_t(millis() - start) < timeout) {
    int c;
    for (unsigned batch=0;batch<256 && (c=uart.read())>=0;++batch) {
      last = millis();
      if (&uart==&nrfUart) telemetryFeed(uint8_t(c));
      if (c==27) { escape=true; continue; }
      if (escape) { if (c>='@' && c<='~' && c!='[') escape=false; continue; }
      if (used + 1 < sizeof(reply)) reply[used++] = char(c); else overflow = true;
    }
    reply[used] = 0;
    if (at) {
      if (terminal(reply,"ERROR") || strstr(reply,"+CME ERROR:") || strstr(reply,"+CMS ERROR:")) break;
      if (terminal(reply,"OK")) { match = true; break; }
    } else {
      match = signature && (signature[0]=='"' ? strstr(reply,signature)!=nullptr : responsePrefix(reply,signature));
      if (match && uint32_t(millis() - last) > 150) break;
      if (!signature && used && uint32_t(millis() - last) > 300) break;
    }
    heartbeatService(); delay(1);
  }
  report.print(F("RX> ")); report.println(used ? reply : "<timeout/no bytes>");
  if (overflow) report.println(F("Response exceeded capture buffer; not accepted as PASS."));
  telemetryPause(false);
  return match && !overflow && errors == uart.errors && drops == uart.dropped;
}
static bool nrfCommand(const char *text, const char *signature, uint32_t timeout = 3000) {
  char line[320];
  if (!nrfOn || !nrfUart.active) { report.println(F("nRF is off. Check R20/U10 and UART mismatch; then use 'nrf on'.")); return false; }
  if (snprintf(line,sizeof(line),"%s%s",NRF_PREFIX,text) >= int(sizeof(line))) return false;
  return transact(nrfUart,line,timeout,signature,false);
}
static void nrfPower(bool on) {
  telemetryPause(true);
  nrfUart.end(); pinInput(NRF_IRQ); pinInput(NRF_RST);
  if (!on) { pinWrite(NRF_EN,false); nrfOn = false; report.println(F("nRF disabled; UART/reset/IRQ high impedance.")); telemetryPause(false); record(NRF_POWER,pinRead(NRF_EN) ? FAIL : INFO,"EN low; off-state rail isolation still requires a meter."); return; }
  report.println(F("Enabling nRF: schematic/BOM R20=10 ohms shunt needs bench correction/confirmation."));
  pinOutput(NRF_RST,false); // Hold reset low while supply ramps; no HIGH drive into an off module.
  pinOutput(NRF_EN,true); waitService(NRF_POWER_SETTLE_MS);
  pinInput(NRF_RST); // Active-low reset, module pullup releases.
  nrfOn = nrfUart.begin(NRF_BAUD); waitService(NRF_BOOT_WAIT_MS);
  report.println(F("UART TX PA22 -> nRF P1.05; bridge firmware must receive on P1.05, not P1.06."));
  telemetryPause(false);
  record(NRF_POWER,nrfOn && pinRead(NRF_EN) ? INFO : FAIL,"EN readback + configured settling delay; actual VDD_NRF and CT ramp require measurement.");
}
static void nrfTest() {
  if (!nrfOn) { record(NRF_LINK,BLOCKED,"Module not enabled; 'nrf on' after power/wiring checks."); return; }
  bool ok = nrfCommand("ble status json","\"ble_ready\"") && (jsonValue("ble_ready","true") || jsonValue("ble_ready","false"));
  ok=nrfCommand("version","WT02E40E fw=") && ok;
  record(NRF_LINK,ok ? PASS : FAIL,"Requires a structured reply, not echoed command or boot bytes.");
}
static void wifiScan() {
  if (!nrfOn) { record(WIFI_SCAN,BLOCKED,"Enable nRF first."); return; }
  if (!nrfCommand("wifi scan json","\"type\":\"wifi_scan\"",16000)) { record(WIFI_SCAN,FAIL,"No scan response; inspect UART/firmware."); return; }
  uint32_t start = millis();
  while (jsonValue("running","true") && uint32_t(millis() - start) < 16000) {
    waitService(500);
    if (!nrfCommand("wifi scan last json","\"type\":\"wifi_scan\"")) break;
  }
  record(WIFI_SCAN,jsonValue("running","false") && jsonValue("valid","true") && jsonValue("status","0") ? PASS : FAIL,"Scan completion/status checked. Zero nearby APs can be valid; association/data transfer are separate.");
}
static void bleTest(bool send) {
  if (!nrfOn) { record(send ? BLE_TX : BLE_READY,BLOCKED,"Enable nRF first."); return; }
  nrfCommand("ble on",nullptr,300); waitService(500); // UART shell has no success line for ble on.
  bool response = nrfCommand("ble status json","\"ble_ready\"");
  bool ready = response && jsonValue("ble_ready","true");
  if (!send) { record(BLE_READY,ready ? PASS : FAIL,"Module reports BLE ready; external peer needed for over-air proof."); return; }
  if (!ready || !jsonValue("ble_connected","true") || !jsonValue("tx_notify","true")) {
    record(BLE_TX,BLOCKED,"Connect BLE client and subscribe to TX notifications, then run 'ble tx'."); return;
  }
  // Do not treat a local queue acknowledgement as remote packet delivery.
  extern void peerChallenge(bool ble,const char *ip,const char *port);
  peerChallenge(true,nullptr,nullptr);
}
struct Challenge { char token[48]; uint32_t started; bool active; } bleChallenge,wifiChallenge;
static uint32_t challengeNumber;
void peerChallenge(bool ble,const char *ip,const char *port) {
  Challenge &test=ble ? bleChallenge : wifiChallenge;
  test.active=false;
  snprintf(test.token,sizeof(test.token),"j18_%s_%lu_%lu",ble ? "ble" : "wifi",(unsigned long)millis(),(unsigned long)++challengeNumber);
  char command[160];
  if (ble) snprintf(command,sizeof(command),"tx ble %s",test.token);
  else {
    if (!validUdpTarget(ip,port,false)) { record(WIFI_PEER,BLOCKED,"Provide a real IPv4/UDP receiver port: wifi tx <ip> <port>."); return; }
    snprintf(command,sizeof(command),"tx wifi %s %s %s",ip,port,test.token);
  }
  bool ok=nrfCommand(command,ble ? "BLE TX: " : "Wi-Fi UDP TX: ",3000);
  if (!ok) { record(ble ? BLE_TX : WIFI_PEER,FAIL,"Bridge did not acknowledge payload queueing/transmission; no peer challenge started."); return; }
  test.active=true; test.started=millis();
  report.print(F("Peer must echo the received token within 60 seconds: tx uart \"j18 peer "));
  report.print(test.token); report.println(F("\" (over the SAME BLE or Wi-Fi transport)."));
  if (ble) record(BLE_TX,INFO,"Bridge accepted BLE TX; waiting for a matching external peer echo.");
  record(ble ? BLE_PEER : WIFI_PEER,INFO,"A matching token returned over the same transport will mark this roundtrip PASS.");
}
static void peerEcho(const char *token) {
  bool ble=commandSource==REMOTE_BLE;
  Challenge &test=ble ? bleChallenge : wifiChallenge;
  if ((commandSource!=REMOTE_BLE && commandSource!=REMOTE_WIFI) || !test.active || !token || strcmp(token,test.token) || uint32_t(millis()-test.started)>PEER_CHALLENGE_MS) {
    report.println(F("Peer echo rejected: wrong source, token, or expired/unstarted challenge.")); return;
  }
  test.active=false; record(ble ? BLE_PEER : WIFI_PEER,PASS,"Unique challenge received by peer and echoed over the same transport through the module UART.");
}
static void challengeTimeouts() {
  if (bleChallenge.active && uint32_t(millis()-bleChallenge.started)>PEER_CHALLENGE_MS) {
    bleChallenge.active=false; record(BLE_PEER,FAIL,"No matching BLE peer echo within the challenge deadline.");
  }
  if (wifiChallenge.active && uint32_t(millis()-wifiChallenge.started)>PEER_CHALLENGE_MS) {
    wifiChallenge.active=false; record(WIFI_PEER,FAIL,"No matching Wi-Fi peer echo within the challenge deadline.");
  }
}
static void wifiStatus() {
  if (!nrfOn) { record(WIFI_NET,BLOCKED,"nRF off."); return; }
  bool ok=nrfCommand("wifi status json","\"type\":\"wifi_status\"") && jsonValue("type","\"wifi_status\"");
  record(WIFI_NET,!ok ? FAIL : (jsonValue("assoc","true") && jsonValue("ipv4","true") ? INFO : BLOCKED),"Association/IP is module-reported. Use wifi tx <ip> <port> and a peer echo to prove a packet roundtrip.");
}
static bool lteReady() { return lteUart.active || lteUart.begin(LTE_BAUD); }
static bool lteAt(const char *text, uint32_t timeout = 3000) { return lteReady() && transact(lteUart,text,timeout,nullptr,true); }
static void lteTest() {
  if (!lteAt("AT")) {
    record(LTE_AT,FAIL,"No OK. Check modem VCC/V_INT and baud; 'lte on' issues a power-on pulse.");
    record(LTE_SIM,BLOCKED,"LTE AT unavailable."); record(LTE_NET,BLOCKED,"LTE AT unavailable.");
    record(LTE_DATA,BLOCKED,"LTE AT unavailable; no data-session test attempted."); return;
  }
  bool identified = lteAt("ATI") && responsePrefix(reply,LTE_MODEL_PREFIX);
  record(LTE_AT,identified ? PASS : FAIL,"AT plus module identity; current target is SARA-R410.");
  lteAt("AT+CGMR");
  bool sim = lteAt("AT+CPIN?") && responseLine(reply,"+CPIN: READY");
  record(LTE_SIM,sim ? PASS : BLOCKED,"SIM must be present/unlocked; inspect CPIN response.");
  lteAt("AT+CSQ");
  bool query = lteAt("AT+CEREG?");
  bool registered=query && networkRegistered(reply);
  record(LTE_NET,registered ? PASS : INFO,"CEREG checks existing registration; no APN/operator/CFUN/profile changes made.");
  lteAt("AT+CGATT?"); lteAt("AT+CGDCONT?");
  record(LTE_DATA,BLOCKED,"No APN/data peer configured; PDP activation and IP data transfer are not inferred from registration.");
}
static bool readReg(uint8_t addr, uint8_t reg, uint8_t *data, size_t n) { return i2c.transfer(addr,&reg,1,data,n); }
static bool writeReg(uint8_t addr, uint8_t reg, uint8_t value) { uint8_t bytes[] = {reg,value}; return i2c.transfer(addr,bytes,2,nullptr,0); }
static void scanI2c() {
  report.print(F("I2C SDA/SCL idle: ")); report.print(pinRead(SDA_PIN)); report.print('/'); report.println(pinRead(SCL_PIN));
  uint8_t count = 0;
  for (uint8_t addr = 8; addr <= 0x77; ++addr) {
    if (i2c.transfer(addr,nullptr,0,nullptr,0)) { report.print(F("ACK 0x")); hexByte(addr); report.println(); ++count; }
    if (i2c.fault) { report.println(F("Bus fault/clock stretch timeout; check pullups and selected branch.")); break; }
    heartbeatService(); telemetryService();
  }
  report.print(F("Devices: ")); report.println(count);
}
static void rootI2cTest() {
  i2c.begin();
  if (!pinRead(SDA_PIN) || !pinRead(SCL_PIN)) { record(I2C_ROOT,FAIL,"Root SDA/SCL stuck low; inspect pullups/power before recovery."); return; }
  scanI2c();
  record(I2C_ROOT,i2c.fault ? FAIL : INFO,"Root ACK inventory captured. Device-specific register tests establish peripheral identity; scan ACKs alone do not.");
}
static bool muxWrite(uint8_t mask) { return i2c.transfer(MUX_ADDRESS,&mask,1,nullptr,0); }
static void muxTest() {
  uint8_t saved = 0, readback = 0;
  bool savedOk = i2c.transfer(MUX_ADDRESS,nullptr,0,&saved,1);
  if (!savedOk) { record(MUX_TEST,FAIL,"Mux absent at 0x70 or host bus fault. Try 'i2c recover' after checking pullups."); return; }
  pinOutput(MUX_RST,false); waitService(1); pinInput(MUX_RST); waitService(1);
  bool ok = i2c.transfer(MUX_ADDRESS,nullptr,0,&readback,1) && readback == 0;
  report.println(F("Mux reset readback, then one-hot channels 0..7; scan exposed channels 0..3."));
  for (uint8_t channel = 0; channel < 8; ++channel) {
    uint8_t mask = 1 << channel;
    bool channelOk = muxWrite(mask) && i2c.transfer(MUX_ADDRESS,nullptr,0,&readback,1) && readback == mask;
    ok &= channelOk;
    report.print(F("Channel ")); report.print(channel); report.println(channelOk ? F(" control OK") : F(" control FAIL"));
    if (channelOk && channel < 4) scanI2c();
    if (i2c.fault) { pinOutput(MUX_RST,false); waitService(1); pinInput(MUX_RST); i2c.begin(); }
  }
  ok &= muxWrite(saved);
  record(MUX_TEST,ok ? PASS : FAIL,"Register/reset readback only; upstream devices also appear on branch scans. Original mask restored.");
}
static uint8_t bcd(uint8_t x) { return (x >> 4) * 10 + (x & 15); }
static void rtcTest(bool irq) {
  if (irq) {
    uint8_t saved;
    if (!readReg(RTC_ADDRESS,0x0e,&saved,1)) { record(RTC_SQW,FAIL,"RTC unavailable."); return; }
    pinInput(RTC_IRQ,1);
    bool ok = writeReg(RTC_ADDRESS,0x0e,saved & ~0x1c); // INTCN=0, RS=00: 1 Hz.
    uint8_t edges = 0; bool last = pinRead(RTC_IRQ); uint32_t start = millis();
    while (ok && uint32_t(millis() - start) < 3200) { bool now = pinRead(RTC_IRQ); if (now != last) { ++edges; last = now; } heartbeatService(); telemetryService(); delay(1); }
    ok &= writeReg(RTC_ADDRESS,0x0e,saved);
    pinInput(RTC_IRQ);
    record(RTC_SQW,ok && edges >= 4 && edges <= 8 ? PASS : FAIL,"1 Hz pin transitions; control register restored. Internal pullup is a lab workaround."); return;
  }
  uint8_t clock[7], status, temp[2], later=0;
  uint32_t sampleTime=millis();
  bool ok = readReg(RTC_ADDRESS,0,clock,7) && readReg(RTC_ADDRESS,0x0f,&status,1) && readReg(RTC_ADDRESS,0x11,temp,2);
  if (!ok) { record(RTC_TEST,FAIL,"No complete RTC register reply."); return; }
  if (!rtcCalendarValid(clock)) { record(RTC_TEST,FAIL,"RTC contains invalid BCD, date, day-of-week or hour format; no time written."); return; }
  report.print(F("RTC date YY-MM-DD ")); report.print(bcd(clock[6])); report.print('-'); report.print(bcd(clock[5] & 0x1f)); report.print('-'); report.println(bcd(clock[4]));
  report.print(F("Seconds/minutes ")); report.print(bcd(clock[0] & 0x7f)); report.print('/'); report.println(bcd(clock[1]));
  report.print(F("Temperature C ")); report.println(int8_t(temp[0]) + (temp[1] >> 6) * .25f,2);
  waitService(1200);
  ok = readReg(RTC_ADDRESS,0,&later,1);
  uint8_t next[7]; memcpy(next,clock,7); next[0]=later;
  unsigned elapsed=uint32_t(millis()-sampleTime)/1000,delta=(bcd(later)+60-bcd(clock[0]))%60;
  ok=ok && rtcCalendarValid(next) && elapsed<30 && delta>=elapsed && delta<=elapsed+1 && delta>0;
  record(RTC_TEST,ok && !(status & 0x80) ? PASS : (ok ? INFO : FAIL),status & 0x80 ? "Clock advances but OSF is set: time validity is not established. No time/status written." : "Seconds advance; oscillator-stop flag clear. No calendar written.");
}
static void batteryTest() {
  int raw = adcReadPin(BATTERY_PIN);
  if (raw < 0) { record(BATTERY,FAIL,"ADC acquisition timed out."); return; }
  float pinVolts = raw * ADC_VDD_VOLTS / 4095.0f;
  float battery = pinVolts * (1.0f + BATTERY_R_TOP / BATTERY_R_BOTTOM) * BATTERY_CALIBRATION;
  report.print(F("PB07 ADC=")); report.print(raw); report.print(F(" /4095; pin V=")); report.print(pinVolts,4); report.print(F("; battery V=")); report.println(battery,4);
  record(BATTERY,raw > 0 && raw < 4095 ? INFO : FAIL,"348k/750k divider; compare with meter and calibrate VDDANA. Does not measure current or certify charging.");
}

static uint8_t radioRead(uint8_t reg) { pinWrite(SD_CS,true); pinWrite(RADIO_CS,false); spi0.transfer(reg & 0x7f); uint8_t value = spi0.transfer(0); pinWrite(RADIO_CS,true); return value; }
static void radioWrite(uint8_t reg, uint8_t value) { pinWrite(SD_CS,true); pinWrite(RADIO_CS,false); spi0.transfer(reg | 0x80); spi0.transfer(value); pinWrite(RADIO_CS,true); }
static bool radioMode(uint8_t mode) {
  radioWrite(1,(radioRead(1) & 0xe3) | mode);
  uint32_t start = millis();
  while (!(radioRead(0x27) & 0x80)) { if (uint32_t(millis() - start) > 200) return false; heartbeatService(); }
  return !spi0.fault;
}
static bool radioTest() {
  if (!spiBegin(spi0,RADIO_SPI)) return false;
  pinOutput(RADIO_RST,true); delayMicroseconds(150); pinInput(RADIO_RST); waitService(10);
  pinInput(RADIO_IRQ);
  uint8_t version = radioRead(0x10); report.print(F("RFM69 RegVersion=0x")); hexByte(version); report.println();
  if (version != 0x24) { record(RADIO_SPI,FAIL,"Expected RFM69 version 0x24. Check CS, reset, pins and power."); return false; }
  uint8_t saved = radioRead(0x2f);
  radioWrite(0x2f,0x55); bool ok = radioRead(0x2f) == 0x55;
  radioWrite(0x2f,0xaa); ok &= radioRead(0x2f) == 0xaa;
  radioWrite(0x2f,saved); ok &= radioRead(0x2f) == saved && radioMode(4) && !spi0.fault;
  record(RADIO_SPI,ok ? PASS : FAIL,"Version + write/read/restore + ModeReady; no RF transmission."); return ok;
}
static bool radioConfigure(uint32_t hz) {
  if (hz < 290000000 || hz > 1020000000 || !radioTest()) return false;
  uint32_t frf = (uint64_t(hz) << 19) / 32000000;
  radioWrite(2,0); radioWrite(3,2); radioWrite(4,0x80); // Packet FSK, 50 kbps.
  radioWrite(5,1); radioWrite(6,0x9a); // ~25 kHz deviation.
  radioWrite(7,frf >> 16); radioWrite(8,frf >> 8); radioWrite(9,frf);
  radioWrite(0x11,0x50); // PA1 only, modest output power; not +20 dBm settings.
  radioWrite(0x19,0x42); radioWrite(0x25,0); // RX bandwidth, DIO0 PacketSent/PayloadReady.
  radioWrite(0x2e,0x88); radioWrite(0x2f,0x2d); radioWrite(0x30,0xd4);
  radioWrite(0x37,0x90); radioWrite(0x38,60); radioWrite(0x3c,0x8f); radioWrite(0x3d,2);
  return !spi0.fault;
}
static void radioPacket(bool tx, uint32_t hz, const char *payload) {
  if (!radioConfigure(hz)) { record(RADIO_RF,BLOCKED,"Radio unavailable or invalid frequency; use installed module/antenna band."); return; }
  if (tx) {
    size_t n = strlen(payload);
    if (!n || n > 60) { record(RADIO_RF,FAIL,"Payload must be 1..60 bytes."); return; }
    pinWrite(RADIO_CS,false); spi0.transfer(0x80); spi0.transfer(n);
    for (size_t i = 0; i < n; ++i) spi0.transfer(payload[i]); pinWrite(RADIO_CS,true);
  }
  bool ok = radioMode(tx ? 12 : 16); uint32_t start = millis(); bool irq = false;
  while (ok && uint32_t(millis() - start) < (tx ? 3000u : 10000u)) {
    uint8_t flags = radioRead(0x28);
    if (flags & (tx ? 8 : 4)) { irq = pinRead(RADIO_IRQ); break; }
    heartbeatService(); telemetryService(); delay(1);
  }
  uint8_t flags = radioRead(0x28);
  ok &= bool(flags & (tx ? 8 : 4)) && irq;
  if (!tx && ok) {
    pinWrite(RADIO_CS,false); spi0.transfer(0);
    uint8_t n = spi0.transfer(0);
    if (!n || n > 60) ok = false;
    report.print(F("RF payload: "));
    for (uint8_t i = 0; i < n && i < 60; ++i) report.write(spi0.transfer(0));
    pinWrite(RADIO_CS,true); report.println();
    ok &= bool(flags & 2); // CrcOk.
  }
  bool standby = radioMode(4);
  record(RADIO_RF,ok && standby && !spi0.fault ? (tx ? INFO : PASS) : FAIL,tx ? "PacketSent + physical DIO0 checked. A peer must confirm reception." : "PayloadReady + CRC + physical DIO0 checked; 10-second RX window.");
}

class DirectSdSpi : public SdSpiBaseClass {
public:
  void begin(SdSpiConfig) override { spi0.begin(); }
  void activate() override { pinWrite(RADIO_CS,true); }
  uint8_t receive() override { return spi0.transfer(0xff); }
  uint8_t receive(uint8_t *data,size_t n) override { for (size_t i = 0; i < n; ++i) data[i] = receive(); return spi0.fault ? 1 : 0; }
  void send(uint8_t value) override { spi0.transfer(value); }
  void send(const uint8_t *data,size_t n) override { for (size_t i = 0; i < n; ++i) send(data[i]); }
  void setSckSpeed(uint32_t hz) override { spi0.speed(hz); }
} sdDriver;
static SdFs sd;
void sdCsInit(SdCsPin_t) { pinOutput(SD_CS,true); }
void sdCsWrite(SdCsPin_t, bool high) { pinWrite(RADIO_CS,true); pinWrite(SD_CS,high); }
static bool sdMount() {
  if (!spiBegin(spi0,SD_MOUNT)) return false;
  bool ok = sd.begin(SdSpiConfig(14,SHARED_SPI,SD_SCK_HZ(250000),&sdDriver)) && !spi0.fault;
  if (!ok) { sd.initErrorPrint(&report); record(SD_MOUNT,FAIL,"Card missing, wiring/power issue or no supported FAT/exFAT filesystem."); }
  else { report.print(F("SD sectors: ")); report.println(sd.card()->sectorCount()); record(SD_MOUNT,PASS,"Card initialized and FAT/exFAT mounted over selected SPI transport; no format performed."); }
  return ok;
}
static void sdFileTest() {
  if (!sdMount()) { record(SD_WRITE,BLOCKED,"Mount failed."); return; }
  FsFile file; char path[16]; bool created = false;
  for (unsigned i = 0; i < 1000; ++i) {
    snprintf(path,sizeof(path),"J18%05u.TST",i);
    if (!sd.exists(path)) { created = file.open(path,O_RDWR | O_CREAT | O_EXCL); break; }
  }
  if (!created) { record(SD_WRITE,FAIL,"Could not create a new exclusive test file; no existing file overwritten."); return; }
  bool ok = true;
  for (uint16_t i = 0; i < 1024; ++i) { uint8_t byte = (i * 73u + 19u) & 0xff; if (file.write(byte) != 1) { ok = false; break; } heartbeatService(); }
  ok &= file.sync(); ok &= file.close();
  if (ok) ok = file.open(path,O_RDONLY) && file.fileSize() == 1024;
  if (ok) for (uint16_t i = 0; i < 1024; ++i) if (file.read() != int((i * 73u + 19u) & 0xff)) { ok = false; break; }
  file.close();
  bool removed = sd.remove(path);
  report.print(F("Test file: ")); report.print(path); report.println(removed ? F(" removed") : F(" retained; removal failed"));
  record(SD_WRITE,ok && removed && !spi0.fault ? PASS : FAIL,"Exclusive new file: 1024-byte pattern write, sync/close, reopen/readback and cleanup. SD CRC enabled.");
}
static void sdList() {
  if (!sdMount()) return;
  FsFile directory=sd.open("/",O_RDONLY),entry; uint32_t started=millis(); unsigned count=0;
  if (!directory) { report.println(F("SD root directory open failed.")); return; }
  while (count<32 && uint32_t(millis()-started)<5000 && entry.openNext(&directory,O_RDONLY)) {
    entry.printName(&report); report.print(entry.isDir() ? F(" /\n") : F(" bytes="));
    if (!entry.isDir()) report.println(entry.fileSize());
    entry.close(); ++count; heartbeatService(); telemetryService();
  }
  directory.close(); report.println(F("Listing limited to 32 root entries / 5 seconds."));
}
static void gpioLoop(Pin out, Pin in) {
  if (!freePin(out) || !freePin(in) || samePin(out,in)) { record(GPIO_TEST,BLOCKED,"Use two distinct free header GPIOs; peripheral/power/USB/SWD pins are excluded."); return; }
  bool wasEnabled=heartbeatEnabled(); heartbeatEnable(false); pinOutput(out,false); bool ok = true;
  for (uint8_t i = 0; i < 32; ++i) {
    bool high=((0xa76935c1ul>>i)&1); pinInput(in,high ? -1 : 1);
    pinWrite(out,high); delayMicroseconds(200); ok &= pinRead(in)==high;
  }
  pinInput(out); pinInput(in); heartbeatEnable(wasEnabled);
  record(GPIO_TEST,ok ? PASS : FAIL,"32-bit pattern with opposing weak input pulls; external jumper required; pins released afterward.");
}
static void spiLoop(uint8_t index) {
  BoardSpi &bus = index ? spi1 : spi0;
  TestId id=index ? SPI_LOOP1 : SPI_LOOP0;
  if (!spiBegin(bus,id)) return;
  bool ok = true;
  for (unsigned value = 0; value < 256; ++value) ok &= bus.transfer(value) == value;
  record(id,ok && !bus.fault ? PASS : FAIL,"Jumper selected bus MOSI to MISO. Both on-board CS lines remain high.");
}
static void watch(Pin pin, uint32_t ms) {
  pinInput(pin); bool last = pinRead(pin); uint32_t edges = 0, start = millis();
  while (uint32_t(millis() - start) < ms) { bool now = pinRead(pin); if (now != last) { ++edges; last = now; } heartbeatService(); telemetryService(); }
  printPin(pin); report.print(F(" state=")); report.print(last); report.print(F(" edges=")); report.println(edges);
  report.println(F("Observation only; no expected device event was generated."));
}
static void sdi12(const char *text) {
  if (!J18_REVISED_SDI12) { record(SDI_TEST,BLOCKED,"Original TXB0102/OE circuit needs redesign; revised DIR profile remains disabled."); return; }
  char response[96]; const char *error;
  bool ok=sdi12Exchange(text,response,sizeof(response),error);
  report.print(F("SDI response: ")); report.println(response);
  record(SDI_TEST,ok ? PASS : FAIL,ok ? "Inverted 1200 7E1, parity/stop bits, CRLF and sensor address checked. Field electrical compliance still needs scope/cable tests." : error);
}
static void help() {
  report.print(F(
    "\nJolteon J18 feature verification (115200 USB serial, newline commands)\n"
    "help | pins | issues | all | summary | clear\n"
    "info | boot | led test|clear|status   Device/reset/boot CRC and visual check\n"
    "report auto|both|off|status   USB primary, BLE fallback by default\n"
    "report wifi <ip> <port>       Add Wi-Fi UDP destination; 0.0.0.0 0 disables\n"
    "blink on|off                  PA05 external LED, 500 ms\n"
    "spi map | spi hard | spi soft Hardware default; software workaround explicit\n"
    "spi loop 0|1                  MOSI-to-MISO jumper required\n"
    "nrf on|off|reset|probe         Inspect R20 and RX=P1.05 first\n"
    "nrf cmd <bridge command>       Adds 'wt ' prefix\n"
    "nrfraw <exact UART line>       No prefix; custom firmware testing\n"
    "wifi scan | wifi status | wifi tx <ip> <port> | ble test | ble tx\n"
    "peer <token>                  Wireless peer echo; USB cannot pass this test\n"
    "lte on | lte reset | lte probe | lte <AT command>\n"
    "bridge nrf|lte                Raw UART terminal; '~.' exits\n"
    "i2c scan | i2c recover | i2c read <addr> <reg> <count>\n"
    "i2c write <addr> <reg> <byte>   Decimal or 0x-prefixed numbers\n"
    "mux test | mux off | mux <0..7>\n"
    "rtc | rtc irq | battery | adc <PAxx/PBxx>\n"
    "radio test                    RFM69 version/write/read/restore\n"
    "radio tx <Hz> <text>           Explicit RF packet; matching antenna/peer\n"
    "radio rx <Hz>                  10 s matching FSK receive/IRQ test\n"
    "sd read | sd list | sd test    test creates/verifies/removes one new file\n"
    "gpio read <pin> | gpio write <pin> <0|1> | gpio release <pin>\n"
    "gpio loop <output> <input>     External jumper, free header pins only\n"
    "irq nrf|radio|rtc              3 s pin transition observation\n"
    "sdi <command!> | sdi power on|off  Original circuit blocked; revised DIR profile optional\n"
    "All leaves module power unchanged, does not send RF, and does not write SD.\n"));
}
static void pins() {
  report.println(F("NRF: TX PA22 SERCOM3/C -> P1.05; RX PA23 <- P1.04; EN PA13; RESET PA14; IRQ PA15 -> P1.06."));
  report.print(F("LTE: TX ")); printPin(LTE_TX); report.print(F(" / RX ")); printPin(LTE_RX);
  report.print(F(" SERCOM1/C RXPO=")); report.println(LTE_RX_PAD);
  report.println(F("PWR gate PB30; RESET gate PB31. HIGH asserts gate."));
  report.println(F("I2C: PA08/PA09 software open-drain; TCA 0x70 RESET PB23; DS3231 0x68 IRQ PA19."));
  report.print(F("SPI0 SCK/MISO/MOSI/CSradio: ")); printPin(SPI0_SCK); report.print('/'); printPin(SPI0_MISO); report.print('/'); printPin(SPI0_MOSI); report.print('/'); printPin(RADIO_CS); report.println();
  report.print(F("SPI1 SCK/MISO/MOSI: ")); printPin(SPI1_SCK); report.print('/'); printPin(SPI1_MISO); report.print('/'); printPin(SPI1_MOSI); report.println();
  report.print(F("RFM69 RESET ")); printPin(RADIO_RST); report.println(F(" DIO0 PA28; SD CS PB14; battery PB07/AIN15 348k/750k; SDI PB15."));
  report.print(F("Heartbeat/status ")); printPin(HEARTBEAT); report.print(F(" TX LED ")); printPin(TX_LED);
  report.print(F(" RX LED ")); printPin(RX_LED); report.print(F(" extra LEDs enabled=")); report.println(ENABLE_EXTRA_LEDS);
  if (J18_REVISED_SDI12) { report.print(F("SDI DIR ")); printPin(SDI_DIR); report.print(F(" power EN ")); printPin(SDI_POWER_EN); report.println(); }
  mappingTests();
}
static void allTests() {
  baselineRan=true; ++runNumber; memset(outcomes,0,sizeof(outcomes)); memset(testedAt,0,sizeof(testedAt));
  bleChallenge.active=wifiChallenge.active=false; indicatorsClear(); indicateBusy(true);
  report.println(F("Starting baseline: no RF transmission, SD write, or modem profile changes."));
  deviceInfo(); bootImageTest(); mappingTests(); nrfTest();
  record(NRF_POWER,nrfOn ? INFO : BLOCKED,nrfOn ? "EN high and UART initialized; actual supply needs a meter." : "nRF off; review R20/U10 before enabling PA13.");
  record(NRF_IRQ_TEST,BLOCKED,"P1.06/PA15 IRQ is not driven by the current bridge firmware; UART/power control are separate tests.");
  if (nrfOn) { wifiScan(); wifiStatus(); bleTest(false); }
  else { record(WIFI_SCAN,BLOCKED,"nRF off."); record(WIFI_NET,BLOCKED,"nRF off."); record(BLE_READY,BLOCKED,"nRF off."); }
  lteTest(); rootI2cTest(); muxTest(); rtcTest(false); rtcTest(true); radioTest(); sdMount(); batteryTest();
  if (outcomes[LTE_DATA]==NOT_RUN) record(LTE_DATA,BLOCKED,"LTE AT unavailable; no data-session test attempted.");
  record(BLE_TX,BLOCKED,"Explicit ble tx requires a connected client subscribed to TX.");
  record(BLE_PEER,BLOCKED,"Run ble tx and echo its unique token from the BLE client.");
  record(WIFI_PEER,BLOCKED,"Run wifi tx <ip> <port> and echo from the receiving Wi-Fi peer.");
  record(RADIO_RF,BLOCKED,"Use radio tx/rx with an antenna and matching FSK peer.");
  record(SD_WRITE,BLOCKED,"Use sd test to authorize a new test file write/read/remove.");
  record(GPIO_TEST,BLOCKED,"Use gpio loop with a physical jumper.");
  record(SPI_LOOP0,BLOCKED,"Use spi loop 0 with a MOSI-to-MISO jumper; map must be supported or spi soft selected.");
  record(SPI_LOOP1,BLOCKED,"Use spi loop 1 with a MOSI-to-MISO jumper; map must be supported or spi soft selected.");
  record(SDI_TEST,BLOCKED,J18_REVISED_SDI12 ? "Power/connect an SDI sensor and issue sdi <address-command!>." : "Original TXB0102/OE circuit requires redesign; no field-bus drive attempted.");
  record(POWER_CHECK,BLOCKED,"PGOOD/STAT1/STAT2 are not routed to MCU inputs. Check solar charging, rails, 3.8 V LTE supply and off-state back-power with instruments.");
  indicateBusy(false); summary(); report.println(F("Baseline finished. BLOCKED items need the stated external setup; expected SPI mapping FAILs are retained."));
}
static void processLine(char *line) {
  if (bridge) {
    if (!strcmp(line,"~.")) { bridge = nullptr; report.println(F("Raw bridge closed.")); return; }
    bridge->write(line); bridge->write("\r\n"); return;
  }
  // Preserve payload spacing and quoting for the bridge's own command parser.
  if (!strncmp(line,"nrfraw ",7)) { transact(nrfUart,line + 7,4000,nullptr,false); return; }
  if (!strncmp(line,"nrf cmd ",8)) { nrfCommand(line + 8,nullptr,4000); return; }
  if (!strncmp(line,"lte AT",6)) { lteAt(line + 4,10000); return; }
  char *save = nullptr;
  char *a = strtok_r(line," ", &save), *b = strtok_r(nullptr," ",&save), *c = strtok_r(nullptr," ",&save), *d = strtok_r(nullptr," ",&save), *e = strtok_r(nullptr," ",&save);
  if (!a) return;
  uint32_t x,y,z; Pin p,q;
  if (!strcmp(a,"help")) help();
  else if (!strcmp(a,"pins")) pins();
  else if (!strcmp(a,"issues")) report.println(F("See HARDWARE_ISSUES.md: SPI0/1 invalid clocks; nRF P1.05/P1.06 mismatch; R20 10-ohm shunt; SDI12 TXB/OE; battery back-power; C27 underrating; USB core LED conflict. Revised PCB changes require explicit pin/circuit profiles."));
  else if (!strcmp(a,"all")) allTests();
  else if (!strcmp(a,"summary")) summary();
  else if (!strcmp(a,"info")) deviceInfo();
  else if (!strcmp(a,"boot")) bootImageTest();
  else if (!strcmp(a,"peer")) peerEcho(b);
  else if (!strcmp(a,"led") && b) {
    if (!strcmp(b,"clear")) indicatorsClear();
    else if (!strcmp(b,"status")) {
      report.print(F("PA05 heartbeat/status enabled=")); report.print(heartbeatEnabled());
      report.print(F("; extra TX/RX LEDs enabled=")); report.println(ENABLE_EXTRA_LEDS);
    } else if (!strcmp(b,"test")) {
      indicateTx(); waitService(400); indicateRx(); waitService(200);
      indicateResult(true); waitService(800); indicateResult(false); waitService(2100); indicatorsClear();
      report.println(F("Visual test completed: TX double pulse, RX flash, PASS two long pulses, FAIL four short pulses. No peripheral delivery is inferred."));
    } else report.println(F("Usage: led test|clear|status"));
  }
  else if (!strcmp(a,"report") && b) {
    if (!strcmp(b,"status")) telemetryStatus();
    else if (!strcmp(b,"wifi")) report.println(telemetryTarget(c,d) ? F("Wi-Fi report target updated; configure Wi-Fi association separately.") : F("Invalid IPv4/port."));
    else if (!strcmp(b,"auto") || !strcmp(b,"both") || !strcmp(b,"off")) telemetryMode(b);
    else report.println(F("Usage: report auto|both|off|status|wifi <ip> <port>"));
  }
  else if (!strcmp(a,"clear")) { memset(outcomes,0,sizeof(outcomes)); memset(testedAt,0,sizeof(testedAt)); indicatorsClear(); summary(); }
  else if (!strcmp(a,"blink") && b) heartbeatEnable(!strcmp(b,"on"));
  else if (!strcmp(a,"spi") && b) {
    if (!strcmp(b,"map")) mappingTests();
    else if (!strcmp(b,"hard") || !strcmp(b,"soft")) { spi0.software = spi1.software = !strcmp(b,"soft"); report.println(spi0.software ? F("SOFTWARE SPI enabled explicitly; hardware mapping still fails.") : F("Hardware SPI selected.")); }
    else if (!strcmp(b,"loop") && number(c,x,1)) spiLoop(x);
    else report.println(F("Usage: spi map|hard|soft|loop <0|1>"));
  }
  else if (!strcmp(a,"nrf") && b) {
    if (!strcmp(b,"on")) nrfPower(true); else if (!strcmp(b,"off")) nrfPower(false);
    else if (!strcmp(b,"reset")) { nrfPower(false); waitService(100); nrfPower(true); }
    else if (!strcmp(b,"probe")) nrfTest(); else report.println(F("Usage: nrf on|off|reset|probe|cmd ..."));
  }
  else if (!strcmp(a,"wifi") && b) { if (!strcmp(b,"scan")) wifiScan(); else if (!strcmp(b,"status")) wifiStatus(); else if (!strcmp(b,"tx")) peerChallenge(false,c,d); else report.println(F("Usage: wifi scan|status|tx <ip> <port>")); }
  else if (!strcmp(a,"ble") && b) { if (!strcmp(b,"test") || !strcmp(b,"tx")) bleTest(!strcmp(b,"tx")); else report.println(F("Usage: ble test|tx")); }
  else if (!strcmp(a,"lte") && b) {
    if (!strcmp(b,"probe")) lteTest();
    else if (!strcmp(b,"on")) { lteReady(); pinWrite(LTE_PWR,true); waitService(500); pinWrite(LTE_PWR,false); waitService(5000); report.println(F("500 ms power gate pulse done. Use lte probe.")); }
    else if (!strcmp(b,"reset")) { pinWrite(LTE_RST,true); waitService(100); pinWrite(LTE_RST,false); waitService(5000); report.println(F("Hard reset gate pulse done; LTE state may be lost.")); }
  }
  else if (!strcmp(a,"bridge") && b) {
    if (commandSource!=REMOTE_NONE || !Serial) { report.println(F("Raw UART bridge requires the USB console; wireless commands use nrf cmd or lte AT...")); return; }
    if (!strcmp(b,"nrf") && nrfOn) bridge = &nrfUart;
    else if (!strcmp(b,"lte") && lteReady()) bridge = &lteUart;
    if (bridge) report.println(F("Raw UART bridge: '~.' alone exits. nRF shell commands require wt prefix."));
    else report.println(F("Selected UART is unavailable."));
  }
  else if (!strcmp(a,"i2c") && b) {
    if (!strcmp(b,"scan")) rootI2cTest();
    else if (!strcmp(b,"recover")) report.println(i2c.recover() ? F("I2C lines released.") : F("I2C recovery failed."));
    else if (!strcmp(b,"read") && number(c,x,0x77) && number(d,y,255) && number(e,z,32) && z) {
      uint8_t data[32]; bool ok = readReg(x,y,data,z);
      if (ok) { for (uint8_t i=0;i<z;++i) { hexByte(data[i]); report.print(' '); } report.println(); } else report.println(F("I2C read failed."));
    }
    else if (!strcmp(b,"write") && number(c,x,0x77) && number(d,y,255) && number(e,z,255)) report.println(writeReg(x,y,z) ? F("I2C write ACKed.") : F("I2C write failed."));
    else report.println(F("Usage: i2c scan|recover|read addr reg count|write addr reg byte"));
  }
  else if (!strcmp(a,"mux") && b) { if (!strcmp(b,"test")) muxTest(); else if (!strcmp(b,"off")) report.println(muxWrite(0) ? F("Mux off.") : F("Mux write failed.")); else if (number(b,x,7)) report.println(muxWrite(1 << x) ? F("Mux selected.") : F("Mux write failed.")); }
  else if (!strcmp(a,"rtc")) rtcTest(b && !strcmp(b,"irq"));
  else if (!strcmp(a,"battery")) batteryTest();
  else if (!strcmp(a,"adc") && parsePin(b,p) && (freePin(p) || samePin(p,BATTERY_PIN))) { bool enabled=heartbeatEnabled(); heartbeatEnable(false); int raw = adcReadPin(p); report.print(F("ADC raw=")); report.println(raw); heartbeatEnable(enabled); }
  else if (!strcmp(a,"sd") && b) { if (!strcmp(b,"test")) sdFileTest(); else if (!strcmp(b,"list")) sdList(); else if (!strcmp(b,"read")) sdMount(); else report.println(F("Usage: sd read|list|test")); }
  else if (!strcmp(a,"radio") && b) {
    if (!strcmp(b,"test")) radioTest();
    else if (number(c,x,1020000000) && (!strcmp(b,"tx") || !strcmp(b,"rx"))) {
      char payload[80] = "";
      if (d) snprintf(payload,sizeof(payload),"%s%s%s%s%s",d,e ? " " : "",e ? e : "",save && *save ? " " : "",save ? save : "");
      radioPacket(!strcmp(b,"tx"),x,payload);
    } else report.println(F("Usage: radio test|tx Hz text|rx Hz"));
  }
  else if (!strcmp(a,"gpio") && b && parsePin(c,p)) {
    if (!strcmp(b,"read") && (freePin(p) || samePin(p,BATTERY_PIN))) { pinInput(p); printPin(p); report.print('='); report.println(pinRead(p)); }
    else if (!strcmp(b,"write") && freePin(p) && number(d,x,1)) { if (samePin(p,HEARTBEAT)) heartbeatEnable(false); pinOutput(p,x); }
    else if (!strcmp(b,"release") && freePin(p)) { if (samePin(p,HEARTBEAT)) heartbeatEnable(false); pinInput(p); }
    else if (!strcmp(b,"loop") && parsePin(d,q)) gpioLoop(p,q);
    else report.println(F("Pin reserved or invalid GPIO command."));
  }
  else if (!strcmp(a,"irq") && b) { if (!strcmp(b,"nrf")) watch(NRF_IRQ,3000); else if (!strcmp(b,"radio")) watch(RADIO_IRQ,3000); else if (!strcmp(b,"rtc")) watch(RTC_IRQ,3000); }
  else if (!strcmp(a,"sdi") && b) {
    if (!strcmp(b,"power") && c && (!strcmp(c,"on") || !strcmp(c,"off"))) report.println(sdi12Power(!strcmp(c,"on")) ? F("SDI power control set; actual 12 V needs measurement.") : F("SDI power profile unavailable or pin readback failed."));
    else sdi12(b);
  }
  else report.println(F("Unknown/incomplete command; type help."));
}
void verificationBegin() {
  indicatorsBegin();
  pinOutput(NRF_EN,false); pinInput(NRF_RST); pinInput(NRF_IRQ);
  pinInput(NRF_TX); pinInput(NRF_RX); pinInput(LTE_TX); pinInput(LTE_RX);
  pinOutput(LTE_PWR,false); pinOutput(LTE_RST,false);
  pinOutput(RADIO_CS,true); pinOutput(SD_CS,true); pinInput(RADIO_RST); pinInput(RADIO_IRQ);
  pinInput(MUX_RST); pinInput(RTC_IRQ); pinInput(BATTERY_PIN); pinInput(SDI_PIN); i2c.begin();
  if (J18_REVISED_SDI12) { pinOutput(SDI_DIR,false); if (SDI_HAS_POWER_ENABLE) pinOutput(SDI_POWER_EN,false); }
  Serial.begin(115200); // No while(!Serial): heartbeat runs without USB.
  report.println(F("Jolteon verification starting. Type help. Wireless fallback requires corrected nRF power/UART."));
  mappingTests();
}
void verificationLoop() {
  heartbeatService();
  challengeTimeouts();
  bool opened = bool(Serial);
  if (opened && !usbWasOpen) { help(); mappingTests(); report.print(F("j18> ")); }
  usbWasOpen = opened;
  static bool fallbackStarted;
  if (!opened && !fallbackStarted && millis() >= 5000) {
    fallbackStarted = true;
    if (AUTO_WIRELESS_FALLBACK && NRF_POWER_PATH_VERIFIED) {
      if (!nrfOn) nrfPower(true);
      nrfCommand("ble on",nullptr,300);
      help(); mappingTests();
      report.println(F("Wireless ready: subscribe to BLE TX and send tx uart \"j18 all\" via BLE command characteristic."));
    } else report.println(F("Automatic nRF power blocked until NRF_POWER_PATH_VERIFIED=true after U10/R20 correction."));
  }
  if (RUN_BASELINE_ON_BOOT && !baselineRan && millis()>=5000) allTests();
  if (bridge) {
    telemetryPause(true);
    for (unsigned i=0;i<256;++i) { int c=bridge->read(); if (c<0) break; if (Serial) Serial.write(uint8_t(c)); }
  } else {
    telemetryPause(false); telemetryService();
    char remoteLine[384];
    if (telemetryCommand(remoteLine,sizeof(remoteLine),commandSource)) {
      processLine(remoteLine); commandSource=REMOTE_NONE;
      report.print(F("j18> "));
    }
  }
  for (unsigned budget=0;budget<128 && Serial.available();++budget) {
    int c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (discardLongLine) { report.println(F("Command too long; discarded without execution.")); }
      else if (commandLength) { commandLine[commandLength] = 0; processLine(commandLine); }
      commandLength = 0; discardLongLine = false; report.print(F("j18> "));
    } else if (c == 8 || c == 127) { if (commandLength) --commandLength; }
    else if (c >= 32 && c < 127 && !discardLongLine) {
      if (commandLength + 1 < sizeof(commandLine)) commandLine[commandLength++] = char(c);
      else { commandLength = 0; discardLongLine = true; }
    }
  }
}
