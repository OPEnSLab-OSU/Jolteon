#include "BoardIO.h"
#include "Sdi12.h"
#include <assert.h>
#include <string>
#include <vector>
#include <iostream>
static uint32_t nowUs,receiveStart;
static bool states[2][32],inputMode,receiving,paused,badParity,badStop;
static std::string sensorResponse;
struct Edge { uint32_t time; bool high; };
static std::vector<Edge> edges;
uint32_t micros() { return nowUs++; }
uint32_t millis() { return nowUs/1000; }
void waitService(uint32_t ms) { nowUs+=ms*1000; }
void heartbeatService() {}
void indicateTx() {}
void indicateRx() {}
void telemetryPause(bool on) { paused=on; }
void pinInput(Pin pin,int) { if (pin.port==SDI_PIN.port && pin.bit==SDI_PIN.bit) inputMode=true; }
void pinWrite(Pin pin,bool high) {
  bool previous=states[pin.port][pin.bit]; states[pin.port][pin.bit]=high;
  if (pin.port==SDI_PIN.port && pin.bit==SDI_PIN.bit) edges.push_back({nowUs,high});
  if (pin.port==SDI_DIR.port && pin.bit==SDI_DIR.bit && previous && !high) { receiveStart=nowUs+1000; receiving=true; }
}
void pinOutput(Pin pin,bool high) { pinWrite(pin,high); if (pin.port==SDI_PIN.port && pin.bit==SDI_PIN.bit) inputMode=false; }
bool pinRead(Pin pin) {
  nowUs+=2;
  if (pin.port!=SDI_PIN.port || pin.bit!=SDI_PIN.bit || !inputMode || !receiving || int32_t(nowUs-receiveStart)<0) return states[pin.port][pin.bit];
  unsigned bit=(nowUs-receiveStart)/833,character=bit/10; bit%=10;
  if (character>=sensorResponse.size()) return false;
  uint8_t byte=sensorResponse[character],parity=0;
  for (unsigned i=0;i<7;++i) parity^=(byte>>i)&1;
  if (!bit) return true;
  if (bit<=7) return !((byte>>(bit-1))&1);
  if (bit==8) return !(parity^(badParity && character==0));
  return badStop && character==0;
}
static void reset(const char *response) {
  nowUs=receiveStart=0; inputMode=true; receiving=paused=badParity=badStop=false;
  for (auto &port:states) for (bool &state:port) state=false;
  sensorResponse=response; edges.clear();
}
static bool at(uint32_t time) { bool high=false; for (const Edge &edge:edges) { if (edge.time>time) break; high=edge.high; } return high; }
static void released() { assert(inputMode && !states[SDI_DIR.port][SDI_DIR.bit] && !paused); }
int main() {
  char response[96]; const char *error;
  reset("0MODEL\r\n");
  assert(sdi12Exchange("0I!",response,sizeof(response),error) && !error && !strcmp(response,"0MODEL\r\n"));
  released(); assert(edges.size()>4 && edges[1].high && !edges[2].high && edges[2].time-edges[1].time>=13000);
  uint32_t first=edges[3].time; assert(first-edges[2].time>=9000);
  const char *command="0I!";
  for (unsigned character=0;character<3;++character) {
    uint32_t start=first+character*8330; assert(at(start+400));
    uint8_t byte=0,parity=0;
    for (unsigned bit=0;bit<7;++bit) { bool one=!at(start+(bit+1)*833+416); byte|=uint8_t(one)<<bit; parity^=uint8_t(one); }
    assert(byte==uint8_t(command[character])); assert(!at(start+8*833+416)==bool(parity)); assert(!at(start+9*833+416));
  }
  reset("0MODEL\r\n"); badParity=true;
  assert(!sdi12Exchange("0I!",response,sizeof(response),error) && strstr(error,"parity")); released();
  reset("0MODEL\r\n"); badStop=true;
  assert(!sdi12Exchange("0I!",response,sizeof(response),error) && strstr(error,"framing")); released();
  reset("1MODEL\r\n");
  assert(!sdi12Exchange("0I!",response,sizeof(response),error) && strstr(error,"address")); released();
  reset("");
  assert(!sdi12Exchange("0I!",response,sizeof(response),error) && nowUs<2000000); released();
  struct { char bytes[8]; uint32_t guard; } bounded={{0},0xcafebabe};
  reset("0MODEL-TOO-LONG\r\n");
  assert(!sdi12Exchange("0I!",bounded.bytes,sizeof(bounded.bytes),error) && strstr(error,"overflow") && bounded.guard==0xcafebabe); released();
  reset(""); assert(sdi12Power(true) && states[SDI_POWER_EN.port][SDI_POWER_EN.bit]);
  assert(sdi12Power(false) && !states[SDI_POWER_EN.port][SDI_POWER_EN.bit]);
  std::cout<<"PASS: actual SDI driver TX waveform and RX decoding; parity/framing/address errors; deadline/buffer bounds; DIR release; independent sensor power\n";
}
