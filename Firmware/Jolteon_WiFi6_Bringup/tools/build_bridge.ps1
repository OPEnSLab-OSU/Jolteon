param(
    [string]$SdkRoot = 'C:\ncs\v3.3.1',
    [string]$ToolchainRoot = 'C:\ncs\toolchains\936afb6332',
    [switch]$Pristine
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$app = Join-Path $repo 'bridge'
$build = Join-Path $repo '.build\bridge'
[void][IO.Directory]::CreateDirectory((Split-Path -Parent $build))
$environment = Join-Path $ToolchainRoot 'environment.json'
if (!(Test-Path -LiteralPath $environment)) { throw 'Nordic environment.json not found. Pass -ToolchainRoot.' }
if (!(Test-Path -LiteralPath (Join-Path $SdkRoot 'zephyr'))) { throw 'Nordic SDK not found. Pass -SdkRoot.' }
foreach ($entry in (Get-Content -LiteralPath $environment -Raw | ConvertFrom-Json).env_vars) {
    if ($entry.type -eq 'relative_paths') {
        $value = ($entry.values | ForEach-Object { Join-Path $ToolchainRoot $_ }) -join ';'
        if ($entry.existing_value_treatment -eq 'prepend_to') {
            $value += ';' + [Environment]::GetEnvironmentVariable($entry.key, 'Process')
        }
    } else { $value = $entry.value }
    [Environment]::SetEnvironmentVariable($entry.key, $value, 'Process')
}
$env:ZEPHYR_BASE = Join-Path $SdkRoot 'zephyr'
$python = Join-Path $ToolchainRoot 'opt\bin\python.exe'
$receiptTool = Join-Path $PSScriptRoot 'build_receipt.py'
$receiptArgs = @('--source',$app,'--build',$build,'--helper',$PSCommandPath,'--sdk-root',$SdkRoot,'--toolchain-root',$ToolchainRoot)
$arguments = @('-m','west','build','--sysbuild','-b','nrf7002dk/nrf5340/cpuapp','-d',$build,$app)
if ($Pristine) { $arguments += @('--pristine','always') }
$arguments += @('--',('-DEXTRA_DTC_OVERLAY_FILE=' + (Join-Path $app 'boards\jolteon_uart.overlay').Replace('\','/')))
Write-Host 'Compiling WT02E40E for Jolteon (RX P1.05); no flashing.'
# One SDK build owns this build directory at a time; retain an empty lock file.
$buildLock = [IO.File]::Open((Join-Path $repo '.build\bridge-build.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
$locationPushed = $false
try {
    Push-Location $SdkRoot
    $locationPushed = $true
    & $python $receiptTool begin @receiptArgs
    if ($LASTEXITCODE) { throw 'Could not record bridge build inputs.' }
    & $python @arguments
    if ($LASTEXITCODE) { throw "Bridge build failed: $LASTEXITCODE" }
    & $python $receiptTool finish @receiptArgs
    if ($LASTEXITCODE) { throw 'Bridge input/output consistency check failed.' }
    Write-Host "CPUAPP: $build\merged.hex"
    Write-Host "CPUNET: $build\merged_CPUNET.hex"
    Write-Host 'Use sysbuild west flash for both cores; tools/package_bridge.py creates a checked dual-core HEX.'
} finally {
    if ($locationPushed) { Pop-Location }
    $buildLock.Dispose()
}
