#include "BoardIO.h"
#include "Indicators.h"
#include "Telemetry.h"
#include <ctype.h>
#include <stdlib.h>

RawUart nrfUart(SERCOM3, 3, NRF_TX, NRF_RX), lteUart(SERCOM1, 1, LTE_TX, LTE_RX,LTE_RX_PAD);
BoardSpi spi0(0), spi1(1);
SoftI2c i2c;
bool samePin(Pin a, Pin b) { return a.port == b.port && a.bit == b.bit; }
void pinWrite(Pin p, bool high) {
  if (high) PORT->Group[p.port].OUTSET.reg = 1ul << p.bit;
  else PORT->Group[p.port].OUTCLR.reg = 1ul << p.bit;
}
void pinInput(Pin p, int pull) {
  PORT->Group[p.port].PINCFG[p.bit].reg = PORT_PINCFG_INEN;
  PORT->Group[p.port].DIRCLR.reg = 1ul << p.bit;
  if (pull) {
    pinWrite(p, pull > 0);
    PORT->Group[p.port].PINCFG[p.bit].bit.PULLEN = 1;
  }
}
void pinOutput(Pin p, bool high) {
  PORT->Group[p.port].PINCFG[p.bit].reg = PORT_PINCFG_INEN;
  pinWrite(p, high); // Set latch before driving to avoid a control pulse.
  PORT->Group[p.port].DIRSET.reg = 1ul << p.bit;
}
bool pinRead(Pin p) { return (PORT->Group[p.port].IN.reg >> p.bit) & 1; }
void pinMux(Pin p, uint8_t function) {
  uint8_t mux = PORT->Group[p.port].PMUX[p.bit / 2].reg;
  if (p.bit & 1) mux = (mux & 0x0f) | (function << 4);
  else mux = (mux & 0xf0) | function;
  PORT->Group[p.port].PMUX[p.bit / 2].reg = mux;
  PORT->Group[p.port].PINCFG[p.bit].reg = PORT_PINCFG_PMUXEN | PORT_PINCFG_INEN;
}
bool parsePin(const char *name, Pin &p) {
  if (!name || toupper(name[0]) != 'P' || (toupper(name[1]) != 'A' && toupper(name[1]) != 'B')) return false;
  char *end;
  long bit = strtol(name + 2, &end, 10);
  if (end == name + 2 || *end || bit < 0 || bit > 31) return false;
  p = {uint8_t(toupper(name[1]) == 'B'), uint8_t(bit)};
  return true;
}
bool freePin(Pin p) {
  const Pin reserved[]={NRF_EN,NRF_RST,NRF_IRQ,NRF_TX,NRF_RX,LTE_TX,LTE_RX,LTE_PWR,LTE_RST,
    SDA_PIN,SCL_PIN,MUX_RST,RTC_IRQ,RADIO_RST,RADIO_IRQ,RADIO_CS,SD_CS,SDI_PIN,BATTERY_PIN,
    SPI0_SCK,SPI0_MISO,SPI0_MOSI,SPI1_SCK,SPI1_MISO,SPI1_MOSI};
  for (Pin pin:reserved) if (samePin(p,pin)) return false;
  if (J18_STOCK_BOOTLOADER_PINMAP && (samePin(p,PA(17)) || samePin(p,PA(27)) || samePin(p,PB(3)))) return false;
  if (ENABLE_EXTRA_LEDS && (samePin(p,TX_LED) || samePin(p,RX_LED))) return false;
  if (J18_REVISED_SDI12 && (samePin(p,SDI_DIR) || (SDI_HAS_POWER_ENABLE && samePin(p,SDI_POWER_EN)))) return false;
  if (p.port == 0) return (p.bit >= 2 && p.bit <= 7) || (p.bit >= 10 && p.bit <= 12) || p.bit == 18 || p.bit == 20 || p.bit == 21;
  return p.bit <= 6 || p.bit == 8 || p.bit == 9;
}
void waitService(uint32_t ms) {
  uint32_t start = millis();
  while (uint32_t(millis() - start) < ms) { heartbeatService(); telemetryService(); delay(1); }
}
static bool uartSync(Sercom *s) {
  uint32_t start = micros();
  while (s->USART.SYNCBUSY.reg || s->USART.CTRLA.bit.SWRST) {
    if (uint32_t(micros() - start) > 100000) return false;
  }
  return true;
}
bool RawUart::begin(uint32_t baud) {
  if (baud < 1200 || baud > 1000000) return false;
  end();
  // These are the verified SERCOM1/3 routes used by the current and optional
  // stock-compatible profiles. Reject arbitrary config.h pin edits explicitly.
  if (index==3 && (!samePin(tx,PA(22)) || !samePin(rx,PA(23)) || rxPad!=1)) return false;
  if (index==1 && (!samePin(tx,PA(16)) || (!(samePin(rx,PA(17)) && rxPad==1) && !(samePin(rx,PA(18)) && rxPad==2)))) return false;
  PM->APBCMASK.reg |= index == 1 ? PM_APBCMASK_SERCOM1 : PM_APBCMASK_SERCOM3;
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(index == 1 ? SERCOM1_GCLK_ID_CORE : SERCOM3_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  uint32_t start = micros();
  while (GCLK->STATUS.bit.SYNCBUSY) if (uint32_t(micros() - start) > 100000) return false;
  s->USART.CTRLA.reg = SERCOM_USART_CTRLA_SWRST;
  if (!uartSync(s)) return false;
  pinMux(tx, 2); pinMux(rx, 2);
  s->USART.CTRLA.reg = SERCOM_USART_CTRLA_MODE_USART_INT_CLK | SERCOM_USART_CTRLA_DORD | SERCOM_USART_CTRLA_RXPO(rxPad) | SERCOM_USART_CTRLA_TXPO(0);
  s->USART.CTRLB.reg = SERCOM_USART_CTRLB_RXEN | SERCOM_USART_CTRLB_TXEN;
  if (!uartSync(s)) return false;
  s->USART.BAUD.reg = uint16_t(65536ull - (65536ull * 16 * baud) / SystemCoreClock);
  head = tail = 0; errors = dropped = 0;
  s->USART.INTENSET.reg = SERCOM_USART_INTENSET_RXC | SERCOM_USART_INTENSET_ERROR;
  IRQn_Type irqNumber = index == 1 ? SERCOM1_IRQn : SERCOM3_IRQn;
  NVIC_ClearPendingIRQ(irqNumber); NVIC_SetPriority(irqNumber, 2); NVIC_EnableIRQ(irqNumber);
  s->USART.CTRLA.bit.ENABLE = 1;
  active = uartSync(s);
  if (!active) { s->USART.CTRLA.bit.ENABLE=0; NVIC_DisableIRQ(irqNumber); pinInput(tx); pinInput(rx); }
  return active;
}
void RawUart::end() {
  NVIC_DisableIRQ(index == 1 ? SERCOM1_IRQn : SERCOM3_IRQn);
  if (active) { s->USART.INTENCLR.reg = 0xff; s->USART.CTRLA.bit.ENABLE = 0; uartSync(s); }
  active = false; head = tail = 0;
  pinInput(tx); pinInput(rx); // Do not back-power an unpowered module through TX.
}
bool RawUart::write(const char *text) {
  if (!active) return false;
  indicateTx();
  for (; *text; ++text) {
    uint32_t start = micros();
    while (!s->USART.INTFLAG.bit.DRE) if (uint32_t(micros() - start) > 100000) return false;
    s->USART.DATA.reg = uint8_t(*text);
  }
  return true;
}
int RawUart::read() {
  if (tail == head) return -1;
  indicateRx();
  uint8_t value = ring[tail]; tail = (tail + 1) & 2047; return value;
}
void RawUart::discard() { for (unsigned i=0;i<2048 && read()>=0;++i) {} }
void RawUart::irq() {
  if (s->USART.INTFLAG.bit.ERROR) {
    ++errors; s->USART.STATUS.reg = SERCOM_USART_STATUS_FERR | SERCOM_USART_STATUS_PERR | SERCOM_USART_STATUS_BUFOVF;
    s->USART.INTFLAG.reg = SERCOM_USART_INTFLAG_ERROR;
  }
  for (uint8_t i=0;i<32 && s->USART.INTFLAG.bit.RXC;++i) {
    uint8_t value = s->USART.DATA.reg;
    uint16_t next = (head + 1) & 2047;
    if (next == tail) ++dropped;
    else { ring[head] = value; head = next; }
  }
}
extern "C" void SERCOM1_Handler() { lteUart.irq(); }
extern "C" void SERCOM3_Handler() { nrfUart.irq(); }

bool BoardSpi::mappingValid() const {
  return bus ? samePin(sck(),PB(17)) && samePin(mosi(),PB(16)) && samePin(miso(),PB(22))
             : samePin(sck(),PB(13)) && samePin(mosi(),PB(12)) && samePin(miso(),PB(11));
}
static bool spiSync(Sercom *s) {
  uint32_t start = micros();
  while (s->SPI.SYNCBUSY.reg || s->SPI.CTRLA.bit.SWRST) if (uint32_t(micros() - start) > 100000) return false;
  return true;
}
bool BoardSpi::begin() {
  fault = false;
  pinOutput(RADIO_CS, true); pinOutput(SD_CS, true);
  if (!software && !mappingValid()) { fault=true; return false; } // No invalid clock-pad drive.
  if (software) { pinOutput(sck(), false); pinOutput(mosi(), true); pinInput(miso()); return true; }
  PM->APBCMASK.reg |= bus ? PM_APBCMASK_SERCOM5 : PM_APBCMASK_SERCOM4;
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(bus ? SERCOM5_GCLK_ID_CORE : SERCOM4_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  uint32_t start = micros();
  while (GCLK->STATUS.bit.SYNCBUSY) if (uint32_t(micros() - start) > 100000) return false;
  Sercom *s = regs();
  s->SPI.CTRLA.reg = SERCOM_SPI_CTRLA_SWRST;
  if (!spiSync(s)) return false;
  pinMux(sck(), 2); pinMux(mosi(), 2); pinMux(miso(), 3);
  // Both suggested arrangements use MOSI PAD0, SCK PAD1 (DOPO=0).
  s->SPI.CTRLA.reg = SERCOM_SPI_CTRLA_MODE_SPI_MASTER | SERCOM_SPI_CTRLA_DOPO(0) | SERCOM_SPI_CTRLA_DIPO(bus ? 2 : 3);
  s->SPI.CTRLB.reg = SERCOM_SPI_CTRLB_RXEN;
  if (!spiSync(s)) return false;
  speed(hz); s->SPI.CTRLA.bit.ENABLE = 1;
  return spiSync(s);
}
void BoardSpi::speed(uint32_t requested) {
  hz = requested < 100000 ? 100000 : (requested > 1000000 ? 1000000 : requested);
  if (!software && mappingValid()) {
    bool enabled = regs()->SPI.CTRLA.bit.ENABLE;
    if (enabled) { regs()->SPI.CTRLA.bit.ENABLE = 0; if (!spiSync(regs())) { fault = true; return; } }
    uint32_t divisor = (SystemCoreClock + 2 * hz - 1) / (2 * hz);
    regs()->SPI.BAUD.reg = divisor > 256 ? 255 : divisor - 1;
    if (enabled) { regs()->SPI.CTRLA.bit.ENABLE = 1; if (!spiSync(regs())) fault = true; }
  }
}
uint8_t BoardSpi::transfer(uint8_t value) {
  if (fault) return 0xff;
  if (software) {
    uint8_t received = 0;
    uint32_t halfUs = (500000 + hz - 1) / hz;
    for (uint8_t mask = 0x80; mask; mask >>= 1) {
      pinWrite(mosi(), value & mask); delayMicroseconds(halfUs);
      pinWrite(sck(), true); received = (received << 1) | pinRead(miso());
      delayMicroseconds(halfUs); pinWrite(sck(), false);
    }
    return received;
  }
  uint32_t start = micros();
  while (!regs()->SPI.INTFLAG.bit.DRE) if (uint32_t(micros() - start) > 10000) { fault = true; return 0xff; }
  regs()->SPI.DATA.reg = value;
  while (!regs()->SPI.INTFLAG.bit.RXC) if (uint32_t(micros() - start) > 10000) { fault = true; return 0xff; }
  return regs()->SPI.DATA.reg;
}

void SoftI2c::low(Pin p) { pinOutput(p, false); }
void SoftI2c::release(Pin p) { pinInput(p); } // External pullups, never drive HIGH.
void SoftI2c::begin() { release(SDA_PIN); release(SCL_PIN); fault = false; }
bool SoftI2c::clockHigh() {
  release(SCL_PIN);
  uint32_t start = micros();
  while (!pinRead(SCL_PIN)) if (uint32_t(micros() - start) > 2500) { fault = true; return false; }
  delayMicroseconds(5); return true;
}
bool SoftI2c::start() {
  release(SDA_PIN);
  if (!clockHigh() || !pinRead(SDA_PIN)) { fault = true; return false; }
  low(SDA_PIN); delayMicroseconds(5); low(SCL_PIN); return true;
}
void SoftI2c::stop() { low(SDA_PIN); clockHigh(); release(SDA_PIN); delayMicroseconds(5); }
bool SoftI2c::put(uint8_t value) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    if (value & mask) release(SDA_PIN); else low(SDA_PIN);
    delayMicroseconds(5); if (!clockHigh()) return false; low(SCL_PIN);
  }
  release(SDA_PIN); delayMicroseconds(5);
  if (!clockHigh()) return false;
  bool ack = !pinRead(SDA_PIN); low(SCL_PIN); return ack;
}
uint8_t SoftI2c::get(bool ack) {
  uint8_t value = 0; release(SDA_PIN);
  for (uint8_t bit = 0; bit < 8; ++bit) {
    delayMicroseconds(5); if (!clockHigh()) return 0xff;
    value = (value << 1) | pinRead(SDA_PIN); low(SCL_PIN);
  }
  if (ack) low(SDA_PIN); else release(SDA_PIN);
  delayMicroseconds(5); clockHigh(); low(SCL_PIN); release(SDA_PIN); return value;
}
bool SoftI2c::transfer(uint8_t address, const uint8_t *tx, size_t nt, uint8_t *rx, size_t nr) {
  fault = false;
  bool ok = address >= 8 && address <= 0x77 && start();
  if (ok && (nt || !nr)) {
    ok = put(address << 1);
    for (size_t i = 0; ok && i < nt; ++i) ok = put(tx[i]);
  }
  if (ok && nr) {
    if (nt) ok = start();
    if (ok) ok = put((address << 1) | 1);
    for (size_t i = 0; ok && i < nr; ++i) { rx[i] = get(i + 1 < nr); ok = !fault; }
  }
  stop(); return ok && !fault;
}
bool SoftI2c::recover() {
  begin();
  for (uint8_t i = 0; i < 9 && !pinRead(SDA_PIN); ++i) { low(SCL_PIN); delayMicroseconds(5); if (!clockHigh()) break; }
  stop(); return !fault && pinRead(SDA_PIN) && pinRead(SCL_PIN);
}
static bool adcSync() {
  uint32_t start = micros();
  while (ADC->STATUS.bit.SYNCBUSY) if (uint32_t(micros() - start) > 100000) return false;
  return true;
}
int adcReadPin(Pin p) {
  int channel = -1;
  if (!p.port && p.bit >= 2 && p.bit <= 3) channel = p.bit - 2;
  if (!p.port && p.bit >= 4 && p.bit <= 11) channel = p.bit;
  if (p.port && p.bit <= 7) channel = p.bit + 8;
  if (p.port && p.bit >= 8 && p.bit <= 9) channel = p.bit - 6;
  if (channel < 0) return -1;
  struct Cleanup {
    Pin pin;
    ~Cleanup() { ADC->CTRLA.bit.ENABLE=0; adcSync(); pinInput(pin); }
  } cleanup{p};
  // PA08..11 actually AIN16..19. Their bus roles are not allowed by the console.
  if (!p.port && p.bit >= 8) channel += 8;
  ADC->CTRLA.bit.ENABLE = 0;
  if (!adcSync()) return -1;
  pinInput(p); pinMux(p, 1);
  ADC->REFCTRL.reg = ADC_REFCTRL_REFSEL_INTVCC1;
  ADC->CTRLB.reg = ADC_CTRLB_PRESCALER_DIV512 | ADC_CTRLB_RESSEL_12BIT;
  ADC->SAMPCTRL.reg = 63;
  ADC->INPUTCTRL.reg = ADC_INPUTCTRL_GAIN_DIV2 | ADC_INPUTCTRL_MUXNEG_GND | ADC_INPUTCTRL_MUXPOS(channel);
  if (!adcSync()) return -1;
  ADC->CTRLA.bit.ENABLE = 1;
  if (!adcSync()) return -1;
  uint32_t sum = 0;
  for (uint8_t sample = 0; sample < 18; ++sample) {
    ADC->INTFLAG.reg = ADC_INTFLAG_RESRDY;
    ADC->SWTRIG.bit.START = 1;
    if (!adcSync()) return -1;
    uint32_t start = micros();
    while (!ADC->INTFLAG.bit.RESRDY) if (uint32_t(micros() - start) > 100000) return -1;
    uint16_t value = ADC->RESULT.reg;
    if (sample >= 2) sum += value;
  }
  return (sum + 8) / 16;
}
