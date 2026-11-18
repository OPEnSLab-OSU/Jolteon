#pragma once
// Zero compatibility launchers force-include this in every C++ translation unit.
#include <Arduino.h>
#undef PIN_LED_TXL
#undef PIN_LED_RXL
#define J18_CORE_USB_LEDS_DISABLED 1
