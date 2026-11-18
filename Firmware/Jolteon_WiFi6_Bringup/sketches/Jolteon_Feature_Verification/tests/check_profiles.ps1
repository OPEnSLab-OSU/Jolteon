param([string]$ArduinoCli)
$ErrorActionPreference='Stop'
if (!$ArduinoCli) {
  $cliCommand=Get-Command arduino-cli -ErrorAction SilentlyContinue
  if ($cliCommand) { $ArduinoCli=$cliCommand.Source }
  else { $ArduinoCli=Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe' }
}
if (!(Test-Path -LiteralPath $ArduinoCli)) { throw 'Arduino CLI not found. Pass -ArduinoCli or add it to PATH.' }
$taskSketch=Split-Path -Parent $PSScriptRoot
$taskHeader=(Join-Path $taskSketch 'J18BuildOverrides.h').Replace('\','/')
$taskProfiles=@(
  @{Name='revised-spi-sdi-leds'; Flags='-DJ18_REVISED_SPI0=1 -DJ18_REVISED_SPI1=1 -DJ18_REVISED_SDI12=1 -DJ18_ENABLE_EXTRA_LEDS=1 -DJ18_NRF_POWER_PATH_VERIFIED=1'},
  @{Name='stock-compatible-leds'; Flags='-DJ18_REVISED_SPI0=1 -DJ18_REVISED_SPI1=1 -DJ18_STOCK_BOOTLOADER_PINMAP=1 -DJ18_ENABLE_EXTRA_LEDS=1 -DJ18_NRF_POWER_PATH_VERIFIED=1'}
)
foreach ($taskProfile in $taskProfiles) {
  Write-Host ('Compiling profile: '+$taskProfile.Name)
  $taskFlags='-include "'+$taskHeader+'" '+$taskProfile.Flags
  & $ArduinoCli compile --fqbn arduino:samd:arduino_zero_native --build-path (Join-Path $taskSketch ('.j18-build\profile-checks\'+$taskProfile.Name)) --build-property ('compiler.cpp.extra_flags='+$taskFlags) $taskSketch
  if ($LASTEXITCODE -ne 0) { throw ('Profile compilation failed: '+$taskProfile.Name) }
}
Write-Host 'PASS: optional profiles compile. No hardware accessed; default config.h was not modified.'
