param([string]$ToolchainBin='', [string]$BuildDirectory='', [string]$Destination='')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
if(!$BuildDirectory){$BuildDirectory=Join-Path $root 'work/build'}
if(!$Destination){$Destination=Join-Path $root 'dist'}
$package=Join-Path $Destination 'Framoryx'
if(Test-Path -LiteralPath $package){throw 'Use a fresh destination so a release cannot pick up old settings, captures or build files.'}
New-Item -ItemType Directory -Force -Path $package | Out-Null
foreach($folder in @('assets','licenses','source','docs','scripts')){
    $from=Join-Path $root $folder
    Copy-Item -LiteralPath $from -Destination $package -Recurse -Force
}
foreach($file in @('README.md','README.txt','LICENSE','THIRD-PARTY-NOTICES.txt','VERSION','CMakeLists.txt')){Copy-Item -LiteralPath (Join-Path $root $file) -Destination $package -Force}
& (Join-Path $PSScriptRoot 'bootstrap-tools.ps1') -Destination (Join-Path $root 'tools')
New-Item -ItemType Directory -Force -Path (Join-Path $package 'tools') | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'tools/PresentMon.exe') -Destination (Join-Path $package 'tools/PresentMon.exe')
& (Join-Path $root 'source/build.ps1') -ToolchainBin $ToolchainBin -BuildDirectory $BuildDirectory -OutputDirectory $package
if($LASTEXITCODE -ne 0){throw 'Native build failed.'}
$manifest=Join-Path $package 'SHA256SUMS.txt'
$files=Get-ChildItem -LiteralPath $package -File -Recurse | Where-Object {$_.FullName -ne $manifest -and $_.Name -ne 'settings.ini'} | Sort-Object FullName
$lines=$files | ForEach-Object {(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()+'  '+[IO.Path]::GetRelativePath($package,$_.FullName).Replace('\','/')}
[IO.File]::WriteAllLines($manifest,$lines,[Text.UTF8Encoding]::new($false))
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=Join-Path $Destination 'Framoryx-Windows-x64.zip'
if(Test-Path -LiteralPath $archive){throw 'Release ZIP already exists. Use a fresh destination for a new build.'}
[IO.Compression.ZipFile]::CreateFromDirectory($package,$archive,[IO.Compression.CompressionLevel]::Optimal,$true)
$hash=(Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText(($archive+'.sha256'),($hash+'  '+[IO.Path]::GetFileName($archive)+[Environment]::NewLine),[Text.UTF8Encoding]::new($false))
Write-Output ('Created '+$archive)
