param([string]$ToolchainBin = '', [string]$BuildDirectory = '', [string]$OutputDirectory = '')
$ErrorActionPreference = 'Stop'
$sourceDirectory = $PSScriptRoot
$packageDirectory = Split-Path $sourceDirectory -Parent
if($OutputDirectory){$packageDirectory=[IO.Path]::GetFullPath($OutputDirectory);New-Item -ItemType Directory -Force -Path (Join-Path $packageDirectory 'tools') | Out-Null}
if (!$BuildDirectory) { $BuildDirectory = Join-Path (Split-Path $sourceDirectory -Parent) 'work/build' }
New-Item -ItemType Directory -Force -Path (Join-Path $packageDirectory 'tools') | Out-Null
$bootstrap=Join-Path (Split-Path $sourceDirectory -Parent) 'scripts/bootstrap-tools.ps1'
if(Test-Path -LiteralPath $bootstrap){& $bootstrap -Destination (Join-Path $packageDirectory 'tools')}
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
New-Item -ItemType Directory -Force $BuildDirectory | Out-Null
if (!$ToolchainBin) {
    $toolchainDirectory = Join-Path $BuildDirectory 'toolchain'
    $ToolchainBin = Join-Path $toolchainDirectory 'llvm-mingw-20261104-ucrt-x86_64\bin'
    if (!(Test-Path (Join-Path $ToolchainBin 'x86_64-w64-mingw32-clang++.exe'))) {
        $archive = Join-Path $BuildDirectory 'llvm.zip'
        Write-Output 'Downloading portable LLVM-MinGW (no system installation)...'
        for($attempt=0;$attempt -lt 4;$attempt++){
            try{Invoke-WebRequest 'https://github.com/mstorsjo/llvm-mingw/releases/download/20261104/llvm-mingw-20261104-ucrt-x86_64.zip' -OutFile $archive -UseBasicParsing -TimeoutSec 600;break}
            catch{if($attempt -eq 3){throw};Start-Sleep -Seconds (2*($attempt+1))}
        }
        if ((Get-FileHash $archive -Algorithm SHA256).Hash -ne '552191b5754c49a5a20723276a6fc56541ea1922c4c50b7b669022333fc1b40f') { throw 'Toolchain SHA-256 mismatch' }
        Expand-Archive -LiteralPath $archive -DestinationPath $toolchainDirectory -Force
    }
}
$cxx = Join-Path $ToolchainBin 'x86_64-w64-mingw32-clang++.exe'
$cc = Join-Path $ToolchainBin 'x86_64-w64-mingw32-clang.exe'
$rc = Join-Path $ToolchainBin 'x86_64-w64-mingw32-windres.exe'
function Invoke-Compiler([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $Executable" }
}
$flags = @('-std=c++20','-O2','-static','-DNDEBUG','-DNOMINMAX','-DUNICODE','-D_UNICODE','-D_WIN32_WINNT=0x0A00','-Wall','-Wextra','-Wno-missing-field-initializers','-Wno-unused-parameter')
Push-Location $sourceDirectory
try {
    $objects = @()
    foreach ($file in @('buffer.c','hook.c','trampoline.c','hde\hde64.c')) {
        $obj = Join-Path $BuildDirectory ((Split-Path $file -Leaf) + '.o')
        Invoke-Compiler $cc @('-O2','-DNOMINMAX','-D_WIN32_WINNT=0x0A00','-Ivendor\minhook\include','-c',"vendor\minhook\src\$file",'-o',$obj)
        $objects += $obj
    }
    Invoke-Compiler $rc @('-i','app.rc','-o',(Join-Path $BuildDirectory 'app.o'))
    Write-Output 'Building native app...'
    Invoke-Compiler $cxx ($flags + @('-municode','-mwindows','main.cpp',(Join-Path $BuildDirectory 'app.o'),'-o',(Join-Path $packageDirectory 'Framoryx.exe'),'-lgdiplus','-lgdi32','-luser32','-lshell32','-lole32','-luuid','-lcomctl32','-lcomdlg32','-ldwmapi','-luxtheme','-ladvapi32','-Wl,--dynamicbase,--nxcompat,--high-entropy-va','-s'))
    Write-Output 'Building opt-in hook module...'
    Invoke-Compiler $cxx ($flags + @('-shared','-Ivendor\minhook\include','-Ivendor\imgui','hook.cpp','vendor\imgui\imgui.cpp','vendor\imgui\imgui_draw.cpp','vendor\imgui\imgui_tables.cpp','vendor\imgui\imgui_widgets.cpp','vendor\imgui\backends\imgui_impl_dx11.cpp') + $objects + @('-o',(Join-Path $packageDirectory 'FramoryxHook.dll'),'-ld3d11','-ld3d12','-ldxgi','-ld3dcompiler','-lgdi32','-luser32','-Wl,--dynamicbase,--nxcompat,--high-entropy-va','-s'))
    Write-Output 'Building graphics test scene and core checks...'
    Invoke-Compiler $cxx ($flags + @('-municode','-mwindows','test_scene.cpp','-o',(Join-Path $packageDirectory 'tools\FramoryxTest.exe'),'-ld3d11','-ldxgi','-ld3dcompiler','-lshell32','-luser32','-ladvapi32','-s'))
    Invoke-Compiler $cxx ($flags + @('-municode','-mwindows','test_scene12.cpp','-o',(Join-Path $packageDirectory 'tools\FramoryxTest12.exe'),'-ld3d12','-ldxgi','-lshell32','-luser32','-ladvapi32','-s'))
    Write-Output 'Building 32-bit DX11 compatibility modules...'
    $cxx32 = Join-Path $ToolchainBin 'i686-w64-mingw32-clang++.exe'
    $cc32 = Join-Path $ToolchainBin 'i686-w64-mingw32-clang.exe'
    $objects32 = @()
    foreach ($file in @('buffer.c','hook.c','trampoline.c','hde\hde32.c')) {
        $obj = Join-Path $BuildDirectory ('x86-' + (Split-Path $file -Leaf) + '.o')
        Invoke-Compiler $cc32 @('-O2','-DNOMINMAX','-D_WIN32_WINNT=0x0A00','-Ivendor\minhook\include','-c',"vendor\minhook\src\$file",'-o',$obj)
        $objects32 += $obj
    }
    Invoke-Compiler $cxx32 ($flags + @('-shared','-Ivendor\minhook\include','-Ivendor\imgui','hook.cpp','vendor\imgui\imgui.cpp','vendor\imgui\imgui_draw.cpp','vendor\imgui\imgui_tables.cpp','vendor\imgui\imgui_widgets.cpp','vendor\imgui\backends\imgui_impl_dx11.cpp') + $objects32 + @('-o',(Join-Path $packageDirectory 'FramoryxHook32.dll'),'-ld3d11','-ld3d12','-ldxgi','-ld3dcompiler','-lgdi32','-luser32','-Wl,--dynamicbase,--nxcompat','-s'))
    Invoke-Compiler $cxx32 ($flags + @('-municode','-mwindows','injector32.cpp','-o',(Join-Path $packageDirectory 'tools\FramoryxInjector32.exe'),'-lshell32','-luser32','-ladvapi32','-Wl,--dynamicbase,--nxcompat','-s'))
    Invoke-Compiler $cxx32 ($flags + @('-municode','-mwindows','test_scene.cpp','-o',(Join-Path $packageDirectory 'tools\FramoryxTest32.exe'),'-ld3d11','-ldxgi','-ld3dcompiler','-lshell32','-luser32','-ladvapi32','-s'))
    Invoke-Compiler $cxx ($flags + @('tests.cpp','-o',(Join-Path $BuildDirectory 'core-tests.exe')))
    & (Join-Path $BuildDirectory 'core-tests.exe')
    if ($LASTEXITCODE -ne 0) { throw 'Core tests failed' }
    Invoke-Compiler $cxx ($flags + @('audio_tests.cpp','-o',(Join-Path $BuildDirectory 'audio-tests.exe'),'-ladvapi32','-lole32','-luuid'))
    & (Join-Path $BuildDirectory 'audio-tests.exe') (Join-Path $BuildDirectory 'audio-tests-data')
    if ($LASTEXITCODE -ne 0) { throw 'Audio configuration tests failed' }
    Write-Output 'Build and core checks passed.'
} finally { Pop-Location }
