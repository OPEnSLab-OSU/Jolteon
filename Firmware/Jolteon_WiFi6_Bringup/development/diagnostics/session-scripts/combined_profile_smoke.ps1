$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$fixture=Join-Path $repo 'sketches/Jolteon_Combined_Mode_Test'
$dest=Join-Path $PSScriptRoot 'combined-mode-fixture'
if ((Test-Path -LiteralPath $fixture) -or (Test-Path -LiteralPath $dest)) { throw 'Fixture already exists.' }
$null=New-Item -ItemType Directory (Join-Path $fixture '.j18-build/firmware') -Force
Copy-Item -LiteralPath (Join-Path $repo 'sketches/Jolteon_Blinky/Jolteon_Blinky.ino') -Destination (Join-Path $fixture 'Jolteon_Combined_Mode_Test.ino')
$firmware=Join-Path $fixture '.j18-build/firmware'
try {
    foreach ($board in @('zero','jolteon')) {
        $app=if ($board -eq 'zero') { 'releases/samd/zero-official/Jolteon_Feature_Verification/firmware.sketch.bin' } else { 'releases/samd/jolteon-jolteon/Jolteon_Blinky/firmware.sketch.bin' }
        Copy-Item -LiteralPath (Join-Path $repo $app) -Destination (Join-Path $firmware 'firmware.sketch.bin') -Force
        $fqbn=if ($board -eq 'zero') { 'arduino:samd:arduino_zero_native' } else { 'opens:samd:jolteon_j18' }
        @{schema=2;completed=$true;fqbn=$fqbn} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $fixture '.j18-build/sketch-build.json') -Encoding UTF8
        $log=Join-Path $PSScriptRoot ('combined-profile-'+$board+'.log')
        & (Join-Path $repo 'Jolteon_Bringup.cmd') -Mode combined -Sketch Jolteon_Combined_Mode_Test -Board $board -Bootloader local -BuildOnly *> $log
        if ($LASTEXITCODE) { Get-Content -LiteralPath $log -Tail 20; throw "Combined $board failed." }
        $expectedBoot=if ($board -eq 'zero') { 'tools/RPi_J18_Tools/samd21_sam_ba.bin' } else { 'bootloader/firmware/samd21_sam_ba.bin' }
        if ((Get-FileHash -LiteralPath (Join-Path $repo $expectedBoot)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $firmware 'samd21_sam_ba.bin')).Hash) { throw 'Wrong profile-selected loader.' }
        [byte[]]$combined=[IO.File]::ReadAllBytes((Join-Path $firmware 'firmware.with_bootloader.bin'))
        [byte[]]$boot=[IO.File]::ReadAllBytes((Join-Path $firmware 'samd21_sam_ba.bin'))
        [byte[]]$appBytes=[IO.File]::ReadAllBytes((Join-Path $firmware 'firmware.sketch.bin'))
        if ($combined.Length -ne 8192+$appBytes.Length) { throw 'Wrong combined length.' }
        for ($i=0;$i -lt $boot.Length;$i++) { if ($combined[$i] -ne $boot[$i]) { throw 'Wrong loader bytes.' } }
        for ($i=$boot.Length;$i -lt 8192;$i++) { if ($combined[$i] -ne 255) { throw 'Wrong padding.' } }
        for ($i=0;$i -lt $appBytes.Length;$i++) { if ($combined[$i+8192] -ne $appBytes[$i]) { throw 'Wrong app offset/content.' } }
        Write-Host "PASS actual Windows combined $board mode ignores B and uses profile loader; config checked, no hardware access."
    }
} finally {
    if ([IO.Path]::GetFullPath($fixture) -ne (Join-Path $repo 'sketches\Jolteon_Combined_Mode_Test')) { throw 'Unsafe fixture move.' }
    Move-Item -LiteralPath $fixture -Destination $dest
}
