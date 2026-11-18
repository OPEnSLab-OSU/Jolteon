#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string>
#include <sstream>
#define F(s) s
struct Sercom {};
extern Sercom *SERCOM4,*SERCOM5;
uint32_t millis();
uint32_t micros();
class Print {
public:
  virtual ~Print() {}
  virtual size_t write(uint8_t)=0;
  size_t write(const char *text) { size_t n=0; while (*text) n+=write(uint8_t(*text++)); return n; }
  size_t print(const char *text) { return write(text); }
  template<class T> size_t print(T value) { std::ostringstream out; out<<value; return write(out.str().c_str()); }
  size_t println() { return write("\r\n"); }
  template<class T> size_t println(T value) { size_t n=print(value); return n+println(); }
};
class FakeSerial : public Print {
public:
  using Print::write;
  bool connected=false;
  std::string output;
  operator bool() const { return connected; }
  int availableForWrite() const { return 4096; }
  size_t write(uint8_t c) override { output+=char(c); return 1; }
};
extern FakeSerial Serial;
