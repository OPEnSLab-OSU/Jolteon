# Generated local Jolteon board profile

Requires arduino:samd@1.8.14. See the repository bootloader/README.md.
Native USB upload uses the inherited bossac recipe. Flash bootloader via the provided OpenOCD configs.
Hardware SPI routes available: SPI0=False, SPI1=False. Physical aliases differ from Zero digital pin numbers.
PWM pin metadata is not implemented in this initial variant; use digital GPIO or a reviewed timer driver.
