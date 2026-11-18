#pragma once
#include "config.h"

bool samePin(Pin a, Pin b);
void pinInput(Pin p, int pull = 0);
void pinOutput(Pin p, bool high);
void pinWrite(Pin p, bool high);
bool pinRead(Pin p);
void pinMux(Pin p, uint8_t function);
bool parsePin(const char *name, Pin &p);
bool freePin(Pin p); // Reserved/connected peripheral pins cannot be general outputs.
int adcReadPin(Pin p);
void heartbeatService();
void heartbeatEnable(bool enabled);
bool heartbeatEnabled();
void waitService(uint32_t ms);

class RawUart {
public:
  RawUart(Sercom *s, uint8_t index, Pin tx, Pin rx, uint8_t rxPad=1) : s(s), index(index), tx(tx), rx(rx), rxPad(rxPad) {}
  bool begin(uint32_t baud);
  void end();
  bool write(const char *text);
  int read();
  void discard();
  void irq();
  bool active = false;
  volatile uint32_t errors = 0, dropped = 0;
private:
  Sercom *s;
  uint8_t index;
  Pin tx, rx;
  uint8_t rxPad;
  volatile uint16_t head = 0, tail = 0;
  uint8_t ring[2048];
};
extern RawUart nrfUart, lteUart;

class BoardSpi {
public:
  explicit BoardSpi(uint8_t bus) : bus(bus) {}
  bool begin();
  uint8_t transfer(uint8_t value);
  void speed(uint32_t hz);
  bool mappingValid() const;
  bool software = false, fault = false;
private:
  uint8_t bus;
  uint32_t hz = 100000;
  Pin sck() const { return bus ? SPI1_SCK : SPI0_SCK; }
  Pin miso() const { return bus ? SPI1_MISO : SPI0_MISO; }
  Pin mosi() const { return bus ? SPI1_MOSI : SPI0_MOSI; }
  Sercom *regs() const { return bus ? SERCOM5 : SERCOM4; }
};
extern BoardSpi spi0, spi1;

class SoftI2c {
public:
  void begin();
  bool transfer(uint8_t address, const uint8_t *tx, size_t nt, uint8_t *rx, size_t nr);
  bool recover();
  bool fault = false;
private:
  void low(Pin p);
  void release(Pin p);
  bool clockHigh();
  bool start();
  void stop();
  bool put(uint8_t value);
  uint8_t get(bool ack);
};
extern SoftI2c i2c;
