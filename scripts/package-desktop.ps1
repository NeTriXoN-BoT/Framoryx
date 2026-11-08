param([string]$BuildDirectory='build/desktop/desktop/Release',[string]$Destination='dist/desktop')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$build=[IO.Path]::GetFullPath((Join-Path $root $BuildDirectory))
$destinationRoot=[IO.Path]::GetFullPath((Join-Path $root $Destination))
$package=Join-Path $destinationRoot 'Framoryx'
if(Test-Path -LiteralPath $package){throw 'Package destination must be fresh.'}
New-Item -ItemType Directory -Path $package -Force | Out-Null
foreach($file in @('Framoryx.exe','FramoryxEngine.exe','FramoryxScene.exe','FramoryxHook.dll','FramoryxHook32.dll')){
    Copy-Item -LiteralPath (Join-Path $build $file) -Destination $package
}
Copy-Item -LiteralPath (Join-Path $build 'tools') -Destination $package -Recurse
foreach($folder in @('licenses','docs')){Copy-Item -LiteralPath (Join-Path $root $folder) -Destination $package -Recurse}
foreach($file in @('README.md','LICENSE','THIRD-PARTY-NOTICES.txt','VERSION')){Copy-Item -LiteralPath (Join-Path $root $file) -Destination $package}
& windeployqt --release --no-translations --no-compiler-runtime --qmldir (Join-Path $root 'desktop/qml') --dir $package (Join-Path $package 'Framoryx.exe')
if($LASTEXITCODE -ne 0){throw 'Qt deployment failed.'}
# Use only the licensed redistributable CRT directory, never DLLs from System32.
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$visualStudio=& $vswhere -latest -products '*' -property installationPath
if(!$visualStudio){throw 'The Visual C++ redistributable directory was not found.'}
$redist=Join-Path $visualStudio 'VC/Redist/MSVC'
$crt=Get-ChildItem -LiteralPath $redist -Directory | Sort-Object Name -Descending | ForEach-Object {Get-ChildItem -LiteralPath (Join-Path $_.FullName 'x64') -Directory -Filter 'Microsoft.VC*.CRT' -ErrorAction SilentlyContinue} | Select-Object -First 1
if(!$crt){throw 'The licensed x64 CRT redistributable was not found.'}
Get-ChildItem -LiteralPath $crt.FullName -File -Filter '*.dll' | Copy-Item -Destination $package
$qtRoot=Split-Path (Split-Path (Get-Command windeployqt).Source -Parent) -Parent
if(Test-Path -LiteralPath (Join-Path $qtRoot 'Licenses')){Copy-Item -LiteralPath (Join-Path $qtRoot 'Licenses') -Destination (Join-Path $package 'licenses/Qt') -Recurse}
[IO.File]::WriteAllText((Join-Path $package 'UNSIGNED-BUILD.txt'),"Framoryx is unsigned: it has no Authenticode publisher certificate.`r`nVerify the release SHA-256 before use. In-game overlays and hooks are used at your own risk.`r`n",[Text.UTF8Encoding]::new($false))
$manifest=Join-Path $package 'SHA256SUMS.txt'
$lines=Get-ChildItem -LiteralPath $package -File -Recurse | Sort-Object FullName | ForEach-Object {(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetRelativePath($package,$_.FullName).Replace('\','/')}
[IO.File]::WriteAllLines($manifest,$lines,[Text.UTF8Encoding]::new($false))
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=Join-Path $destinationRoot 'Framoryx-Windows-x64.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($package,$archive,[IO.Compression.CompressionLevel]::Optimal,$true)
$hash=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(($archive+'.sha256'),($hash+'  '+[IO.Path]::GetFileName($archive)+"`n"),[Text.UTF8Encoding]::new($false))
Write-Output ('Created '+$archive)
