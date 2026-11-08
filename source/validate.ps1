param(
    [string]$ToolchainBin = '',
    [string]$BuildDirectory = '',
    [switch]$IncludeGraphicsTests,
    [switch]$IncludeHookSoak,
    [ValidateRange(0,600)][int]$SoakSeconds = 0,
    [ValidateRange(0,120)][int]$UiSmokeSeconds = 0
)
$ErrorActionPreference = 'Stop'
$packageDirectory = Split-Path $PSScriptRoot -Parent
if (!$BuildDirectory) { $BuildDirectory = Join-Path $PSScriptRoot 'build' }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
New-Item -ItemType Directory -Force $BuildDirectory | Out-Null
if (!$ToolchainBin) { $ToolchainBin = Join-Path $BuildDirectory 'toolchain\llvm-mingw-20261104-ucrt-x86_64\bin' }
$cxx = Join-Path $ToolchainBin 'x86_64-w64-mingw32-clang++.exe'
if (!(Test-Path -LiteralPath $cxx)) { throw 'Run build.ps1 first, or supply -ToolchainBin.' }
if ($IncludeHookSoak -and !$IncludeGraphicsTests) { throw 'Use -IncludeGraphicsTests with -IncludeHookSoak for the optional injection stress test.' }
$flags = @('-std=c++20','-O2','-static','-DNDEBUG','-DNOMINMAX','-DUNICODE','-D_UNICODE','-D_WIN32_WINNT=0x0A00')
if ($IncludeGraphicsTests) {
    Write-Output 'Graphics tests target only the bundled Framoryx test scenes.'
    $integration = Join-Path $packageDirectory 'FramoryxCheck.exe'
    & $cxx @flags (Join-Path $PSScriptRoot 'integration_tests.cpp') -o $integration -luser32 -ladvapi32 -lole32 -luuid
    if ($LASTEXITCODE -ne 0) { throw 'Integration-test compilation failed.' }
    & $integration | Tee-Object (Join-Path $BuildDirectory 'integration-results.txt')
    if ($LASTEXITCODE -ne 0) { throw 'Graphics integration checks failed.' }
}
if ($SoakSeconds -gt 0) {
    $testName = if($IncludeHookSoak){'FramoryxSoak.exe'}else{'FramoryxPassiveCheck.exe'}
    $testSource = if($IncludeHookSoak){'stress_tests.cpp'}else{'passive_tests.cpp'}
    $resultName = if($IncludeHookSoak){'soak-results.txt'}else{'passive-soak-results.txt'}
    $completion = if($IncludeHookSoak){'PASS: '+$SoakSeconds+' second DX11 telemetry/limiter/resize soak'}else{'PASS: '+$SoakSeconds+' second non-injecting ETW/borderless soak'}
    if($IncludeHookSoak){Write-Output 'Optional hook stress was blocked by Defender on the development host. A block is not bypassed or counted as a pass.'}
    $soak = Join-Path $packageDirectory $testName
    & $cxx @flags (Join-Path $PSScriptRoot $testSource) -o $soak -luser32 -ladvapi32 -lpsapi
    if ($LASTEXITCODE -ne 0) { throw 'Soak-test compilation failed.' }
    & $soak $SoakSeconds | Tee-Object (Join-Path $BuildDirectory $resultName)
    if ($LASTEXITCODE -eq 2 -and $IncludeHookSoak) { Write-Output 'Hook soak skipped because its guard refused the environment.' }
    elseif ($LASTEXITCODE -ne 0) { throw 'Soak failed.' }
    elseif (!(Select-String -LiteralPath (Join-Path $BuildDirectory $resultName) -SimpleMatch $completion -Quiet)) { throw 'Soak ended without its completion marker; this run is incomplete.' }
}
if ($UiSmokeSeconds -gt 0) {
    $report = Join-Path $BuildDirectory 'ui-smoke.json'
    $settings = Join-Path $BuildDirectory 'ui-test-settings.ini'
    $arguments = @('--smoke-seconds', $UiSmokeSeconds, '--with-demo', '--smoke-resize', '--smoke-animate', '--settings-file', ('"'+$settings+'"'), '--report', ('"' + $report + '"'))
    if($UiSmokeSeconds -ge 16){$arguments += '--smoke-customize'}
    $process = Start-Process -FilePath (Join-Path $packageDirectory 'Framoryx.exe') -ArgumentList $arguments -PassThru
    $process.WaitForExit()
    if ($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath $report)) { throw 'UI smoke did not complete.' }
    Get-Content -LiteralPath $report
}
Write-Output 'Requested validation completed.'
