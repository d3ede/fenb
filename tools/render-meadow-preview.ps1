param([Parameter(Mandatory=$true)][string]$ClientArtworkRoot,[string]$OutputDirectory='artifacts/meadow-preview')
$ErrorActionPreference='Stop'
Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)
$output=[IO.Path]::GetFullPath($OutputDirectory)
$artifacts=[IO.Path]::GetFullPath((Join-Path $PWD 'artifacts'))+[IO.Path]::DirectorySeparatorChar
if(-not $output.StartsWith($artifacts,[StringComparison]::OrdinalIgnoreCase)){throw 'Keep disposable previews inside artifacts/.'}
$client=(Resolve-Path -LiteralPath $ClientArtworkRoot).Path
$toolchain=Get-ChildItem -LiteralPath '.tools' -Directory -Filter 'llvm-mingw-*-ucrt-x86_64' | Sort-Object Name -Descending | Select-Object -First 1
if(-not $toolchain){throw 'Portable LLVM-MinGW is missing.'}
New-Item -ItemType Directory -Force -Path build,$output | Out-Null
$cxx=Join-Path $toolchain.FullName 'bin/i686-w64-mingw32-clang++.exe'
& $cxx '-std=c++20' '-O2' '-static' '-municode' '-Isrc' 'tools/render-meadow-preview.cpp' '-ld3d9' '-ldxguid' '-ld3dcompiler' '-lgdiplus' '-o' 'build/meadow-preview.exe'
if($LASTEXITCODE){throw 'Meadow preview compilation failed.'}
& './build/meadow-preview.exe' $client $output
if($LASTEXITCODE){throw 'Meadow preview rendering failed.'}
$artworkFiles=@('World/Texture/ground_lawn001.dds','World/Texture/ground_n_ice02.dds')
$manifest=[ordered]@{
    sourceRevision=(git rev-parse HEAD).Trim()
    studySourceSha256=(Get-FileHash -LiteralPath tools/render-meadow-preview.cpp -Algorithm SHA256).Hash
    artworkSourceSha256=(Get-FileHash -LiteralPath tools/meadow_artwork.hpp -Algorithm SHA256).Hash
    atlasSize=@(2048,2048)
    atlasVariants=4
    generatedArtwork=@(@('summer-atlas.png','winter-atlas.png') | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $output $_) -Algorithm SHA256).Hash} })
    patchMeters=@(3,3)
    nativeArtwork=@($artworkFiles | ForEach-Object { [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath (Join-Path $client $_) -Algorithm SHA256).Hash} })
    limitations=@('Isolated flat test patch; not installed in the game.','No terrain masks, wind, fog, HUD, LOD or live-client lighting.','Card sorting uses root depth; intersecting translucent cards can have ordering artifacts.')
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $output 'manifest.json') -Encoding UTF8
