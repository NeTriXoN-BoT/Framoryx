param([string]$BuildDirectory='work/build')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$build=[IO.Path]::GetFullPath((Join-Path $root $BuildDirectory))
$tools=Join-Path $root 'tools'
New-Item -ItemType Directory -Force -Path $build,$tools | Out-Null
function Download-ReleaseAsset([string]$Repository,[string]$Tag,[string]$Asset,[string]$Directory,[string]$Hash){
    $file=Join-Path $Directory $Asset
    if(Test-Path -LiteralPath $file){if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -eq $Hash){return}}
    for($attempt=0;$attempt -lt 4;$attempt++){
        & gh release download $Tag --repo $Repository --pattern $Asset --dir $Directory --clobber
        if($LASTEXITCODE -eq 0){break}
        if($attempt -eq 3){throw ('Download failed: '+$Asset)}
        Start-Sleep -Seconds (2*($attempt+1))
    }
    if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $Hash){throw ('SHA-256 mismatch: '+$Asset)}
}
$compiler=Join-Path $build 'toolchain/llvm-mingw-20261104-ucrt-x86_64/bin/x86_64-w64-mingw32-clang++.exe'
if(!(Test-Path -LiteralPath $compiler)){
    Download-ReleaseAsset 'mstorsjo/llvm-mingw' '20261104' 'llvm-mingw-20261104-ucrt-x86_64.zip' $build '552191b5754c49a5a20723276a6fc56541ea1922c4c50b7b669022333fc1b40f'
    Expand-Archive -LiteralPath (Join-Path $build 'llvm-mingw-20261104-ucrt-x86_64.zip') -DestinationPath (Join-Path $build 'toolchain') -Force
}
Download-ReleaseAsset 'GameTechDev/PresentMon' 'v2.6.0' 'PresentMon-2.6.0-x64.exe' $tools 'b2a706bc6ad475749e3b7e3409263aa1e6906d45bdcf993f6dbc0f660188f1af'
Copy-Item -LiteralPath (Join-Path $tools 'PresentMon-2.6.0-x64.exe') -Destination (Join-Path $tools 'PresentMon.exe') -Force
Write-Output 'Pinned CI compiler and PresentMon are ready.'
