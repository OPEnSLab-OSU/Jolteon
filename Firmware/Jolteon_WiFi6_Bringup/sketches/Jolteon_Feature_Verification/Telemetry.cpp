#include "Telemetry.h"
#include "Protocol.h"
#include <string.h>

ReportConsole report;
static char queue[TELEMETRY_QUEUE_BYTES],rxLine[384],remote[384],frozenCommand[256];
static uint16_t head,tail,rxUsed,pendingBytes;
static uint32_t dropped,sequence,sentAt,retryAt,acknowledged,timeouts,commandDrops,rxFaults;
static uint32_t seenErrors,seenDrops;
static bool paused,pending,frameReady,rxOverflow,commandReady,escape;
static RemoteSource remoteSource=REMOTE_NONE;
enum Route { AUTO,BOTH,OFF };
static Route route=AUTO;
static char targetIp[16]="0.0.0.0",targetPort[6]="0",expectedTag[25];
static unsigned expectedPayload;
static bool due(uint32_t now,uint32_t deadline) { return int32_t(now-deadline)>=0; }
size_t ReportConsole::write(uint8_t c) {
  if (Serial) Serial.write(c);
  if (route!=OFF && (route==BOTH || !Serial)) {
    uint16_t next=(head+1)%sizeof(queue);
    if (next==tail) ++dropped;
    else { queue[head]=char(c); head=next; }
  }
  return 1;
}
void telemetryPause(bool pause) { paused=pause; }
void telemetryMode(const char *mode) {
  if (!strcmp(mode,"auto")) route=AUTO;
  else if (!strcmp(mode,"both")) route=BOTH;
  else if (!strcmp(mode,"off")) { route=OFF; head=tail=0; pending=frameReady=false; }
  report.println(F("Report route updated. auto=USB primary/BLE fallback; both=USB plus wireless."));
}
bool telemetryTarget(const char *ip,const char *port) {
  if (!validUdpTarget(ip,port,true)) return false;
  uint32_t number;
  if (!parseUnsigned(port,number,65535)) return false;
  strcpy(targetIp,ip); snprintf(targetPort,sizeof(targetPort),"%lu",(unsigned long)number);
  pending=frameReady=false; retryAt=millis(); // Next attempt gets a new tag.
  return true;
}
void telemetryStatus() {
  report.print(F("Report queue=")); report.print((head+sizeof(queue)-tail)%sizeof(queue));
  report.print(F(" dropped=")); report.print(dropped); report.print(F(" ack=")); report.print(acknowledged);
  report.print(F(" timeout=")); report.print(timeouts); report.print(F(" remote_drop=")); report.print(commandDrops);
  report.print(F(" rx_fault=")); report.println(rxFaults);
  report.print(F("UDP=")); report.print(targetIp); report.print(':'); report.println(targetPort);
  report.println(F("Wireless ack means locally queued, not peer delivery. Frozen retries retain J18# tag and content."));
}
static void receivedLine() {
  rxLine[rxUsed]=0;
  if (pending && bridgeAck(rxLine,expectedPayload,expectedTag)) {
    tail=(tail+pendingBytes)%sizeof(queue); pending=frameReady=false; ++acknowledged;
    retryAt=millis()+80;
  } else if (pending && !strncmp(rxLine,"err tx both ",12) && strstr(rxLine,expectedTag)) {
    pending=false; retryAt=millis()+TELEMETRY_RETRY_MS;
  }
  const char *prefixes[]={"BLE UART TX: j18 ","WIFI UART TX: j18 ","BRIDGE UART TX: j18 "};
  for (unsigned i=0;i<3;++i) {
    size_t n=strlen(prefixes[i]);
    if (!strncmp(rxLine,prefixes[i],n)) {
      if (commandReady) { ++commandDrops; return; }
      strcpy(remote,rxLine+n); remoteSource=RemoteSource(i+1); commandReady=true;
      return;
    }
  }
}
void telemetryFeed(uint8_t c) {
  if (c==27) { escape=true; return; }
  if (escape) { if (c>='@' && c<='~' && c!='[') escape=false; return; }
  if (c=='\r' || c=='\n') {
    if (rxUsed && !rxOverflow) receivedLine();
    if (rxOverflow) ++rxFaults;
    rxUsed=0; rxOverflow=false;
  } else if (c>=32 && c<127) {
    if (rxUsed+1<sizeof(rxLine)) rxLine[rxUsed++]=char(c);
    else rxOverflow=true;
  }
}
bool telemetryCommand(char *out,size_t size,RemoteSource &source) {
  if (!commandReady) return false;
  if (!out || strlen(remote)>=size) { ++commandDrops; commandReady=false; return false; }
  strcpy(out,remote); source=remoteSource; commandReady=false; return true;
}
bool telemetryCommand(char *out,size_t size) {
  RemoteSource source; return telemetryCommand(out,size,source);
}
static void freezeFrame() {
  char payload[128];
  snprintf(expectedTag,sizeof(expectedTag),"J18#%lu",(unsigned long)sequence++);
  size_t used=snprintf(payload,sizeof(payload),"%s ",expectedTag);
  uint16_t at=tail; pendingBytes=0;
  while (at!=head && pendingBytes<TELEMETRY_CHUNK_BYTES && used+4<sizeof(payload)) {
    char ch=queue[at]; at=(at+1)%sizeof(queue); ++pendingBytes;
    if (ch=='\r') continue;
    if (ch=='\n') { payload[used++]=' '; payload[used++]='|'; payload[used++]=' '; }
    else if (ch>=32 && ch<127) payload[used++]=ch;
  }
  payload[used]=0; expectedPayload=unsigned(used);
  size_t out=snprintf(frozenCommand,sizeof(frozenCommand),"wt tx both %s %s \"",targetIp,targetPort);
  for (size_t i=0;i<used;++i) {
    if (payload[i]=='"' || payload[i]=='\\') frozenCommand[out++]='\\';
    frozenCommand[out++]=payload[i];
  }
  strcpy(frozenCommand+out,"\"\r\n"); frameReady=true;
}
void telemetryService() {
  if (paused) return;
  // Replay reports generated before USB opened, rather than stranding them.
  if (route==AUTO && Serial && head!=tail) {
    pending=frameReady=false;
    for (unsigned i=0;i<128 && tail!=head && Serial.availableForWrite()>0;++i) {
      Serial.write(uint8_t(queue[tail])); tail=(tail+1)%sizeof(queue);
    }
  }
  if (!nrfUart.active) return;
  if (seenErrors!=nrfUart.errors || seenDrops!=nrfUart.dropped) {
    seenErrors=nrfUart.errors; seenDrops=nrfUart.dropped; ++rxFaults;
    rxUsed=0; rxOverflow=escape=false;
  }
  for (unsigned i=0;i<256;++i) { int c=nrfUart.read(); if (c<0) break; telemetryFeed(uint8_t(c)); }
  if (pending && uint32_t(millis()-sentAt)>TELEMETRY_ACK_MS) {
    pending=false; ++timeouts; retryAt=millis()+TELEMETRY_RETRY_MS;
  }
  if (pending || head==tail || route==OFF || (route==AUTO && Serial) || !due(millis(),retryAt)) return;
  if (!frameReady) freezeFrame();
  if (nrfUart.write(frozenCommand)) { pending=true; sentAt=millis(); }
  else retryAt=millis()+TELEMETRY_RETRY_MS;
}
