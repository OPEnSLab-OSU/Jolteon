#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#ifdef PIN_LED_TXL
#error USB must not drive PA27 as a TX LED.
#endif
#ifdef PIN_LED_RXL
#error USB must not drive PB03 as an RX LED.
#endif
static_assert(PINS_COUNT == 52, "Review package pin table if MCU changes.");
static_assert(PIN_NRF_TX == J18_PIN_PA22, "WT TX route changed.");
static_assert(PIN_LTE_RX == J18_PIN_PA17, "Current PCB LTE RX route changed.");

// Compilation example only. Call after validating and powering both modules.
void uartApiExample() {
  SerialWT.begin(115200);
  SerialLTE.begin(115200);
}

void setup() {
  Serial.begin(115200); // Native USB; no wait for a client.
  pinMode(LED_BUILTIN, OUTPUT);
  Wire.begin(); // PA08/PA09, SERCOM0.
  analogReadResolution(12);
  Serial.print("Battery ADC: ");
  Serial.println(analogRead(PIN_BATTERY)); // PB07 / AIN15.
  // Current PCB: SPI_INTERFACES_COUNT == 0. SPI.begin() intentionally fails
  // to compile because PB10 cannot provide the required SPI0 clock pad.
  // Use explicit software SPI or rewire before enabling hardware SPI.
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  delay(500);
}
