param([Parameter(Mandatory)][string]$ToolchainBin,[string]$BuildDirectory='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$BuildDirectory){$BuildDirectory=Join-Path $root 'work/sanitizers'}
New-Item -ItemType Directory -Force -Path $BuildDirectory | Out-Null
$BuildDirectory=[IO.Path]::GetFullPath($BuildDirectory)
$compiler=Join-Path $ToolchainBin 'x86_64-w64-mingw32-clang++.exe'
$previousPath=$env:PATH
$previousAsan=$env:ASAN_OPTIONS
$previousUbsan=$env:UBSAN_OPTIONS
try {
    $env:PATH=([IO.Path]::GetFullPath($ToolchainBin))+';'+$env:PATH
    $env:ASAN_OPTIONS='halt_on_error=1:detect_leaks=0'
    $env:UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'
    foreach($name in @('tests','fuzz_tests','audio_tests','microphone_tests','safety_tests')) {
        $exe=Join-Path $BuildDirectory ($name+'.exe')
        & $compiler '-std=c++20' '-O1' '-g' '-DNOMINMAX' '-D_WIN32_WINNT=0x0A00' '-fno-omit-frame-pointer' '-fsanitize=address,undefined' (Join-Path $root "source/$name.cpp") '-o' $exe '-ladvapi32' '-lole32' '-luuid' '-lshell32' '-lbcrypt'
        if($LASTEXITCODE -ne 0){throw "Sanitizer build failed: $name"}
        & $exe (Join-Path $BuildDirectory 'scratch-configs')
        if($LASTEXITCODE -ne 0){throw "Sanitizer check failed: $name"}
    }
    $exe=Join-Path $BuildDirectory 'dependency-tests.exe'
    $dependencySources=@('dependency_tests.cpp','vendor/imgui/imgui.cpp','vendor/imgui/imgui_draw.cpp','vendor/imgui/imgui_tables.cpp','vendor/imgui/imgui_widgets.cpp') | ForEach-Object {Join-Path $root ('source/'+$_)}
    & $compiler '-std=c++20' '-O1' '-g' '-DNOMINMAX' '-D_WIN32_WINNT=0x0A00' '-fno-omit-frame-pointer' '-fsanitize=address,undefined' @dependencySources '-o' $exe '-limm32'
    if($LASTEXITCODE -ne 0){throw 'Dependency sanitizer build failed.'}
    & $exe
    if($LASTEXITCODE -ne 0){throw 'Headless dependency sanitizer checks failed.'}
} finally {
    $env:PATH=$previousPath
    $env:ASAN_OPTIONS=$previousAsan
    $env:UBSAN_OPTIONS=$previousUbsan
}
