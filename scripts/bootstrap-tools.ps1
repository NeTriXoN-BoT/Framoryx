param([string]$Destination = '')
$ErrorActionPreference = 'Stop'
if(!$Destination){$Destination=Join-Path (Split-Path $PSScriptRoot -Parent) 'tools'}
New-Item -ItemType Directory -Force -Path $Destination | Out-Null
$binary=Join-Path $Destination 'PresentMon.exe'
$expected='b2a706bc6ad475749e3b7e3409263aa1e6906d45bdcf993f6dbc0f660188f1af'
if(!(Test-Path -LiteralPath $binary) -or (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected){
    Invoke-WebRequest 'https://github.com/GameTechDev/PresentMon/releases/download/v2.6.0/PresentMon-2.6.0-x64.exe' -OutFile $binary
}
if((Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected){throw 'PresentMon SHA-256 verification failed.'}
Write-Output 'Verified PresentMon 2.6.0.'
