# Build and check a local release archive. This does not publish or install it.
param()
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot

$versionMatch=[regex]::Match((Get-Content -LiteralPath 'src/version.hpp' -Raw),'FENBVersion\[\]\s*=\s*"(\d\.\d[a-h])"')
if(-not $versionMatch.Success){throw 'Invalid public version in src/version.hpp'}
$version=$versionMatch.Groups[1].Value

$staging=Join-Path $PSScriptRoot ('build/release-'+[guid]::NewGuid().ToString('N'))
& './build.ps1' -Tests -OutputDirectory $staging
node tools/verify-binary.mjs (Join-Path $staging 'd3d9.dll')
if($LASTEXITCODE){throw 'Release binary verification failed'}
if((Get-Content -LiteralPath 'src/version.hpp' -Raw) -notmatch ('FENBVersion\[\]\s*=\s*"'+[regex]::Escape($version)+'"')){
    throw 'Version changed during the build; run release.ps1 again'
}

# Native fixtures may save settings beside their DLL. Always ship pristine defaults.
Copy-Item -LiteralPath 'config/FENB.ini' -Destination (Join-Path $staging 'FENB.ini') -Force
node tools/prepare-distribution.mjs $staging
if($LASTEXITCODE){throw 'Release documentation generation failed'}

# Never archive dist wholesale: it can contain logs, tests and personal profiles.
$files=@('d3d9.dll','FENB.ini','INSTALL.txt','LICENSES.txt')
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$temporary=Join-Path $staging 'release.zip'
$destination=Join-Path $PSScriptRoot "dist/FENB-$version-win32.zip"
try{
    $archive=[System.IO.Compression.ZipFile]::Open($temporary,[System.IO.Compression.ZipArchiveMode]::Create)
    try{
        foreach($file in $files){
            $source=Join-Path $staging $file
            [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,$source,$file,[System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
        }
    }finally{$archive.Dispose()}

    # Read back every payload before replacing an existing local archive.
    $archive=[System.IO.Compression.ZipFile]::OpenRead($temporary)
    try{
        if($archive.Entries.Count -ne $files.Count){throw 'Unexpected release archive contents'}
        foreach($file in $files){
            $entry=$archive.GetEntry($file)
            if(-not $entry){throw "Missing release entry: $file"}
            $stream=$entry.Open()
            $sha=[System.Security.Cryptography.SHA256]::Create()
            try{$actual=[BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','')}
            finally{$sha.Dispose();$stream.Dispose()}
            $expected=(Get-FileHash -LiteralPath (Join-Path $staging $file) -Algorithm SHA256).Hash
            if($actual -ne $expected){throw "Release entry verification failed: $file"}
        }
    }finally{$archive.Dispose()}
    New-Item -ItemType Directory -Force -Path (Join-Path $PSScriptRoot 'dist') | Out-Null
    Move-Item -LiteralPath $temporary -Destination $destination -Force
}finally{
    if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary}
}
Get-Item -LiteralPath $destination | Select-Object FullName,Length
node tools/prepare-update.mjs dist
if($LASTEXITCODE){throw 'Update manifest generation failed'}
Get-Item -LiteralPath 'dist/FENB-update.json' | Select-Object FullName,Length
