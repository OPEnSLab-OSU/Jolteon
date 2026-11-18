$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$batch=Join-Path $repo 'tools\Windows_J18_Tools\build_and_flash_j18_separate_bootloader.bat'
$payload=[IO.File]::ReadAllText($batch)
$marker='# J18_POWERSHELL_PAYLOAD'
$payload=$payload.Substring($payload.LastIndexOf($marker)+$marker.Length)
foreach ($source in @($payload,[IO.File]::ReadAllText((Join-Path $repo 'tools/jolteon_menu.ps1')))) {
    $tokens=$null; $errors=$null
    $ast=[Management.Automation.Language.Parser]::ParseInput($source,[ref]$tokens,[ref]$errors)
    if ($errors.Count) { throw ($errors | Out-String) }
    foreach ($node in $ast.EndBlock.Statements) {
        if ($node -is [Management.Automation.Language.FunctionDefinitionAst]) {
            $rootLiteral="'"+(Join-Path $repo 'tools').Replace("'","''")+"'"
            . ([ScriptBlock]::Create($node.Extent.Text.Replace('$PSScriptRoot',$rootLiteral)))
        }
    }
}
$ProbeMap='generic'; $ProbeAppOffset='0x2000'
$targetSketch=Join-Path $repo '.build/config-menu-smoke/Config_Smoke'
Invoke-SwdProbe $targetSketch $true $false $false $true $false
Invoke-SwdProbe $targetSketch $true $true $true $true $true
$config=Get-Content -LiteralPath (Join-Path $targetSketch '.j18-build/swd/swd_probe.cfg') -Raw
if ($config -notmatch 'connect_assert_srst') { throw 'Missing wired reset configuration.' }
if ($config -notmatch 'set SWD_SCAN_BYTES [1-9][0-9]*') { throw 'Missing image catalog scan range.' }
Write-Output 'PASS: Windows full wired probe and image-identification configurations checked; no hardware accessed.'
