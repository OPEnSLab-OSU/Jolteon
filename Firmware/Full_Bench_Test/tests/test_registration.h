#pragma once
#include "../Registration.h"
#include <assert.h>
#include <string>
#include <vector>

struct RegistrationStep {
  std::string command,reply;
  bool ok,finished,sensitive,cancel;
  uint32_t elapsed;
};
struct FakeRegistration : RegistrationIO {
  std::vector<RegistrationStep> steps;
  size_t position=0;
  std::string current;
  uint32_t clock=0;
  bool final=true,stop=false,cancelOnPause=false;
  void add(const char *cmd,const char *reply="\r\nOK\r\n",bool ok=true,bool completed=true,bool secret=false,bool cancelled=false,uint32_t elapsed=1) {
    steps.push_back({cmd,reply,ok,completed,secret,cancelled,elapsed});
  }
  void check(int status) {
    add("AT"); add("AT+CGMM","\r\nSARA-R510M8S\r\nOK\r\n"); add("AT+CPIN?","\r\n+CPIN: READY\r\nOK\r\n");
    char text[64]; snprintf(text,sizeof(text),"\r\n+CEREG: 2,%d\r\nOK\r\n",status); add("AT+CEREG?",text);
  }
  void setup(bool att=false,const char *auth="AT+UAUTHREQ=1,0,\"\",\"\"") {
    add("AT+CFUN=0"); add("AT+CGDCONT=1,\"IP\",\"hologram\""); add(auth,"\r\nOK\r\n",true,true,true);
    add("AT+CFUN=1"); if (!att) add("AT+COPS?","+COPS: 2\r\nOK\r\n");
    add(att?"AT+COPS=1,2,\"310410\",7":"AT+COPS=0");
  }
  bool command(const char *cmd,uint32_t timeout,bool secret=false) override {
    assert(timeout>0); assert(position<steps.size());
    const RegistrationStep &s=steps[position++]; assert(s.command==cmd); assert(s.sensitive==secret);
    current=s.reply; clock+=s.elapsed; final=s.finished; stop=s.cancel; return s.ok;
  }
  const char *response() const override { return current.c_str(); }
  bool completed() const override { return final; }
  bool cancelled() const override { return stop; }
  uint32_t now() const override { return clock; }
  void pause(uint32_t ms) override { clock+=ms; if (cancelOnPause) stop=true; }
  void phase(const char *,bool) override {}
  void consumed() { assert(position==steps.size()); }
};
inline void testRegistration() {
  const NetworkCredentials plain={"hologram","","",0};
  const NetworkCredentials auth={"hologram","test-user","test-password",1};
  for (int status : {1,5}) {
    FakeRegistration io; io.check(status);
    assert(registerIfNeeded(io,plain,true)==RegistrationResult::ALREADY_REGISTERED); io.consumed();
  }
  for (bool att : {false,true}) {
    FakeRegistration io; io.check(0); io.setup(att);
    io.add("AT+CEREG?","+CEREG: 2\r\n+CEREG: 2,2\r\nOK\r\n");
    io.add("AT+CEREG?","+CEREG: 2,5,\"1234\",\"5678\",7\r\nOK\r\n");
    assert(registerIfNeeded(io,plain,att)==RegistrationResult::REGISTERED); io.consumed();
  }
  {
    FakeRegistration io; io.check(0); io.setup();
    io.steps.pop_back(); io.steps.back().reply="+COPS: 0\r\nOK\r\n"; // Automatic mode already set: no redundant search command.
    io.add("AT+CEREG?","+CEREG: 0,5\r\nOK\r\n");
    assert(registerIfNeeded(io,plain,false)==RegistrationResult::REGISTERED); io.consumed();
  }
  {
    FakeRegistration io; io.check(2); io.setup(false,"AT+UAUTHREQ=1,1,\"test-user\",\"test-password\"");
    io.add("AT+CEREG?","+CEREG: 0,1\r\nOK\r\n");
    assert(registerIfNeeded(io,auth,false)==RegistrationResult::REGISTERED); io.consumed();
  }
  for (const char *sim : {"+CPIN: SIM PIN\r\nOK\r\n","+CPIN: SIM PUK\r\nOK\r\n","+CME ERROR: 10\r\n"}) {
    FakeRegistration io; io.add("AT"); io.add("AT+CGMM","SARA-R510M8S\r\nOK\r\n");
    io.add("AT+CPIN?",sim); assert(registerIfNeeded(io,plain,false)==RegistrationResult::SIM_NOT_READY); io.consumed();
  }
  {
    FakeRegistration io; io.add("AT","",false,false);
    assert(registerIfNeeded(io,plain,false)==RegistrationResult::NO_AT); io.consumed();
  }
  {
    FakeRegistration io; io.add("AT"); io.add("AT+CGMM","SARA-R410\r\nOK\r\n");
    assert(registerIfNeeded(io,plain,false)==RegistrationResult::WRONG_MODEL); io.consumed();
  }
  {
    FakeRegistration io; io.check(0); io.steps.back().reply="+CEREG: 2\r\nOK\r\n";
    assert(registerIfNeeded(io,plain,false)==RegistrationResult::STATUS_UNREADABLE); io.consumed();
  }
  for (NetworkCredentials bad : {NetworkCredentials{"hologram\"\rAT+CFUN=0","","",0}, NetworkCredentials{"hologram","u","p",0},NetworkCredentials{"hologram","","",4}}) {
    FakeRegistration io; io.check(0); assert(registerIfNeeded(io,bad,false)==RegistrationResult::INVALID_CREDENTIALS); io.consumed();
  }
  {
    FakeRegistration io; io.check(0); io.add("AT+CFUN=0");
    io.add("AT+CGDCONT=1,\"IP\",\"hologram\"","ERROR\r\n",false);
    io.add("AT+CFUN=1"); assert(registerIfNeeded(io,plain,false)==RegistrationResult::CONFIG_FAILED); io.consumed();
  }
  {
    FakeRegistration io; io.check(0); io.add("AT+CFUN=0");
    io.add("AT+CGDCONT=1,\"IP\",\"hologram\"","",false,false);
    assert(registerIfNeeded(io,plain,false)==RegistrationResult::CONFIG_FAILED); io.consumed(); // No overlapping RF restore.
  }
  {
    FakeRegistration io; io.check(0); io.add("AT+CFUN=0"); io.add("AT+CGDCONT=1,\"IP\",\"hologram\"");
    io.add("AT+UAUTHREQ=1,0,\"\",\"\"","ERROR\r\n",false,true,true);
    io.add("AT+CFUN=1"); assert(registerIfNeeded(io,plain,false)==RegistrationResult::CONFIG_FAILED); io.consumed();
  }
  {
    FakeRegistration io; io.check(0); io.setup();
    io.add("AT+CEREG?","+CEREG: 0,3\r\nOK\r\n");
    assert(registerIfNeeded(io,plain,false)==RegistrationResult::DENIED); io.consumed();
  }
  {
    FakeRegistration io; io.check(0); io.setup(); io.steps.back().ok=false;
    io.add("AT+CEREG?","+CEREG: 0,0\r\nOK\r\n");
    assert(registerIfNeeded(io,plain,false)==RegistrationResult::SELECTION_FAILED); io.consumed();
  }
  {
    FakeRegistration io; io.check(0); io.setup(); io.steps.back().ok=false;
    io.add("AT+CEREG?","+CEREG: 0,5\r\nOK\r\n");
    assert(registerIfNeeded(io,plain,false)==RegistrationResult::REGISTERED); io.consumed(); // Query wins race with COPS error.
  }
  {
    FakeRegistration io; io.check(0); io.setup();
    io.steps.back().ok=false; io.steps.back().finished=false; io.steps.back().elapsed=180000;
    assert(registerIfNeeded(io,plain,false)==RegistrationResult::TIMED_OUT); io.consumed();
  }
  {
    FakeRegistration io; io.check(0); io.setup(); io.steps.back().ok=false; io.steps.back().finished=false; io.steps.back().cancel=true;
    assert(registerIfNeeded(io,plain,false)==RegistrationResult::CANCELLED); io.consumed();
  }
  {
    FakeRegistration io; io.check(0); io.setup(); io.cancelOnPause=true;
    io.add("AT+CEREG?","+CEREG: 0,2\r\nOK\r\n");
    assert(registerIfNeeded(io,plain,false)==RegistrationResult::CANCELLED); io.consumed();
  }
  {
    FakeRegistration io; io.check(0); io.setup();
    for (int i=0;i<60;++i) io.add("AT+CEREG?","+CEREG: 0,2\r\nOK\r\n");
    assert(registerIfNeeded(io,plain,false)==RegistrationResult::TIMED_OUT); io.consumed();
  }
  assert(credentialsValid(plain) && credentialsValid(auth));
  assert(!atFieldSafe("password\nAT+RESET",50)); assert(!atFieldSafe("bad\\quote",50));
  assert(registrationStatus("noise+CEREG: 0,1\r\n")==-1);
  assert(registrationStatus("+CEREG: 2\r\n+CEREG: 5, 1,\"ABCD\",\"1234\",7\r\nOK\r\n")==1);
  assert(registrationStatus("+CEREG: 0,10\r\n")==10 && !registration("+CEREG: 0,10\r\n"));
  assert(registrationStatus("+CEREG: 0,1junk\r\n")==-1);
  assert(registrationStatus("+CEREG: 0,111\r\n")==-1);
  assert(operatorMode("+COPS: 0\r\nOK\r\n")==0);
  assert(operatorMode("+COPS: 1,2,\"310410\",7\r\nOK\r\n")==1);
  assert(operatorMode("+COPS: 11\r\n")==-1 && operatorMode("garbage +COPS: 0\r\n")==-1);
  PendingAtReply pending;
  pending.begin("+COPS: 0\r\nO",false);
  assert(pending.active && !pending.feed('K') && !pending.feed('\r') && pending.feed('\n') && !pending.active);
  pending.begin("",true);
  for (char ch:std::string("AT+UAUTHREQ=...\r\nNOT OK\r\n")) assert(!pending.feed(ch));
  assert(pending.active && pending.hidden);
  for (char ch:std::string("+CME ERROR: 30\r")) assert(!pending.feed(ch));
  assert(pending.feed('\n') && !pending.active && !pending.hidden);
  pending.begin("",false);
  for (int i=0;i<100;++i) assert(!pending.feed('x'));
  for (char ch:std::string("OK\r\n")) assert(!pending.feed(ch)); // Long line cannot masquerade as terminal OK.
  assert(pending.active);
  for (char ch:std::string("ERROR\r")) assert(!pending.feed(ch));
  assert(pending.feed('\n') && !pending.active);
  puts("PASS conditional R5 registration, no-op preservation, SIM locks, auto/ATT selection, private authentication, denial, timeout and cancellation");
}
