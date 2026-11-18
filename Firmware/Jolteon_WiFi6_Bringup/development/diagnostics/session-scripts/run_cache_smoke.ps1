$ErrorActionPreference='Stop'
$fixture=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'cache-smoke-path.txt') -Raw
$batch=Join-Path $fixture 'Windows_J18_Tools/build_and_flash_j18_separate_bootloader.bat'
$env:J18_NO_PAUSE='1'
function Run-Smoke([string]$Name,[string[]]$Flags,[bool]$ExpectReuse) {
    $log=Join-Path $fixture ($Name+'.log')
    & $batch @Flags *> $log
    if ($LASTEXITCODE) { Get-Content -LiteralPath $log -Tail 35; throw "Launcher failed: $Name" }
    $text=Get-Content -LiteralPath $log -Raw
    if ($ExpectReuse -ne $text.Contains('Sketch unchanged; skipping compilation.')) { throw "Unexpected cache result: $Name; see $log" }
    if ($text -match 'WARNING: interface/stlink-dap.cfg|Warn : DEPRECATED!') { throw "Deprecated ST-LINK selection: $Name" }
    if ($text -notmatch 'Both OpenOCD phases checked; hardware was not accessed') { throw "Missing check-only result: $Name" }
    Write-Host "PASS $Name; reused=$ExpectReuse; no deprecated driver/transport warnings."
}
Run-Smoke 'first-build' @('--build-only') $false
Run-Smoke 'second-reuses' @('--build-only') $true
Run-Smoke 'forced-rebuild' @('--build-only','--rebuild') $false
Add-Content -LiteralPath (Join-Path $fixture 'CacheSmoke.ino') -Value '// source change must invalidate reuse'
Run-Smoke 'changed-source-rebuilds' @('--build-only') $false
Write-Host "All real launcher cache checks passed. Logs: $fixture"
