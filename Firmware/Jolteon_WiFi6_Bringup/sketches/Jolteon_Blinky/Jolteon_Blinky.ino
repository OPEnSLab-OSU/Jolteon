#include <Arduino.h>
#ifndef J18_CORE_USB_LEDS_DISABLED
#error Use the Jolteon board profile or tools/build_samd.py so USB cannot drive radio reset / PB03.
#endif

// Minimal current-PCB test: external LED + series resistor on PA05.
// No automatic radio/module power, UART traffic, or waiting for a USB client.
void setup() {
  PORT->Group[0].PINCFG[5].reg=0;
  PORT->Group[0].OUTCLR.reg=1ul<<5;
  PORT->Group[0].DIRSET.reg=1ul<<5;
}
void loop() {
  PORT->Group[0].OUTTGL.reg=1ul<<5;
  delay(500);
}
