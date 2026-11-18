#pragma once
#include <Arduino.h>

struct Pin { uint8_t port, bit; };
constexpr Pin PA(uint8_t bit) { return {0, bit}; }
constexpr Pin PB(uint8_t bit) { return {1, bit}; }

// Existing PCB placeholders are deliberately retained. Hardware pin-map checks
// FAIL on both current SPI sets; they must not silently use a software workaround.
#ifndef J18_REVISED_SPI0
#define J18_REVISED_SPI0 0
#endif
#ifndef J18_REVISED_SPI1
#define J18_REVISED_SPI1 0
#endif
#ifndef J18_REVISED_SDI12
#define J18_REVISED_SDI12 0
#endif
#ifndef J18_STOCK_BOOTLOADER_PINMAP
#define J18_STOCK_BOOTLOADER_PINMAP 0
#endif
#ifndef J18_ENABLE_EXTRA_LEDS
#define J18_ENABLE_EXTRA_LEDS 0
#endif
#ifndef J18_NRF_POWER_PATH_VERIFIED
#define J18_NRF_POWER_PATH_VERIFIED 0
#endif
// Set true AFTER correcting/confirming U10/R20 and the nRF UART RX=P1.05.
// Until then, enable nRF explicitly with 'nrf on' using the USB console.
constexpr bool NRF_POWER_PATH_VERIFIED = J18_NRF_POWER_PATH_VERIFIED;
constexpr bool AUTO_WIRELESS_FALLBACK = true;
constexpr bool RUN_BASELINE_ON_BOOT = true; // Once after 5 s; no RF TX or SD writes.
constexpr uint32_t TELEMETRY_ACK_MS = 2500, TELEMETRY_RETRY_MS = 2000;
constexpr uint32_t PEER_CHALLENGE_MS = 60000;
constexpr uint16_t TELEMETRY_QUEUE_BYTES = 6144;
constexpr uint8_t TELEMETRY_CHUNK_BYTES = 64;
constexpr Pin HEARTBEAT = PA(5); // Same pin as samd_blinky_test; external LED needed.
// PA05 also displays test/traffic patterns. Extra LEDs require physical LEDs
// with series resistors; leave disabled if these header pins serve another load.
constexpr bool ENABLE_EXTRA_LEDS = J18_ENABLE_EXTRA_LEDS;
constexpr bool LED_ACTIVE_HIGH = true;
constexpr Pin TX_LED = J18_STOCK_BOOTLOADER_PINMAP ? PB(4) : PA(20);
constexpr Pin RX_LED = PA(21);
constexpr Pin NRF_EN = PA(13), NRF_RST = PA(14), NRF_IRQ = PA(15);
constexpr Pin NRF_TX = PA(22), NRF_RX = PA(23); // SERCOM3 C PAD0/PAD1.
// Schematic: PA22 -> nRF P1.05. Upstream firmware RX=P1.06 MUST be corrected.
constexpr Pin LTE_TX = PA(16); // SERCOM1 C PAD0.
#if J18_STOCK_BOOTLOADER_PINMAP
constexpr Pin LTE_RX = PA(18), RADIO_RST = PA(20); // Future PCB only.
constexpr uint8_t LTE_RX_PAD = 2;
#else
constexpr Pin LTE_RX = PA(17), RADIO_RST = PA(27); // Current PCB.
constexpr uint8_t LTE_RX_PAD = 1;
#endif
constexpr Pin LTE_PWR = PB(30), LTE_RST = PB(31); // HIGH asserts MOSFET gate.
constexpr Pin SDA_PIN = PA(8), SCL_PIN = PA(9), MUX_RST = PB(23);
constexpr Pin RTC_IRQ = PA(19), RADIO_IRQ = PA(28);
constexpr Pin SD_CS = PB(14), SDI_PIN = PB(15), BATTERY_PIN = PB(7);
constexpr Pin SDI_DIR = PA(12), SDI_POWER_EN = PA(18); // Revised 1T45 circuit only.
constexpr bool SDI_HAS_POWER_ENABLE = true;
constexpr uint32_t SDI_POWER_SETTLE_MS = 1000;
static_assert(!J18_REVISED_SDI12 || !SDI_HAS_POWER_ENABLE || LTE_RX.port!=SDI_POWER_EN.port || LTE_RX.bit!=SDI_POWER_EN.bit,
              "SDI power EN and stock-compatible LTE RX both use PA18; choose another power-enable pin in config.h.");
#if J18_REVISED_SPI0
// Proposed PCB swap: PB10 <-> PB13. PB11 mux D; PB12/PB13 mux C.
constexpr Pin SPI0_SCK = PB(13), SPI0_MISO = PB(11), SPI0_MOSI = PB(12), RADIO_CS = PB(10);
#else
constexpr Pin SPI0_SCK = PB(10), SPI0_MISO = PB(11), SPI0_MOSI = PB(12), RADIO_CS = PB(13);
#endif
#if J18_REVISED_SPI1
// Proposed three-signal rotation. PB16/PB17 mux C; PB22 mux D.
constexpr Pin SPI1_SCK = PB(17), SPI1_MISO = PB(22), SPI1_MOSI = PB(16);
#else
constexpr Pin SPI1_SCK = PB(16), SPI1_MISO = PB(17), SPI1_MOSI = PB(22);
#endif
constexpr uint32_t NRF_BAUD = 115200, LTE_BAUD = 115200;
constexpr char LTE_MODEL_PREFIX[]="SARA-R410";
// The Jolteon Arduino board profile supplies the generated custom identity.
// Zero builds retain the official core 1.8.14 reference unless both custom
// macros are explicitly supplied (e.g. via a forced-include identity header).
// Only actual image bytes are checked, not unused padding.
#if defined(J18_CUSTOM_BOOTLOADER_BYTES) && defined(J18_CUSTOM_BOOTLOADER_CRC32)
constexpr uint32_t EXPECTED_BOOTLOADER_BYTES=J18_CUSTOM_BOOTLOADER_BYTES;
constexpr uint32_t EXPECTED_BOOTLOADER_CRC32=J18_CUSTOM_BOOTLOADER_CRC32;
#else
constexpr uint32_t EXPECTED_BOOTLOADER_BYTES=6504;
constexpr uint32_t EXPECTED_BOOTLOADER_CRC32=0x032dc51eul;
#endif
constexpr uint32_t NRF_POWER_SETTLE_MS=1500,NRF_BOOT_WAIT_MS=2500;
static_assert(TELEMETRY_CHUNK_BYTES>0 && TELEMETRY_CHUNK_BYTES<=64,"Report chunks must fit the bridge shell command buffer.");
static_assert(TELEMETRY_QUEUE_BYTES>=256 && TELEMETRY_QUEUE_BYTES<32768,"Report queue must fit indices and SAMD21 SRAM.");
constexpr char NRF_PREFIX[] = "wt "; // Zephyr UART shell, not bare BLE commands.
constexpr uint8_t MUX_ADDRESS = 0x70, RTC_ADDRESS = 0x68;
constexpr float ADC_VDD_VOLTS = 3.300f; // Calibrate to measured VDDANA.
constexpr float BATTERY_R_TOP = 348000.0f, BATTERY_R_BOTTOM = 750000.0f;
constexpr float BATTERY_CALIBRATION = 1.000f;
// Original TXB0102/OE circuit remains BLOCKED. J18_REVISED_SDI12 enables a
// direction-controlled 1T45 bench driver, not a claim of electrical compliance.

// Reject overlapping functions before driving anything. GPIOs on the current
// headers remain usable when their optional indicator/SDI functions are disabled.
constexpr bool pinSame(Pin a,Pin b) { return a.port==b.port && a.bit==b.bit; }
constexpr bool validPackagePin(Pin p) {
  return p.port==0 ? p.bit<32 && p.bit!=26 && p.bit!=29
                  : p.port==1 && (p.bit<=17 || p.bit==22 || p.bit==23 || p.bit==30 || p.bit==31);
}
constexpr bool pinListed(Pin p,const Pin *list,size_t count) {
  return count ? pinSame(p,*list) || pinListed(p,list+1,count-1) : false;
}
constexpr bool uniquePins(const Pin *list,size_t count) {
  return count ? validPackagePin(*list) && !pinListed(*list,list+1,count-1) && uniquePins(list+1,count-1) : true;
}
constexpr Pin CORE_PINS[]={HEARTBEAT,NRF_EN,NRF_RST,NRF_IRQ,NRF_TX,NRF_RX,LTE_TX,LTE_RX,LTE_PWR,LTE_RST,
  SDA_PIN,SCL_PIN,MUX_RST,RTC_IRQ,RADIO_RST,RADIO_IRQ,SD_CS,SDI_PIN,BATTERY_PIN,
  SPI0_SCK,SPI0_MISO,SPI0_MOSI,RADIO_CS,SPI1_SCK,SPI1_MISO,SPI1_MOSI,PA(0),PA(1),PA(24),PA(25),PA(30),PA(31)};
constexpr size_t CORE_PIN_COUNT=sizeof(CORE_PINS)/sizeof(CORE_PINS[0]);
static_assert(uniquePins(CORE_PINS,CORE_PIN_COUNT),"Invalid/overlapping config.h pins, including USB, SWD and crystal pins.");
static_assert(!ENABLE_EXTRA_LEDS || (validPackagePin(TX_LED) && validPackagePin(RX_LED) && !pinSame(TX_LED,RX_LED) && !pinListed(TX_LED,CORE_PINS,CORE_PIN_COUNT) && !pinListed(RX_LED,CORE_PINS,CORE_PIN_COUNT)),"Extra LEDs overlap reserved pins; revise config.h.");
static_assert(!J18_REVISED_SDI12 || (validPackagePin(SDI_DIR) && !pinListed(SDI_DIR,CORE_PINS,CORE_PIN_COUNT)),"SDI DIR overlaps a reserved pin.");
static_assert(!J18_REVISED_SDI12 || !SDI_HAS_POWER_ENABLE || (validPackagePin(SDI_POWER_EN) && !pinSame(SDI_DIR,SDI_POWER_EN) && !pinListed(SDI_POWER_EN,CORE_PINS,CORE_PIN_COUNT)),"SDI power enable overlaps a reserved pin.");
static_assert(!J18_REVISED_SDI12 || !ENABLE_EXTRA_LEDS || (!pinSame(SDI_DIR,TX_LED) && !pinSame(SDI_DIR,RX_LED) && (!SDI_HAS_POWER_ENABLE || (!pinSame(SDI_POWER_EN,TX_LED) && !pinSame(SDI_POWER_EN,RX_LED)))),"SDI controls overlap extra LEDs.");
