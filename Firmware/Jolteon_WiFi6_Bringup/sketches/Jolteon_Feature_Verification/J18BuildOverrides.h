#pragma once
// The launcher passes this header to EVERY C++ translation unit, including
// USB/USBCore.cpp. Including/undefining only in the .ino would not fix the core.
#include <Arduino.h>
#undef PIN_LED_TXL
#undef PIN_LED_RXL
#define J18_CORE_USB_LEDS_DISABLED 1
