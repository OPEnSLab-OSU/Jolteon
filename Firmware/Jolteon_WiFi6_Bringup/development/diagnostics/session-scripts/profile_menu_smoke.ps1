$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$fixture=Join-Path $repo 'sketches/Jolteon_Menu_Profile_Test'
if (Test-Path -LiteralPath $fixture) { throw 'Fixture already exists.' }
$null=New-Item -ItemType Directory $fixture
Copy-Item -LiteralPath (Join-Path $repo 'sketches/Jolteon_Blinky/Jolteon_Blinky.ino') -Destination (Join-Path $fixture 'Jolteon_Menu_Profile_Test.ino')
Copy-Item -LiteralPath (Join-Path $repo 'sketches/Jolteon_Blinky/J18BuildOverrides.h') -Destination $fixture
$menu=Join-Path $repo 'Jolteon_Bringup.cmd'
function Run-Profile([string]$Label,[string[]]$Flags,[bool]$ExpectFailure=$false) {
    $log=Join-Path $PSScriptRoot ($Label+'.log')
    & $menu @Flags *> $log
    $code=$LASTEXITCODE
    if (($ExpectFailure -and !$code) -or (!$ExpectFailure -and $code)) {
        Get-Content -LiteralPath $log -Tail 30
        throw "Profile check failed: $Label"
    }
    if (!$ExpectFailure -and (Get-Content -LiteralPath $log -Raw) -notmatch 'hardware was not accessed') {
        throw 'Missing hardware-free completion message.'
    }
    Write-Host "PASS $Label"
}
Run-Profile 'menu-profile-zero' @('-Mode','app','-Sketch','Jolteon_Menu_Profile_Test','-Board','zero','-BuildOnly')
$app=Join-Path $fixture '.j18-build/firmware/firmware.sketch.bin'
$hash=(Get-FileHash -LiteralPath $app).Hash
Run-Profile 'menu-profile-mismatch' @('-Mode','app','-Sketch','Jolteon_Menu_Profile_Test','-Board','jolteon','-BuildOnly') $true
if ((Get-FileHash -LiteralPath $app).Hash -ne $hash) { throw 'Mismatch replaced app.' }
Run-Profile 'menu-profile-jolteon' @('-Mode','app','-Sketch','Jolteon_Menu_Profile_Test','-Board','jolteon','-BuildOnly','-Rebuild')
$receipt=Get-Content -LiteralPath (Join-Path $fixture '.j18-build/sketch-build.json') -Raw | ConvertFrom-Json
if ($receipt.fqbn -ne 'opens:samd:jolteon_j18') { throw 'Wrong compiled profile receipt.' }
Run-Profile 'menu-profile-jolteon-reuse' @('-Mode','app','-Sketch','Jolteon_Menu_Profile_Test','-Board','jolteon','-BuildOnly')
$destination=Join-Path $PSScriptRoot 'profile-menu-fixture'
if (!(Test-Path -LiteralPath $destination)) {
    if ([IO.Path]::GetFullPath($fixture) -ne (Join-Path $repo 'sketches\Jolteon_Menu_Profile_Test')) { throw 'Unsafe fixture path.' }
    Move-Item -LiteralPath $fixture -Destination $destination
}
