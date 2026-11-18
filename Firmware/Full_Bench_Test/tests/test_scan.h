#pragma once
#include "../ScanLogic.h"
#include "../Logic.h"
#include <assert.h>
#include <stdio.h>
#include <string>

inline void testScan() {
  uint8_t fields[6]={};
  assert(scanNumbers("AT+CFUN?\r\n+CFUN: 1,0\r\nOK\r\n","+CFUN:",fields,2) && fields[0]==1 && fields[1]==0);
  assert(scanNumbers("+CFUN: 4\r\n","+CFUN:",fields,1) && fields[0]==4);
  assert(!scanNumbers("+CFUN: 1,junk\r\n","+CFUN:",fields,1));
  assert(!scanNumbers("+CFUN: 256\r\n","+CFUN:",fields,1));
  assert(!scanNumbers("+CFUN: -1\r\n","+CFUN:",fields,1));
  assert(!scanNumbers("URC +CFUN: 1\r\n","+CFUN:",fields,1));
  assert(scanNumbers("+CESQ: 99,99,255,255,20,80\r\nOK\r\n","+CESQ:",fields,6) && fields[4]==20 && fields[5]==80);
  assert(scanNumbers("+CESQ: 99,99,255,255,255,255\r\n","+CESQ:",fields,6) && fields[5]==255);
  assert(!scanNumbers("+CESQ: 99,99,255,255,20\r\n","+CESQ:",fields,6));
  assert(!scanNumbers("+CESQ: 99,99,255,255,20,80junk\r\n","+CESQ:",fields,6));

  CopsScanCounter scan;
  const std::string response="AT+COPS=?\r\n+CEREG: 2\r\n+COPS: (2,\"Carrier, A\",\"A\",\"310410\",7),(1,\"B (LTE)\",\"B\",\"23415\",9),(3,\"C\",\"C\",\"310260\"),,(0-4),(0-2)\r\nOK\r\n";
  for (char ch:response) scan.feed(ch);
  assert(scan.operators==3); // Includes forbidden networks: receiving is distinct from permission to register.
  CopsScanCounter malformed;
  const char *bad="+COPS: (1,\"A\",\"A\",\"31041x\",7),(1,\"B\",\"B\",\"1234\"),(99,\"C\",\"C\",\"310410\"),(0-4),(0-2)\r\n+COPS: (1,\"D\",\"D\",\"310410\"\r\n";
  while (*bad) malformed.feed(*bad++);
  assert(malformed.operators==0);
  CopsScanCounter longScan;
  PendingAtReply terminal; terminal.begin("",false);
  std::string large="+COPS: ";
  for (int i=0;i<150;++i) large+="(1,\"A\",\"A\",\"310410\",7),";
  large+="(0-4),(0-2)\r\nOK\r\n";
  assert(large.size()>2048);
  bool completed=false;
  for (char ch:large) { longScan.feed(ch); if (terminal.feed(ch)) { completed=true; assert(lineEquals(terminal.line,"OK")); } }
  assert(longScan.operators==150 && completed && !terminal.active);
  puts("PASS RF query parsing and streaming operator lists (quoted commas, malformed replies, >2 KB, final OK)");
}
