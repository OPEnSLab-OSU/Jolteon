#include <cassert>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <vector>
#define F(text) text
#include <Internet/Connectivity/Loom_LTE/Loom_LTE_SaraR5Data.h>
struct Transcript {
  std::vector<std::string> commands;
  std::vector<int8_t> replies={1,2,1,1,1,1,1};
  std::vector<uint32_t> waits;
  void append(std::ostringstream &) {}
  template<class T,class... Rest> void append(std::ostringstream &out,T value,Rest... rest) {
    out<<value; append(out,rest...);
  }
  template<class... Args> void sendAT(Args... args) {
    std::ostringstream out; append(out,args...); commands.push_back(out.str());
  }
  int8_t waitResponse(uint32_t timeout) {
    waits.push_back(timeout); assert(waits.size()<=replies.size());
    return replies[waits.size()-1];
  }
};
int main() {
  Transcript t;
  assert(!loomSaraR5OpenData(t,"hologram","",""));
  const std::vector<std::string> expected={"+CMEE=2","+UPSDA=0,4","+CGATT=1",
    "+UPSD=0,1,\"hologram\"","+UPSD=0,0,0","+UPSD=0,100,1","+UPSDA=0,3"};
  assert(t.commands==expected);
  // Carrier-expanded APNs never enter a strcmp/CGDCONT search. CID 1 is the
  // explicit context configured by Loom; profile APN precedes mapping/activation.
  for (size_t step=0;step<expected.size();++step) {
    Transcript failed; failed.replies[step]=0;
    assert(loomSaraR5OpenData(failed,"hologram","",""));
    assert(failed.commands.size()==step+1); // No subsequent AT after timeout.
    if (step!=1) {
      Transcript error; error.replies[step]=2;
      assert(loomSaraR5OpenData(error,"hologram","",""));
      assert(error.commands.size()==step+1);
    }
  }
  Transcript auth; auth.replies={1,2,1,1,1,1,1,1,1};
  assert(!loomSaraR5OpenData(auth,"private.apn","user","secret"));
  assert(auth.commands[2]=="+CGACT=0,1");
  assert(auth.commands[3]=="+UAUTHREQ=1,3,\"user\",\"secret\"");
  for (const char *bad:{"","bad\"apn","bad\rapn","bad\\apn"}) {
    Transcript invalid; assert(loomSaraR5OpenData(invalid,bad,"","")); assert(invalid.commands.empty());
  }
  Transcript longAuth;
  const std::string overlong(51,'x');
  assert(loomSaraR5OpenData(longAuth,"hologram",overlong.c_str(),""));
  assert(longAuth.commands.empty());
  puts("PASS: actual R5 CID/profile sequence, APN before mapping, IPv4 before activation, auth and stage failures");
}
