#pragma once
#include "../neopixel_fakes/Arduino.h"
#define ARDUINO 100
using byte=uint8_t;
using boolean=bool;
class __FlashStringHelper;
#define PROGMEM
#define pgm_read_byte(address) (*(const unsigned char *)(address))
#define memcpy_P memcpy
uint32_t millis();
void delay(uint32_t);
