#pragma once
#include "Arduino.h"
#include <vector>
using neoPixelType=uint16_t;
constexpr neoPixelType NEO_RGB=0,NEO_GRB=1,NEO_KHZ800=2;
struct PixelFrame { int16_t pin; std::vector<uint32_t> colors; uint8_t brightness; };
extern std::vector<PixelFrame> frames;
extern std::vector<int> begun;
class Adafruit_NeoPixel {
  int16_t pin; uint8_t brightness=255; std::vector<uint32_t> colors;
public:
  Adafruit_NeoPixel(uint16_t count,int16_t data,neoPixelType):pin(data),colors(count) {}
  void setPin(int16_t data) { pin=data; }
  int16_t getPin() const { return pin; }
  void updateLength(uint16_t count) { colors.assign(count,0); }
  uint16_t numPixels() const { return uint16_t(colors.size()); }
  void setBrightness(uint8_t value) { brightness=value; }
  void begin() { begun.push_back(pin); }
  void show() { frames.push_back({pin,colors,brightness}); }
  bool canShow() { return true; }
  uint32_t Color(uint8_t r,uint8_t g,uint8_t b) { return (uint32_t(r)<<16)|(uint32_t(g)<<8)|b; }
  void setPixelColor(uint16_t index,uint32_t color) { colors.at(index)=color; }
};
