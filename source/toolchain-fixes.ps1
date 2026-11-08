param([Parameter(Mandatory)][string]$ToolchainBin)
$ErrorActionPreference='Stop'
$base=[IO.Path]::GetFullPath((Join-Path $ToolchainBin '..'))
function Digest([byte[]]$Bytes){
    $hash=[Security.Cryptography.SHA256]::Create()
    try{([BitConverter]::ToString($hash.ComputeHash($Bytes))).Replace('-','').ToLowerInvariant()}
    finally{$hash.Dispose()}
}
$patches=@(
    @{Path='lib/clang/23/include/cetintrin.h';Before='ec1203e06e12b4a21944443330eab3a968b1048d429470c4ee0236b7ddfa39e7';After='b3fd2b4510f9d2c61eb02fe102b9b33d6674ea4bb1154d0b7fbc655748135c54';Changes=@(
        @{Old=@'
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  unsigned int t;
  return __builtin_ia32_rdsspd(t);
#pragma clang diagnostic pop
'@
New=@'
  unsigned int t = 0;
  return __builtin_ia32_rdsspd(t);
'@
},
        @{Old=@'
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  unsigned long long t;
  return __builtin_ia32_rdsspq(t);
#pragma clang diagnostic pop
'@
New=@'
  unsigned long long t = 0;
  return __builtin_ia32_rdsspq(t);
'@
}
    )},
    @{Path='include/c++/v1/fstream';Before='eb3d4e323574046d891ce8ffa19dd637e09a62868554ec868905eaa8431c9108';After='c11d547a514fad21d70c0cd14fde0d568a99a98941b69ab728b936eeb5dcd120';Changes=@(
        @{Old=@'
__c += __width * (this->egptr() - this->gptr());
'@
New=@'
__c += static_cast<off_type>(__width) * (this->egptr() - this->gptr());
'@
}
    )}

)
$pending=@()
foreach($patch in $patches){
    $path=Join-Path $base $patch.Path
    if(!(Test-Path -LiteralPath $path)){throw ('Required pinned toolchain header is missing: '+$patch.Path)}
    $bytes=[IO.File]::ReadAllBytes($path)
    $digest=Digest $bytes
    if($digest -eq $patch.After){continue}
    if($digest -ne $patch.Before){throw ('Unrecognized toolchain header; no patch was applied: '+$patch.Path)}
    $text=[Text.Encoding]::UTF8.GetString($bytes)
    foreach($change in $patch.Changes){
        $before=$change.Old.Replace("`r`n","`n")
        $after=$change.New.Replace("`r`n","`n")
        if($text.IndexOf($before,[StringComparison]::Ordinal) -lt 0){throw 'Toolchain patch context mismatch.'}
        $text=$text.Replace($before,$after)
    }
    $updated=[Text.UTF8Encoding]::new($false).GetBytes($text)
    if((Digest $updated) -ne $patch.After){throw 'Toolchain patch output checksum mismatch.'}
    $pending+=@{Path=$path;Bytes=$updated}
}
# Validate every input/output before patching the explicitly selected portable toolchain.
foreach($update in $pending){
    $temporary=$update.Path+'.framoryx-'+[Guid]::NewGuid().ToString('N')+'.tmp'
    try {
        [IO.File]::WriteAllBytes($temporary,$update.Bytes)
        Move-Item -LiteralPath $temporary -Destination $update.Path -Force
    } finally {
        if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary}
    }
}
Write-Output 'Verified pinned CET fallback and stream-offset header fixes.'
