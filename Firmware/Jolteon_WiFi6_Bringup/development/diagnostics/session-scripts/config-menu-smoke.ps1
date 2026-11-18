$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$batch=Join-Path $repo 'tools\Windows_J18_Tools\build_and_flash_j18_separate_bootloader.bat'
$toolFolder=Split-Path -Parent $batch
$toolStore=Join-Path $env:LOCALAPPDATA 'OPEnS\J18Tools'
$scriptRoot=$repo
$payload=[IO.File]::ReadAllText($batch)
$marker='# J18_POWERSHELL_PAYLOAD'
$payload=$payload.Substring($payload.LastIndexOf($marker)+$marker.Length)
foreach ($source in @($payload,[IO.File]::ReadAllText((Join-Path $repo 'tools/jolteon_menu.ps1')))) {
    $tokens=$null; $errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseInput($source,[ref]$tokens,[ref]$errors)
    if ($errors.Count) { throw ($errors | Out-String) }
    foreach ($node in $ast.EndBlock.Statements) {
        if ($node -is [Management.Automation.Language.FunctionDefinitionAst]) { . ([ScriptBlock]::Create($node.Extent.Text)) }
    }
}
$Board='target'
$targetSketch=Join-Path $repo '.build/config-menu-smoke/Config_Smoke'
Invoke-MenuAction 'combined' $targetSketch 'github' '' $true $true
$receipt=Get-Content -LiteralPath (Join-Path $targetSketch '.j18-build/sketch-build.json') -Raw | ConvertFrom-Json
if (!$receipt.profile_signature -or $receipt.fqbn -ne 'opens:samd:jolteon_j18') { throw 'Missing configured-profile identity in Windows receipt.' }
$before=Get-Sha256 (Join-Path $targetSketch '.j18-build/firmware/firmware.sketch.bin')
Invoke-MenuAction 'app' $targetSketch 'github' '' $true $false
if ((Get-Sha256 (Join-Path $targetSketch '.j18-build/firmware/firmware.sketch.bin')) -ne $before) { throw 'Static reuse changed app.' }
Write-Host 'PASS: Windows configured-profile compilation, combined image, and static reuse; no hardware accessed.'
