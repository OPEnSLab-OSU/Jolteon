// Generated from board.json by build.py; edit board.json instead.
#pragma once
#define J18_BOARD_REVISION "jolteon-current-2026-10"
#define J18_BOOT_VERSION "0.1.1"
#define J18_BOOT_INDICATOR 0
#define J18_LED 5u // PA05
#define J18_NRF_EN 13u // PA13
#define J18_NRF_RST 14u // PA14
#define J18_NRF_IRQ 15u // PA15
#define J18_NRF_TX 22u // PA22
#define J18_NRF_RX 23u // PA23
#define J18_LTE_TX 16u // PA16
#define J18_LTE_RX 17u // PA17
#define J18_LTE_PWR 62u // PB30
#define J18_LTE_RST 63u // PB31
#define J18_SDA 8u // PA08
#define J18_SCL 9u // PA09
#define J18_MUX_RST 55u // PB23
#define J18_RTC_IRQ 19u // PA19
#define J18_RADIO_RST 27u // PA27
#define J18_RADIO_IRQ 28u // PA28
#define J18_RADIO_CS 45u // PB13
#define J18_SD_CS 46u // PB14
#define J18_SDI 47u // PB15
#define J18_BATTERY 39u // PB07
#define J18_SPI0_SCK 42u // PB10
#define J18_SPI0_MISO 43u // PB11
#define J18_SPI0_MOSI 44u // PB12
#define J18_SPI1_SCK 48u // PB16
#define J18_SPI1_MISO 49u // PB17
#define J18_SPI1_MOSI 54u // PB22
#define J18_SAFE_OUTPUTS(X) X(NRF_EN, 0) X(LTE_PWR, 0) X(LTE_RST, 0) X(RADIO_RST, 0) X(SD_CS, 0) X(RADIO_CS, 0)
#define J18_HIGH_IMPEDANCE(X) X(NRF_RST) X(NRF_TX) X(NRF_RX) X(NRF_IRQ) X(LTE_TX) X(LTE_RX) X(MUX_RST) X(RTC_IRQ) X(RADIO_IRQ) X(SDI) X(BATTERY) X(SDA) X(SCL) X(SPI0_SCK) X(SPI0_MISO) X(SPI0_MOSI) X(SPI1_SCK) X(SPI1_MISO) X(SPI1_MOSI)
#ifdef __cplusplus
extern "C" {
#endif
void jolteon_board_early_init(void);
#ifdef __cplusplus
}
#endif
