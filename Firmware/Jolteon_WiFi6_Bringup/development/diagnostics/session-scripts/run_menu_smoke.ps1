$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$fixture=Join-Path $repo 'sketches/Jolteon_Menu_Test'
$firmware=Join-Path $fixture '.j18-build/firmware'
$null=New-Item -ItemType Directory -Force $firmware
$menu=Join-Path $repo 'Jolteon_Bringup.cmd'
function Run-Menu([string]$Label,[string[]]$Flags,[bool]$ExpectFailure=$false) {
    $log=Join-Path $PSScriptRoot ('menu-'+$Label+'.log')
    & $menu @Flags *> $log
    $code=$LASTEXITCODE
    $text=Get-Content -LiteralPath $log -Raw
    if ($ExpectFailure) {
        if (!$code) { throw "Expected failure missing: $Label" }
    } elseif ($code -or $text -notmatch 'hardware was not accessed') {
        Get-Content -LiteralPath $log -Tail 30
        throw "Menu failed: $Label (exit $code)"
    }
    Write-Host "PASS $Label (expectedFailure=$ExpectFailure)"
}
Run-Menu 'boot-only-without-app' @('-Mode','boot','-Sketch','Jolteon_Menu_Test','-Bootloader','jolteon','-BuildOnly')
$app=Join-Path $firmware 'firmware.sketch.bin'
$boot=Join-Path $firmware 'samd21_sam_ba.bin'
if (Test-Path -LiteralPath $app) { throw 'Boot-only unexpectedly built an application.' }
Copy-Item -LiteralPath (Join-Path $repo 'sketches/SAMD21J18_Blinky/.j18-build/firmware/firmware.sketch.bin') -Destination $app
# Remove only the known fixture bootloader, proving app-only does not require one.
if (![IO.Path]::GetFullPath($boot).StartsWith([IO.Path]::GetFullPath($fixture)+'\',[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid fixture delete target.' }
Remove-Item -LiteralPath $boot
Run-Menu 'app-only-without-boot' @('-Mode','app','-Sketch','Jolteon_Menu_Test','-BuildOnly')
if (Test-Path -LiteralPath $boot) { throw 'Sketch-only unexpectedly created a bootloader.' }
Run-Menu 'combined-official' @('-Mode','combined','-Sketch','Jolteon_Menu_Test','-BuildOnly')
[byte[]]$combined=[IO.File]::ReadAllBytes((Join-Path $firmware 'firmware.with_bootloader.bin'))
[byte[]]$appBytes=[IO.File]::ReadAllBytes($app)
[byte[]]$bootBytes=[IO.File]::ReadAllBytes($boot)
if ($combined.Length -ne (8192+$appBytes.Length)) { throw 'Wrong combined length.' }
for ($i=0;$i -lt $bootBytes.Length;$i++) { if ($combined[$i] -ne $bootBytes[$i]) { throw 'Wrong boot bytes.' } }
for ($i=$bootBytes.Length;$i -lt 8192;$i++) { if ($combined[$i] -ne 255) { throw 'Wrong padding.' } }
for ($i=0;$i -lt $appBytes.Length;$i++) { if ($combined[8192+$i] -ne $appBytes[$i]) { throw 'Wrong app offset.' } }
Run-Menu 'separate-github' @('-Mode','separate','-Sketch','Jolteon_Menu_Test','-Bootloader','github','-BuildOnly')
Copy-Item -LiteralPath (Join-Path $repo 'bootloader/firmware/samd21_sam_ba.bin') -Destination $boot -Force
$bootHash=(Get-FileHash -LiteralPath $boot -Algorithm SHA256).Hash
$oldAppHash=(Get-FileHash -LiteralPath $app -Algorithm SHA256).Hash
Run-Menu 'manual-boot-swap-retained' @('-Mode','separate','-Sketch','Jolteon_Menu_Test','-Bootloader','current','-BuildOnly')
if ((Get-FileHash -LiteralPath $boot -Algorithm SHA256).Hash -ne $bootHash) { throw 'Manual bootloader swap was overwritten.' }
Run-Menu 'failed-rebuild-retains-app' @('-Mode','app','-Sketch','Jolteon_Menu_Test','-BuildOnly','-Rebuild') $true
if ((Get-FileHash -LiteralPath $app -Algorithm SHA256).Hash -ne $oldAppHash) { throw 'Failed rebuild replaced the stored image.' }
Set-Content -LiteralPath (Join-Path $fixture 'Jolteon_Menu_Test.ino') -Value 'void setup() {} void loop() { delay(20); }' -Encoding utf8
Run-Menu 'explicit-rebuild' @('-Mode','app','-Sketch','Jolteon_Menu_Test','-BuildOnly','-Rebuild')
if ((Get-FileHash -LiteralPath $boot -Algorithm SHA256).Hash -ne $bootHash) { throw 'Rebuild replaced the selected bootloader.' }
$dates=@(Get-ChildItem -LiteralPath (Join-Path $fixture '.j18-build') -Directory | Where-Object { $_.Name -match '^20[0-9]{6}-' })
if ($dates.Count) { throw 'Timestamp folders were created.' }
Run-Menu 'static-reuse' @('-Mode','app','-Sketch','Jolteon_Menu_Test','-BuildOnly')
$source=(Resolve-Path -LiteralPath $fixture).Path
$destination=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'menu-smoke-fixture'))
if (!$source.StartsWith($repo+'\',[StringComparison]::OrdinalIgnoreCase) -or !$destination.StartsWith($repo+'\',[StringComparison]::OrdinalIgnoreCase) -or (Test-Path -LiteralPath $destination)) { throw 'Unsafe fixture archive target.' }
Move-Item -LiteralPath $source -Destination $destination
Write-Host 'PASS all menu modes, combined layout, static reuse, manual swaps and explicit rebuilds; no hardware accessed.'
