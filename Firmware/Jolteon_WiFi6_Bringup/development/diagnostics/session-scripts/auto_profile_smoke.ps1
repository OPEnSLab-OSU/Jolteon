$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$fixture=Join-Path $repo 'sketches/Jolteon_Menu_Auto_Test'
if (Test-Path -LiteralPath $fixture) { throw 'Fixture already exists.' }
$null=New-Item -ItemType Directory $fixture
$source=Join-Path $fixture 'Jolteon_Menu_Auto_Test.ino'
$original=[IO.File]::ReadAllText((Join-Path $repo 'sketches/Jolteon_Blinky/Jolteon_Blinky.ino'))
[IO.File]::WriteAllText($source,$original)
Copy-Item -LiteralPath (Join-Path $repo 'sketches/Jolteon_Blinky/J18BuildOverrides.h') -Destination $fixture
$firmware=Join-Path $fixture '.j18-build/firmware'
$null=New-Item -ItemType Directory -Force $firmware
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'profile-menu-fixture/.j18-build/firmware/firmware.sketch.bin') -Destination $firmware
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'profile-menu-fixture/.j18-build/sketch-build.json') -Destination (Join-Path $fixture '.j18-build/sketch-build.json')
Copy-Item -LiteralPath (Join-Path $repo 'bootloader/firmware/samd21_sam_ba.bin') -Destination $firmware
$bootHash=(Get-FileHash -LiteralPath (Join-Path $firmware 'samd21_sam_ba.bin')).Hash
$menu=Join-Path $repo 'Jolteon_Bringup.cmd'
function Run-Profile([string]$Label,[string]$Board,[string]$Expected,[bool]$Failure=$false) {
    $log=Join-Path $PSScriptRoot ($Label+'.log')
    & $menu -Mode app -Sketch Jolteon_Menu_Auto_Test -Board $Board -BuildOnly *> $log
    $code=$LASTEXITCODE
    $text=Get-Content -LiteralPath $log -Raw
    if (($Failure -and !$code) -or (!$Failure -and $code) -or $text -notmatch $Expected) {
        Get-Content -LiteralPath $log -Tail 25
        throw "Failed automatic rebuild check: $Label"
    }
    Write-Host "PASS $Label"
}
Run-Profile 'auto-profile-zero' 'zero' 'Recompiling automatically'
Run-Profile 'auto-profile-jolteon' 'jolteon' 'Recompiling automatically'
Run-Profile 'auto-profile-reuse' 'jolteon' 'Reusing static sketch'
$app=Join-Path $firmware 'firmware.sketch.bin'
$appHash=(Get-FileHash -LiteralPath $app).Hash
[IO.File]::WriteAllText($source,'this source intentionally does not compile')
Run-Profile 'auto-profile-failure' 'zero' 'Recompiling automatically' $true
if ((Get-FileHash -LiteralPath $app).Hash -ne $appHash) { throw 'Failed compile replaced saved application.' }
if ((Get-FileHash -LiteralPath (Join-Path $firmware 'samd21_sam_ba.bin')).Hash -ne $bootHash) { throw 'Profile change replaced bootloader.' }
$destination=Join-Path $PSScriptRoot 'auto-profile-fixture'
if ([IO.Path]::GetFullPath($fixture) -ne (Join-Path $repo 'sketches\Jolteon_Menu_Auto_Test')) { throw 'Unsafe fixture move.' }
if (Test-Path -LiteralPath $destination) { throw 'Destination already exists.' }
Move-Item -LiteralPath $fixture -Destination $destination
Write-Host 'PASS saved app/bootloader retained on compile failure; no timestamp folders or hardware access.'
