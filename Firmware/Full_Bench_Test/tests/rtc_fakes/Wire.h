#pragma once
#include "Arduino.h"
#include <vector>
struct RegisterWire {
  uint8_t regs[256]={},pointer=0;
  bool failWrite=false,shortRead=false;
  std::vector<uint8_t> tx,rx; size_t pos=0;
  void begin() {}
  void beginTransmission(uint8_t) { tx.clear(); }
  size_t write(uint8_t v) { tx.push_back(v); return 1; }
  size_t write(const uint8_t *data,size_t n) { tx.insert(tx.end(),data,data+n); return n; }
  uint8_t endTransmission(bool=true) {
    if (failWrite) { failWrite=false; return 2; }
    if (!tx.empty()) { pointer=tx[0]; for (size_t i=1;i<tx.size();++i) regs[pointer++]=tx[i]; }
    return 0;
  }
  uint8_t requestFrom(uint8_t,uint8_t n) {
    rx.clear(); pos=0;
    if (shortRead) { shortRead=false; --n; }
    while (n--) rx.push_back(regs[pointer++]);
    return uint8_t(rx.size());
  }
  int available() { return int(rx.size()-pos); }
  int read() { return available()?rx[pos++]:-1; }
};
extern RegisterWire Wire;
