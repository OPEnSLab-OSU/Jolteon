#include <cassert>
#include <OPEnS_RTC.cpp>
RegisterWire Wire;
uint32_t ticks=0;
uint32_t millis() { return ticks++; }
void delay(uint32_t n) { ticks+=n; }
int main() {
  RTC_DS3231 rtc;
  assert(rtc.begin());
  const DateTime utc(2026,11,4,21,3,30);
  assert(rtc.adjustChecked(utc));
  assert(rtc.now().unixtime()==utc.unixtime());
  // Both stale flags + oscillator-stop + 32kHz. Only flags should be consumed.
  Wire.regs[14]=0xfc; Wire.regs[15]=0x8b;
  assert(rtc.setAlarm1(utc+TimeSpan(10),DS3231_A1_Date));
  assert(Wire.regs[7]==0x40 && Wire.regs[8]==0x03 && Wire.regs[9]==0x21 && Wire.regs[10]==0x04);
  assert((Wire.regs[14]&7)==5 && (Wire.regs[14]&0xe0)==0);
  assert(Wire.regs[15]==0x88);
  assert(rtc.getAlarm1Mode()==DS3231_A1_Date);
  const DateTime alarm=rtc.getAlarm1();
  assert(alarm.day()==4 && alarm.hour()==21 && alarm.minute()==3 && alarm.second()==40);
  Wire.regs[15]|=3;
  assert(rtc.alarmFired(1) && rtc.alarmFired(2));
  assert(rtc.clearAlarm(1) && !rtc.alarmFired(1) && rtc.alarmFired(2));
  assert(rtc.clearAlarm() && Wire.regs[15]==0x88);
  assert(rtc.disableAlarm(1) && rtc.disableAlarm(2) && (Wire.regs[14]&3)==0);
  // Date and weekday modes use the day/date register, never the hour byte.
  assert(rtc.setAlarm1(utc,DS3231_A1_Day));
  assert(Wire.regs[9]==0x21 && Wire.regs[10]==(0x40|utc.dayOfTheWeek()+1));
  const DateTime midnight=DateTime(2028,2,29,23,59,55)+TimeSpan(10);
  assert(rtc.setAlarm1(midnight,DS3231_A1_Date));
  assert(Wire.regs[7]==0x05 && Wire.regs[8]==0 && Wire.regs[9]==0 && Wire.regs[10]==0x01);
  Wire.failWrite=true; assert(!rtc.setAlarm1(utc,DS3231_A1_Date) && !rtc.lastOperationSucceeded());
  Wire.shortRead=true; assert(!rtc.clearAlarm() && !rtc.lastOperationSucceeded());
  assert(!rtc.clearAlarm(0) && !rtc.alarmFired(3));
  puts("PASS: actual OPEnS DS3231 time/alarm registers, masks, flag clearing, rollover and I2C failures");
}
