param([string]$PackageDirectory='./dist/Framoryx',[string]$ReportsDirectory='./work/feature-reports')
$ErrorActionPreference='Stop'
if($env:GITHUB_ACTIONS -ne 'true'){throw 'Window/graphics feature checks run only on the isolated CI runner. Use the headless build/tests locally.'}
$package=(Resolve-Path -LiteralPath $PackageDirectory).Path
$reports=[IO.Path]::GetFullPath($ReportsDirectory)
New-Item -ItemType Directory -Force -Path $reports | Out-Null
$app=Join-Path $package 'Framoryx.exe'
$null=[Environment]::GetFolderPath([Environment+SpecialFolder]::Desktop,[Environment+SpecialFolderOption]::Create)
function Run-Diagnostic([string]$Name,[int]$Seconds,[string[]]$Options=@(),[string]$Settings="motion=1`nauto_noise_updates=0`ndesktop_scale=100`n",[string]$Executable=$app){
    $settingsFile=Join-Path $reports ($Name+'.ini')
    $reportFile=Join-Path $reports ($Name+'.json')
    [IO.File]::WriteAllText($settingsFile,$Settings)
    $arguments=@('--smoke-seconds',$Seconds,'--settings-file',('"'+$settingsFile+'"'),'--report',('"'+$reportFile+'"'))+$Options
    $process=Start-Process -FilePath $Executable -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(($Seconds+20)*1000)){Stop-Process -Id $process.Id -Force;throw "Diagnostic timed out: $Name"}
    if(!(Test-Path -LiteralPath $reportFile)){throw "Diagnostic report missing: $Name (exit $($process.ExitCode))"}
    $result=Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
    if($process.ExitCode -ne 0){$result|ConvertTo-Json -Depth 5|Write-Output;throw "Diagnostic failed: $Name"}
    return $result
}
$full=Run-Diagnostic 'feature-controls' 70 @('--smoke-feature-controls','--with-demo','--smoke-animate','--smoke-maximized')
if($full.featureSteps -ne 24 -or $full.featureChecks -lt 60 -or $full.featureFailures.Count -ne 0 -or $full.audioControlChecks -ne 8 -or $full.rulesControlChecks -ne 2){throw 'Incomplete control-flow coverage.'}
Write-Output ("PASS: {0} feature controls, 8 audio controls, 2 rule controls; {1} live frame samples." -f $full.featureChecks,$full.demoLiveSamples)
$profileResult=Run-Diagnostic 'profiles-motion' 28 @('--with-demo','--smoke-customize','--smoke-animate','--smoke-resize','--smoke-shown')
if($profileResult.customizationChecks -ne 4 -or !$profileResult.customizationRoundTrip -or $profileResult.transitionsCompleted -lt 6 -or $profileResult.animationFrames -lt 30){throw 'Profile/motion round-trip incomplete.'}
Write-Output ("PASS: profile persistence, motion and live capture; average paint {0} ms, transition storage {1} bytes." -f $profileResult.averagePaintMs,$profileResult.peakTransitionBytes)
$missing=Join-Path $reports 'missing-runtime'
New-Item -ItemType Directory -Force -Path $missing | Out-Null
Copy-Item -LiteralPath $app -Destination (Join-Path $missing 'Framoryx.exe')
Copy-Item -LiteralPath (Join-Path $package 'assets') -Destination $missing -Recurse
$failure=Run-Diagnostic 'missing-runtime' 7 @('--smoke-capture-failure','--smoke-page','0','--reduced-motion') -Executable (Join-Path $missing 'Framoryx.exe')
if($failure.featureChecks -ne 1 -or $failure.featureFailures.Count -ne 0){throw 'Capture error regression incomplete.'}
Write-Output 'PASS: missing collector error stays visible and overlay state rolls back.'
$shots=@(
    @{Name='overview';Page=0},@{Name='limiter';Page=2},@{Name='borderless';Page=3},
    @{Name='guide';Page=4},@{Name='compatibility';Page=6},
    @{Name='studio-glass';Page=5;Style=0},@{Name='studio-compact';Page=5;Style=1},@{Name='studio-telemetry';Page=5;Style=2},
    @{Name='audio-output';Page=1;Audio=$true},@{Name='audio-mic';Page=1;Audio=$true;Mic=$true},@{Name='audio-setup';Page=1;Audio=$true;Setup=$true}
)
foreach($shot in $shots){
    $image=Join-Path $reports ($shot.Name+'.png')
    $options=@('--smoke-page',$shot.Page,'--snapshot',('"'+$image+'"'),'--reduced-motion','--smoke-shown')
    if($shot.Audio){$options+='--smoke-audio-fixture'}
    if($shot.Mic){$options+='--audio-mic'}
    if($shot.Setup){$options+='--audio-setup'}
    $style=if($shot.ContainsKey('Style')){$shot.Style}else{0}
    $settings="motion=0`nauto_noise_updates=0`ndesktop_scale=100`nhud_base=1023|0|90|0|0|0|$style`nmic_options=0 1 1 6 -3 0`n"
    $null=Run-Diagnostic $shot.Name 6 $options $settings
    if(!(Test-Path -LiteralPath $image)){throw ('Snapshot missing: '+$shot.Name)}
}
$rulesImage=Join-Path $reports 'rules.png'
$null=Run-Diagnostic 'rules' 10 @('--smoke-page','2','--rules-snapshot',('"'+$rulesImage+'"'),'--reduced-motion')
if(!(Test-Path -LiteralPath $rulesImage)){throw 'Rules snapshot missing.'}
Write-Output 'PASS: generated 12 native UI snapshots for visual review.'
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class FramoryxCiWindow {
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd,uint msg,IntPtr w,IntPtr l);
}
'@
$settings=Join-Path $reports 'single-instance.ini'
[IO.File]::WriteAllText($settings,"motion=0`nauto_noise_updates=0`n")
$first=Start-Process -FilePath $app -ArgumentList @('--settings-file',('"'+$settings+'"')) -WindowStyle Hidden -PassThru
try {
    $handle=[IntPtr]::Zero
    foreach($attempt in 1..60){
        $candidate=[FramoryxCiWindow]::FindWindow('Framoryx.Desktop',[NullString]::Value)
        [uint32]$owner=0;$null=[FramoryxCiWindow]::GetWindowThreadProcessId($candidate,[ref]$owner)
        if($candidate -ne [IntPtr]::Zero -and $owner -eq $first.Id){$handle=$candidate;break}
        Start-Sleep -Milliseconds 100
    }
    if($handle -eq [IntPtr]::Zero){throw "First instance did not initialize (exited=$($first.HasExited), exit=$($first.ExitCode))."}
    $second=Start-Process -FilePath $app -ArgumentList @('--settings-file',('"'+$settings+'"')) -WindowStyle Hidden -PassThru
    if(!$second.WaitForExit(10000) -or $second.ExitCode -ne 0 -or $first.HasExited){throw 'Single-instance activation failed.'}
    foreach($attempt in 1..20){if([FramoryxCiWindow]::IsWindowVisible($handle)){break};Start-Sleep -Milliseconds 100}
    if(![FramoryxCiWindow]::IsWindowVisible($handle)){throw 'Existing instance was not restored.'}
    $null=[FramoryxCiWindow]::PostMessage($handle,0x10,[IntPtr]::Zero,[IntPtr]::Zero)
    if(!$first.WaitForExit(10000)){throw 'Normal instance did not shut down.'}
} finally {if(!$first.HasExited){Stop-Process -Id $first.Id -Force}}
$link=(New-Object -ComObject WScript.Shell).CreateShortcut((Join-Path ([Environment]::GetFolderPath('Desktop')) 'Framoryx.lnk'))
if($link.TargetPath -ne $app){throw 'Generated shortcut targets the wrong executable.'}
Write-Output 'PASS: actual single-instance activation, clean exit and desktop shortcut target.'
