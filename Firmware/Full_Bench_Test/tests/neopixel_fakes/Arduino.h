#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#define F(text) text
#define OUTPUT 1
struct FakeSerial {
  template<class T> void print(const T &) {}
  template<class T> void println(const T &) {}
};
extern FakeSerial Serial;
void pinMode(int pin,int mode);
