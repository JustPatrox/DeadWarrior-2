param([string]$Configuration = 'Release', [switch]$CheckTutorial, [switch]$CheckArchidon,
    [switch]$CheckMagic, [switch]$CheckRebels, [switch]$CheckAudio, [switch]$CheckGiants)
$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Install the Visual Studio x64 C++ workload and CMake tools.' }
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$major = (& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion).Split('.')[0]
$generators = @{ '17'='Visual Studio 17 2022'; '18'='Visual Studio 18 2026' }
& $cmake -S . -B build/windows -G $generators[$major] -A x64
if ($LASTEXITCODE) {throw 'CMake configure failed'}
& $cmake --build build/windows --config $Configuration --parallel
if ($LASTEXITCODE) {throw 'C++ build failed'}
& $cmake --install build/windows --config $Configuration --prefix out/windows-x64
if ($LASTEXITCODE) {throw 'CMake install failed'}
if ($CheckTutorial) {
    & $cmake --build build/windows --config $Configuration --target tutorial_probe --parallel
    if ($LASTEXITCODE) {throw 'Tutorial test build failed'}
    & (Join-Path (Get-Location) "build/windows/$Configuration/tutorial_probe.exe") assets
    if ($LASTEXITCODE) {throw 'Tutorial simulation failed'}
}
if ($CheckArchidon) {
    & $cmake --build build/windows --config $Configuration --target archidon_probe --parallel
    if ($LASTEXITCODE) {throw 'Archidon test build failed'}
    & (Join-Path (Get-Location) "build/windows/$Configuration/archidon_probe.exe") assets
    if ($LASTEXITCODE) {throw 'Archidon simulation failed'}
}
foreach ($check in @(@($CheckMagic,'magic_probe'),@($CheckRebels,'rebels_probe'),@($CheckAudio,'audio_probe'),@($CheckGiants,'giants_probe'))) {
    if (!$check[0]) {continue}
    $target = $check[1]
    & $cmake --build build/windows --config $Configuration --target $target --parallel
    if ($LASTEXITCODE) {throw "$target build failed"}
    & (Join-Path (Get-Location) "build/windows/$Configuration/$target.exe") assets
    if ($LASTEXITCODE) {throw "$target simulation failed"}
}
