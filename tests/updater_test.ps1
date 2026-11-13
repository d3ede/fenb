param([string]$Binary='dist/d3d9.dll')
$ErrorActionPreference='Stop'
$binaryPath=(Resolve-Path -LiteralPath $Binary).Path
$repo=Split-Path $PSScriptRoot
. (Join-Path $repo 'tools/update-worker.ps1') -Mode Library
$fixture=Join-Path $repo ('build/update-test-'+[guid]::NewGuid().ToString('N'))
$Root=Join-Path $fixture "game's [test] & space"
$Work=Join-Path $Root 'FENB-update'
[IO.Directory]::CreateDirectory($Work) | Out-Null
function Assert($Condition,[string]$Message) {if(-not $Condition){throw $Message}}
function Reject([scriptblock]$Action,[string]$Message) {
    $rejected=$false
    try { & $Action } catch { $rejected=$true }
    Assert $rejected $Message
}
Assert ((Version-Number '0.3h') -lt (Version-Number '0.4a')) 'Version rollover'
Reject {Version-Number '0.4i'} 'Invalid suffix accepted'
Reject {Version-Number '../x'} 'Invalid version accepted'
[IO.File]::Copy($binaryPath,(Join-Path $Root 'd3d9.dll'))
[IO.File]::WriteAllText((Join-Path $Root 'LICENSES.txt'),'original notices')
[IO.File]::WriteAllText((Join-Path $Root 'FENB.ini'),"[Custom]`nUnknownFutureSetting=42`nLanguage=ru")
[IO.Directory]::CreateDirectory((Join-Path $Root 'FENB-presets')) | Out-Null
[IO.File]::WriteAllText((Join-Path $Root 'FENB-presets/personal.ini'),'personal profile')
$original=Hash (Join-Path $Root 'd3d9.dll')
$ini=Hash (Join-Path $Root 'FENB.ini')
$profile=Hash (Join-Path $Root 'FENB-presets/personal.ini')
$payload=[IO.File]::ReadAllBytes($binaryPath)+[byte]1
[IO.File]::WriteAllBytes((Join-Path $Work 'new.d3d9.dll'),$payload)
[IO.File]::WriteAllText((Join-Path $Work 'new.LICENSES.txt'),'new notices')
$new=Hash (Join-Path $Work 'new.d3d9.dll')
Test-Dll (Join-Path $Work 'new.d3d9.dll')
Install-Payload
Assert ((Hash (Join-Path $Root 'd3d9.dll')) -eq $new) 'Install failed'
Assert ((Hash (Join-Path $Work 'rollback.dll')) -eq $original) 'Wrong rollback generation'
Assert ((Hash (Join-Path $Root 'FENB.ini')) -eq $ini) 'Settings modified'
Assert ((Hash (Join-Path $Root 'FENB-presets/personal.ini')) -eq $profile) 'Profile modified'
Restore-Rollback
Assert ((Hash (Join-Path $Root 'd3d9.dll')) -eq $original) 'Manual rollback failed'
Assert ([IO.File]::ReadAllText((Join-Path $Root 'LICENSES.txt')) -eq 'original notices') 'Notice rollback failed'
# A real sharing violation must never remove the current DLL or the usable snapshot.
$held=[IO.File]::Open((Join-Path $Root 'd3d9.dll'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
try {Reject {Install-Payload} 'Locked DLL was replaced'}finally{$held.Dispose()}
Assert ((Hash (Join-Path $Root 'd3d9.dll')) -eq $original) 'Failed install damaged DLL'
Assert ((Hash (Join-Path $Work 'rollback.dll')) -eq $original) 'Failed install damaged rollback'
Reject {Save-Rollback} 'Pending recovery snapshot was overwritten'
Restore-Rollback
Assert (-not (Test-Path -LiteralPath (Join-Path $Work 'transaction.txt'))) 'Manual recovery left journal'
# Replace the second file unsuccessfully once, exercising restoration after DLL replacement.
$replaceBody=${function:Replace-Checked}
function Replace-Checked([string]$Source,[string]$Target,[string]$Expected) {
    if($Source.EndsWith('new.LICENSES.txt')){throw 'Injected notice failure'}
    & $replaceBody $Source $Target $Expected
}
Reject {Install-Payload} 'Second-file failure was ignored'
Assert ((Hash (Join-Path $Root 'd3d9.dll')) -eq $original) 'Automatic rollback failed'
Set-Item -LiteralPath function:Replace-Checked -Value $replaceBody
Install-Payload
# A subsequent update replaces the previous snapshot instead of accumulating copies.
[IO.File]::WriteAllBytes((Join-Path $Work 'new.d3d9.dll'),($payload+[byte]2))
Install-Payload
Assert ((Hash (Join-Path $Work 'rollback.dll')) -eq $new) 'Previous generation not advanced'
Assert (@(Get-ChildItem -LiteralPath $Work -Filter 'rollback*.dll').Count -eq 1) 'Multiple backup generations'
Restore-Rollback
Assert ((Hash (Join-Path $Root 'd3d9.dll')) -eq $new) 'Second rollback failed'
# Corrupt rollback cannot overwrite the installed DLL.
[IO.File]::WriteAllText((Join-Path $Work 'rollback.dll'),'corrupt')
Reject {Restore-Rollback} 'Corrupt snapshot accepted'
Assert ((Hash (Join-Path $Root 'd3d9.dll')) -eq $new) 'Corrupt rollback damaged DLL'
# Validate architecture and archive allowlist before a restart is offered.
[IO.File]::WriteAllBytes((Join-Path $Work 'invalid.dll'),[byte[]](1,2,3))
Reject {Test-Dll (Join-Path $Work 'invalid.dll')} 'Truncated PE accepted'
$bad=[IO.File]::ReadAllBytes($binaryPath)
$offset=[BitConverter]::ToInt32($bad,60)
$bad[$offset+4]=0x64;$bad[$offset+5]=0x86
[IO.File]::WriteAllBytes((Join-Path $Work 'invalid.dll'),$bad)
Reject {Test-Dll (Join-Path $Work 'invalid.dll')} 'x64 PE accepted'
$package=Join-Path $fixture 'package'
[IO.Directory]::CreateDirectory($package) | Out-Null
foreach($name in @('d3d9.dll','FENB.ini','INSTALL.txt','LICENSES.txt')) {
    if($name -eq 'd3d9.dll'){[IO.File]::Copy($binaryPath,(Join-Path $package $name))}
    else{[IO.File]::WriteAllText((Join-Path $package $name),'shipped default')}
}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$publicVersion=[regex]::Match([IO.File]::ReadAllText((Join-Path $repo 'src/version.hpp')),'FENBVersion\[\]\s*=\s*"(\d\.\d[a-h])"').Groups[1].Value
$zip=Join-Path $fixture ("FENB-"+$publicVersion+"-win32.zip")
[IO.Compression.ZipFile]::CreateFromDirectory($package,$zip)
& node (Join-Path $repo 'tools/prepare-update.mjs') $fixture
Assert ($LASTEXITCODE -eq 0) 'Release manifest generation failed'
$generated=Read-Manifest (Join-Path $fixture 'FENB-update.json')
Assert ($generated.version -eq $publicVersion) 'Release manifest version mismatch'
Verify-Archive $zip $generated
Extract-Payload $zip
Assert ((Hash (Join-Path $Root 'FENB.ini')) -eq $ini) 'Extract overwrote settings'
[IO.File]::WriteAllText((Join-Path $package 'unexpected.ps1'),'untrusted')
$extra=Join-Path $fixture 'extra.zip'
[IO.Compression.ZipFile]::CreateFromDirectory($package,$extra)
Reject {Extract-Payload $extra} 'Unexpected ZIP entry accepted'
$manifest=Join-Path $fixture 'manifest.json'
[IO.File]::WriteAllText($manifest,(@{schema=1;version='0.4b';bytes=(Get-Item -LiteralPath $zip).Length;sha256=(Hash $zip)}|ConvertTo-Json))
$validManifest=Read-Manifest $manifest
Verify-Archive $zip $validManifest
$validManifest.sha256='0'*64
Reject {Verify-Archive $zip $validManifest} 'Corrupt archive accepted'
[IO.File]::WriteAllText($manifest,'{"schema":1,"version":"0.4b","bytes":512,"sha256":"BAD"}')
Reject {Read-Manifest $manifest} 'Bad checksum manifest accepted'
Reject {Fetch 'http://github.com/file' (Join-Path $fixture 'http') 4096} 'HTTP accepted'
Reject {Fetch 'https://example.com/file' (Join-Path $fixture 'untrusted') 4096} 'Untrusted host accepted'
Assert ((Hash (Join-Path $Root 'FENB.ini')) -eq $ini) 'Settings changed after rollback tests'
Assert ((Hash (Join-Path $Root 'FENB-presets/personal.ini')) -eq $profile) 'Profile changed after rollback tests'
$child=Start-Process -FilePath (Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe') -WindowStyle Hidden -ArgumentList @('-NoProfile','-NonInteractive','-Command','Start-Sleep -Milliseconds 1500') -PassThru
$OwnerId=$child.Id;$OwnerStart=$child.StartTime.ToUniversalTime().ToFileTimeUtc()
$timer=[Diagnostics.Stopwatch]::StartNew()
Wait-Owner
Assert ($timer.ElapsedMilliseconds -ge 900) 'Installation did not wait for owner exit'
$OwnerId=$PID;$OwnerStart=1
Wait-Owner # A reused PID with a different creation time must not block.
Write-State 'Ready' '0.4b'
Write-State 'Installed' '0.4b'
Assert ((Get-Content -LiteralPath (Join-Path $Work 'state.txt') -First 1) -eq 'Installed') 'Atomic state write failed'
# Prove the real worker refuses a concurrent controller before any network call.
$held=[IO.File]::Open((Join-Path $Work 'lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
try {
    & (Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe') -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repo 'tools/update-worker.ps1') -Mode Check -Root $Root -CurrentVersion '0.4b'
    Assert ($LASTEXITCODE -eq 3) 'Concurrent worker was not rejected'
}finally{$held.Dispose()}
# The actual script entry point recovers an interrupted transaction without downloading.
Save-Rollback
[IO.File]::WriteAllText((Join-Path $Work 'transaction.txt'),'pending')
& (Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe') -NoProfile -ExecutionPolicy Bypass -File (Join-Path $repo 'tools/update-worker.ps1') -Mode Check -Root $Root -CurrentVersion '0.4b'
Assert ($LASTEXITCODE -eq 0) 'Interrupted transaction recovery failed'
Assert (-not (Test-Path -LiteralPath (Join-Path $Work 'transaction.txt'))) 'Recovery left pending journal'
Assert ((Get-Content -LiteralPath (Join-Path $Work 'state.txt') -First 1) -eq 'Restored') 'Recovery state not published'
$fixturePath=[IO.Path]::GetFullPath($fixture)
$buildRoot=[IO.Path]::GetFullPath((Join-Path $repo 'build'))
if([IO.Path]::GetDirectoryName($fixturePath) -ne $buildRoot -or [IO.Path]::GetFileName($fixturePath) -notmatch '^update-test-[a-f0-9]{32}$'){throw 'Invalid fixture cleanup path'}
Remove-Item -LiteralPath $fixturePath -Recurse -Force
Write-Output 'PASS: update install, real file lock, automatic/manual rollback, single generation, settings/profiles, PE and ZIP validation, HTTPS allowlist.'
exit 0
