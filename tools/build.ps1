param([string]$Generator='', [string]$WindowsSdkVersion='', [string]$CMakeExecutable='cmake')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path -Parent $PSScriptRoot
Push-Location -LiteralPath $projectRoot
try {
& (Join-Path $PSScriptRoot 'setup-dependencies.ps1')
if (!$Generator) {
 $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
 if (Test-Path -LiteralPath $vswhere) {
  $version=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
  if ($version -like '17.*') { $Generator='Visual Studio 17 2022' }
  elseif ($version -like '16.*') { $Generator='Visual Studio 16 2019' }
 }
 if (!$Generator) { throw 'Pass -Generator with an installed Visual Studio C++ generator.' }
}
$cmake=(Get-Command $CMakeExecutable -ErrorAction Stop).Source
$sdkArguments=@()
if ($WindowsSdkVersion) { $sdkArguments+=('-DCMAKE_SYSTEM_VERSION='+$WindowsSdkVersion) }
$bankPath=(Join-Path $projectRoot 'build/vst3-studio/generated/driver-bank.bin').Replace('\','/')
Set-Content -LiteralPath build/vst3-studio/generated/studio.rc -Value ('102 RCDATA "'+$bankPath+'"')
& $cmake -S src/mm-studio-vst-051 -B build/vst3-studio-051/cmake-release -G $Generator -A x64 @sdkArguments -DSMTG_CREATE_BUNDLE_FOR_WINDOWS=ON
if ($LASTEXITCODE) { throw 'Configuration failed' }
& $cmake --build build/vst3-studio-051/cmake-release --config Release --target MMStudio051 held_program_tests mm5_settings_tests mm5_live_tests mm5_performance_tests mm6_driver_tests game_bank_tests mm5_library_tests browser_tests validator --parallel 4
if ($LASTEXITCODE) { throw 'Build failed' }
& 'build/vst3-studio-051/cmake-release/Release/mm5_settings_tests.exe' build/vst3-studio-051/settings-tests | Tee-Object build/vst3-studio-051/settings-tests.txt
if ($LASTEXITCODE) { throw 'Settings MIDI tests failed' }
& 'build/vst3-studio-051/cmake-release/Release/mm5_live_tests.exe' | Tee-Object build/vst3-studio-051/live-tests.txt
if ($LASTEXITCODE) { throw 'Live MIDI / percussion pad tests failed' }
& 'build/vst3-studio-051/cmake-release/Release/mm5_performance_tests.exe' build/vst3-studio/generated/driver-bank.bin | Tee-Object build/vst3-studio-051/performance-tests.txt
if ($LASTEXITCODE) { throw 'Performance tests failed' }
& 'build/vst3-studio-051/cmake-release/bin/Release/validator.exe' 'build/vst3-studio-051/cmake-release/VST3/Release/MM Studio 0.5.1.vst3' *> build/vst3-studio-051/validator.txt
if ($LASTEXITCODE) { throw 'VST3 validation failed' }
& 'build/vst3-studio-051/cmake-release/Release/mm6_driver_tests.exe' build/vst3-studio/generated/driver-bank.bin build/vst3-studio-051/mm6-registers.csv
if ($LASTEXITCODE) { throw 'MM6 driver tests failed' }
& 'build/vst3-studio-051/cmake-release/Release/game_bank_tests.exe' build/vst3-studio/generated/driver-bank.bin build/vst3-studio-051/game-registers.csv | Tee-Object build/vst3-studio-051/game-bank-tests.txt
if ($LASTEXITCODE) { throw 'Game-bank / fine tune tests failed' }
& 'build/vst3-studio-051/cmake-release/Release/mm5_library_tests.exe' build/vst3-studio/generated/driver-bank.bin | Tee-Object build/vst3-studio-051/library-tests.txt
if ($LASTEXITCODE) { throw 'Source audition library tests failed' }
& 'build/vst3-studio-051/cmake-release/Release/browser_tests.exe' | Tee-Object build/vst3-studio-051/browser-tests.txt
if ($LASTEXITCODE) { throw 'Browser naming tests failed' }
& 'build/vst3-studio-051/cmake-release/Release/held_program_tests.exe' build/vst3-studio/generated/driver-bank.bin build/vst3-studio-051/held-program-writes.csv build/vst3-studio-051/held-program-states.csv | Tee-Object build/vst3-studio-051/held-program-tests.txt
if ($LASTEXITCODE) { throw 'Held native program-change tests failed' }
Write-Output 'Build, native processor and VST validation complete.'

} finally { Pop-Location }
