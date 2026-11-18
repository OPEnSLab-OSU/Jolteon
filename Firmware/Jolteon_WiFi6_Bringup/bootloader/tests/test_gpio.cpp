// Exercise the actual boot GPIO implementation against logged registers.
#include <cassert>
#include <cstdio>
#include "host/sam.h"
#include "../src/jolteon_board.h"
#include "../src/jolteon_vectors.h"
std::vector<Write> writes;
Port fakePort;
#include "../src/jolteon_board.c"

int main() {
  assert(jolteon_app_vectors_valid(0x20008000,0x2001));
  assert(jolteon_app_vectors_valid(0x20007ff8,0x3ffff));
  assert(!jolteon_app_vectors_valid(0xffffffff,0xffffffff));
  assert(!jolteon_app_vectors_valid(0x20007ffc,0x2001));
  assert(!jolteon_app_vectors_valid(0x20008000,0x2000));
  assert(!jolteon_app_vectors_valid(0x20008000,0x1001));
  assert(!jolteon_app_vectors_valid(0x20008000,0x40001));
  assert(!jolteon_app_vectors_valid(0x20000000,0x2001));
  for (unsigned group=0;group<2;++group) {
    auto& p=fakePort.Group[group];
    p.DIRCLR.reg={group,DIR_CLEAR,0}; p.OUTSET.reg={group,OUT_SET,0};
    p.OUTCLR.reg={group,OUT_CLEAR,0}; p.DIRSET.reg={group,DIR_SET,0};
    for (unsigned pin=0;pin<32;++pin) p.PINCFG[pin].reg={group,PIN_CONFIG,pin};
  }
  jolteon_board_early_init();
  size_t i=0;
  auto output=[&](unsigned pin,unsigned high) {
    const unsigned g=pin/32, bit=pin%32;
    assert(i+3<=writes.size());
    const auto a=writes[i++],b=writes[i++],c=writes[i++];
    assert(a.group==g && a.kind==PIN_CONFIG && a.pin==bit && a.value==0);
    assert(b.group==g && b.kind==static_cast<unsigned>(high?OUT_SET:OUT_CLEAR) && b.value==(1ul<<bit));
    assert(c.group==g && c.kind==DIR_SET && c.value==(1ul<<bit));
  };
#define EXPECT_OUTPUT(name, level) output(J18_##name, level);
  J18_SAFE_OUTPUTS(EXPECT_OUTPUT)
#undef EXPECT_OUTPUT
  auto input=[&](unsigned pin) {
    const unsigned g=pin/32, bit=pin%32;
    assert(i+2<=writes.size());
    const auto a=writes[i++],b=writes[i++];
    assert(a.group==g && a.kind==DIR_CLEAR && a.value==(1ul<<bit));
    assert(b.group==g && b.kind==PIN_CONFIG && b.pin==bit && b.value==0);
  };
#define EXPECT_INPUT(name) input(J18_##name);
  J18_HIGH_IMPEDANCE(EXPECT_INPUT)
#undef EXPECT_INPUT
#if J18_BOOT_INDICATOR
  output(J18_LED,0);
#endif
  assert(i==writes.size()); // No undeclared or reserved pin was touched.
  static_assert(J18_NRF_EN==13 && J18_LED==5, "Review current PCB assignments.");
  bool sawRadioLow=false;
  for (auto w:writes) {
    if (w.group==0 && w.kind==OUT_CLEAR && (w.value&(1ul<<27))) sawRadioLow=true;
    if (w.group==0 && w.kind==DIR_SET) assert(!(w.value&(1ul<<17))); // LTE RX.
    if (w.group==1 && w.kind==DIR_SET) assert(!(w.value&(1ul<<3))); // Former RX LED.
    if (w.group==0 && w.kind==OUT_SET) assert(!(w.value&(1ul<<27))); // Radio must not pulse.
  }
  assert(sawRadioLow);
  std::puts("PASS: application vector guards, boot output ordering, high impedance states, and conflicting LED pins.");
}
