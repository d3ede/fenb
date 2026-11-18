param(
    [Parameter(Mandatory = $true)][string]$ClientArtworkRoot,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../artifacts/feature-showcase-2026-11-18'),
    [string]$ToolchainDirectory = (Join-Path $PSScriptRoot '../.tools'),
    [switch]$ReuseRaw
)

$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
Set-Location -LiteralPath $repoRoot
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
$artworkRoot = (Resolve-Path -LiteralPath $ClientArtworkRoot).Path
$artifactRoot = [IO.Path]::GetFullPath((Join-Path $repoRoot 'artifacts')) + [IO.Path]::DirectorySeparatorChar
if (-not $outputRoot.StartsWith($artifactRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Keep disposable showcase output under this checkout''s ignored artifacts directory.'
}
if ((Test-Path -LiteralPath $outputRoot) -and -not $ReuseRaw) {
    throw 'Output already exists. Choose a new output directory, or use -ReuseRaw to repackage existing renders.'
}
$rawRoot = Join-Path $outputRoot 'raw'
if (-not $ReuseRaw) {
    $toolchain = Get-ChildItem -LiteralPath $ToolchainDirectory -Directory -Filter 'llvm-mingw-*-ucrt-x86_64' |
        Sort-Object Name -Descending | Select-Object -First 1
    if (-not $toolchain) { throw 'The portable LLVM-MinGW toolchain is missing.' }
    $cxx = Join-Path $toolchain.FullName 'bin/i686-w64-mingw32-clang++.exe'
    $null = New-Item -ItemType Directory -Force -Path 'build', $outputRoot
    & $cxx '-std=c++20' '-O2' '-static' '-municode' '-Isrc' '-include' 'tools/feature-showcase-clock.hpp' 'tools/render-feature-showcase.cpp' `
        'src/grass.cpp' 'src/effects.cpp' 'src/scene_fx.cpp' '-ld3d9' '-ldxguid' '-lbcrypt' '-lgdiplus' '-lgdi32' `
        '-o' 'build/feature-showcase.exe'
    if ($LASTEXITCODE) { throw 'Feature showcase compilation failed.' }
    $sourceFiles = @('tools/render-feature-showcase.cpp', 'tools/feature-showcase-clock.hpp', 'src/version.hpp',
        'src/grass.cpp', 'src/effects.cpp', 'src/scene_fx.cpp', 'src/material.hpp', 'src/trees.hpp', 'src/sfx.hpp',
        'src/refinement_trail.hpp', 'src/actor_visibility.hpp', 'src/puddles.hpp', 'src/settings.hpp', 'src/scene_settings.inc',
        'src/generated/trees.hpp', 'src/generated/material_ps.hpp', 'src/generated/material_bark_ps.hpp', 'src/generated/material_snow_ps.hpp',
        'src/generated/grass_capture_vs.hpp', 'src/generated/grass_capture_ps.hpp',
        'src/generated/grass_blade_vs.hpp', 'src/generated/grass_blade_ps.hpp', 'src/generated/scene_shaders.hpp', 'src/generated/sfx_shaders.hpp')
    $buildManifest = [ordered]@{
        sourceRevision = (git rev-parse HEAD).Trim(); version = ((Get-Content src/version.hpp -Raw) -split '"')[1]
        sources = @($sourceFiles | ForEach-Object { @{file = $_; sha256 = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash} })
    }
    $buildManifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $outputRoot 'renderer-build.json') -Encoding UTF8
    & './build/feature-showcase.exe' $artworkRoot $rawRoot | Tee-Object -FilePath (Join-Path $outputRoot 'render.log')
    if ($LASTEXITCODE) { throw 'Feature showcase rendering or pixel checks failed.' }
}
$catalog = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'feature-showcase.json') -Raw | ConvertFrom-Json
$buildManifest = Get-Content -LiteralPath (Join-Path $outputRoot 'renderer-build.json') -Raw | ConvertFrom-Json
$checks = @(Get-Content -LiteralPath (Join-Path $rawRoot 'pixel-checks.json') -Raw | ConvertFrom-Json)
if ($checks.Count -ne 19 -or @($checks | Where-Object { $_.changedPixels -le 100 }).Count) {
    throw 'Expected 19 verified, nonempty off/on comparisons.'
}

Add-Type -AssemblyName System.Drawing
$null = New-Item -ItemType Directory -Force -Path (Join-Path $outputRoot 'pairs'), (Join-Path $outputRoot 'splits')
$logoPath = Join-Path $repoRoot 'assets/branding/approved/PNG/Dark/fenb-dark-horizontal-transparent.png'
$logo = [Drawing.Image]::FromFile($logoPath)
Copy-Item -LiteralPath $logoPath -Destination (Join-Path $outputRoot 'fenb-logo.png')
$labelFont = [Drawing.Font]::new('Arial', 29, [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
$captionFont = [Drawing.Font]::new('Arial', 22, [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
$smallFont = [Drawing.Font]::new('Arial', 14, [Drawing.FontStyle]::Regular, [Drawing.GraphicsUnit]::Pixel)
$white = [Drawing.SolidBrush]::new([Drawing.Color]::White)
$shadow = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(210, 0, 0, 0))

function Canvas([int]$width, [int]$height) {
    $bitmap = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = [Drawing.Graphics]::FromImage($bitmap)
    $g.Clear([Drawing.ColorTranslator]::FromHtml('#10121a'))
    $g.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    return @{ bitmap = $bitmap; g = $g }
}
function Text($g, [string]$value, $font, [single]$x, [single]$y) {
    $g.DrawString($value, $font, $shadow, $x + 2, $y + 2)
    $g.DrawString($value, $font, $white, $x, $y)
}
function Brand($g, [int]$y) {
    # Preserve the approved horizontal artwork's 3.2:1 aspect ratio.
    $g.DrawImage($logo, [Drawing.Rectangle]::new(1330, $y + 18, 240, 75))
}
function SaveCanvas($canvas, [string]$path) {
    try { $canvas.bitmap.Save($path, [Drawing.Imaging.ImageFormat]::Png) }
    finally { $canvas.g.Dispose(); $canvas.bitmap.Dispose() }
}
function Escape([string]$value) { return [Net.WebUtility]::HtmlEncode($value) }

$manifestCards = @()
$htmlCards = @()
try {
    foreach ($card in $catalog.cards) {
        $beforePath = Join-Path $rawRoot ($card.before + '.png')
        $afterPath = Join-Path $rawRoot ($card.after + '.png')
        $before = [Drawing.Image]::FromFile($beforePath)
        $after = [Drawing.Image]::FromFile($afterPath)
        try {
            if ($before.Width -ne 1600 -or $before.Height -ne 900 -or $after.Width -ne 1600 -or $after.Height -ne 900) {
                throw "Unexpected dimensions: $($card.id)"
            }
            $beforeLabel = if ($card.beforeLabel) { $card.beforeLabel } else { 'ORIGINAL' }
            $afterLabel = if ($card.afterLabel) { $card.afterLabel } else { 'FENB' }
            $pair = Canvas 1600 1864
            $pair.g.DrawImageUnscaled($before, 0, 0)
            $pair.g.DrawImageUnscaled($after, 0, 900)
            Text $pair.g $beforeLabel $labelFont 24 23
            Text $pair.g $afterLabel $labelFont 24 923
            Brand $pair.g 900
            Text $pair.g $card.title $captionFont 24 1810
            Text $pair.g 'Standalone DX9 render / identical scene and camera' $smallFont 24 1838
            SaveCanvas $pair (Join-Path $outputRoot ('pairs/' + $card.id + '.png'))

            $split = Canvas 1600 964
            $split.g.DrawImageUnscaled($after, 0, 0)
            $split.g.DrawImage($before, [Drawing.Rectangle]::new(0, 0, 800, 900), 0, 0, 800, 900, [Drawing.GraphicsUnit]::Pixel)
            $line = [Drawing.Pen]::new([Drawing.Color]::FromArgb(220, 210, 189, 248), 2)
            try { $split.g.DrawLine($line, 800, 0, 800, 900) } finally { $line.Dispose() }
            Text $split.g $beforeLabel $labelFont 24 23
            Brand $split.g 0
            Text $split.g $card.title $captionFont 24 910
            Text $split.g 'Standalone DX9 render / left: baseline / right: selected effect' $smallFont 24 938
            SaveCanvas $split (Join-Path $outputRoot ('splits/' + $card.id + '.png'))
        } finally { $before.Dispose(); $after.Dispose() }

        $variants = @()
        foreach ($variant in $card.variants) {
            if (-not (Test-Path -LiteralPath (Join-Path $rawRoot ($variant.file + '.png')))) { throw "Missing variant: $($variant.file)" }
            $variants += "<a href=`"raw/$($variant.file).png`" target=`"_blank`"><img loading=`"lazy`" src=`"raw/$($variant.file).png`" alt=`"$(Escape $variant.label)`"><span>$(Escape $variant.label)</span></a>"
        }
        $htmlCards += @"
<article id="$($card.id)" data-group="$(Escape $card.group)">
  <div class="card-head"><span class="eyebrow">$(Escape $card.group)</span><h2>$(Escape $card.title)</h2><p>$(Escape $card.description)</p></div>
  <div class="compare" style="--position:50%">
    <img class="after" loading="lazy" src="raw/$($card.after).png" alt="$(Escape $card.title) / $(Escape $afterLabel)">
    <img class="before" loading="lazy" src="raw/$($card.before).png" alt="$(Escape $card.title) / $(Escape $beforeLabel)">
    <span class="tag left">$(Escape $beforeLabel)</span><span class="tag right">$(Escape $afterLabel)</span><div class="divider"></div>
    <input type="range" min="0" max="100" value="50" aria-label="Comparison divider: $(Escape $card.title)">
  </div>
  <div class="card-links"><a href="pairs/$($card.id).png" target="_blank">Stacked PNG ↗</a><a href="splits/$($card.id).png" target="_blank">Split PNG ↗</a><a href="raw/$($card.before).png" target="_blank">Before ↗</a><a href="raw/$($card.after).png" target="_blank">After ↗</a></div>
  <div class="variants">$($variants -join "`n")</div>
</article>
"@
        $manifestCards += [ordered]@{
            id = $card.id; title = $card.title; description = $card.description
            before = "raw/$($card.before).png"; after = "raw/$($card.after).png"
            beforeSha256 = (Get-FileHash -LiteralPath $beforePath -Algorithm SHA256).Hash
            afterSha256 = (Get-FileHash -LiteralPath $afterPath -Algorithm SHA256).Hash
            implementation = $card.implementation; pair = "pairs/$($card.id).png"; split = "splits/$($card.id).png"
        }
    }

    $overview = Canvas 1600 2210
    Text $overview.g 'FENB / A day of changes' $labelFont 24 18
    Text $overview.g '18 November 2026 / standalone renders / original above, selected effect below' $smallFont 24 58
    $i = 0
    foreach ($card in $catalog.cards) {
        $pair = [Drawing.Image]::FromFile((Join-Path $outputRoot ('pairs/' + $card.id + '.png')))
        try {
            $x = ($i % 4) * 400; $y = [math]::Floor($i / 4) * 525 + 100
            $overview.g.DrawImage($pair, [Drawing.Rectangle]::new($x + 8, $y, 384, 447))
            Text $overview.g $card.title $smallFont ($x + 10) ($y + 457)
        } finally { $pair.Dispose() }
        $i++
    }
    SaveCanvas $overview (Join-Path $outputRoot '00-overview.png')
} finally {
    foreach ($resource in @($logo, $labelFont, $captionFont, $smallFont, $white, $shadow)) { $resource.Dispose() }
}

$filters = @('All') + @($catalog.cards.group | Select-Object -Unique)
$filterButtons = ($filters | ForEach-Object { '<button type="button" data-filter="' + (Escape $_) + '">' + (Escape $_) + '</button>' }) -join ''
$html = @"
<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>FENB / 18 November feature comparisons</title>
<style>
:root{color-scheme:dark;font-family:Segoe UI,Arial,sans-serif;background:#0e1017;color:#f0edf8}*{box-sizing:border-box}body{margin:0}main{max-width:1320px;margin:auto;padding:32px 24px 64px}header{padding:32px 0 24px;border-bottom:1px solid #302b40}header img{width:224px;max-width:60%}.eyebrow{color:#b49adb;text-transform:uppercase;letter-spacing:.14em;font-size:11px;font-weight:700}h1{font-size:clamp(32px,5vw,58px);font-weight:600;letter-spacing:-.045em;line-height:1.08;margin:14px 0}header p{max-width:880px;color:#b5b2c1;font-size:16px;line-height:1.6}.meta{display:flex;gap:10px;flex-wrap:wrap;color:#c3b7d9;font-size:12px}.meta span{border:1px solid #3a304b;border-radius:5px;padding:7px 11px}nav{display:flex;gap:8px;flex-wrap:wrap;margin:24px 0}button{background:#1c1a28;border:1px solid #363044;color:#d4cddd;border-radius:6px;padding:9px 14px;cursor:pointer}button.active{background:#7351a6;color:white;border-color:#9978c7}article{border:1px solid #2a2737;border-radius:10px;background:#15141f;margin:0 0 28px;overflow:hidden}.card-head{padding:22px 24px 16px}h2{font-size:24px;letter-spacing:-.025em;margin:8px 0}p{line-height:1.6}.card-head p{color:#b6b2c1;margin:8px 0 0;max-width:1000px;font-size:14px}.compare{position:relative;aspect-ratio:16/9;background:#171722;overflow:hidden}.compare>img{position:absolute;inset:0;width:100%;height:100%;object-fit:contain}.compare .before{clip-path:inset(0 calc(100% - var(--position)) 0 0)}.tag{position:absolute;top:16px;font-size:clamp(10px,1.3vw,16px);font-weight:700;letter-spacing:.08em;background:#111321bb;padding:8px 11px;border-radius:4px;pointer-events:none}.left{left:16px}.right{right:16px}.divider{position:absolute;left:var(--position);top:0;bottom:0;width:2px;background:#d0b8f0;pointer-events:none}.divider:after{content:'↔';position:absolute;left:-19px;top:48%;width:40px;height:40px;background:#171321cc;border:1px solid #d0b8f0;border-radius:50%;text-align:center;line-height:36px}.compare input{position:absolute;inset:0;width:100%;height:100%;opacity:0;cursor:ew-resize;margin:0}.compare:focus-within{outline:3px solid #ba9de5;outline-offset:-3px}.card-links{padding:13px 22px;display:flex;gap:24px;flex-wrap:wrap;border-top:1px solid #2c2838}a{color:#c4a7ec;text-decoration:none;font-size:13px}a:hover{text-decoration:underline}.variants{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:12px;padding:0 22px 18px}.variants:empty{display:none}.variants a{display:block;background:#1c1a27;border:1px solid #302a40;border-radius:6px;overflow:hidden}.variants img{display:block;width:100%;aspect-ratio:16/9;object-fit:cover}.variants span{display:block;padding:9px 10px}footer{color:#9691a5;font-size:13px;line-height:1.7}footer a{margin-right:20px}[hidden]{display:none!important}@media(max-width:650px){main{padding:16px 12px}.card-head{padding:16px}.card-links{gap:14px}.tag{top:8px;padding:5px}.left{left:8px}.right{right:8px}}
</style>
<main><header><img src="fenb-logo.png" alt="FENB"><div class="eyebrow">18 November 2026 · feature study</div><h1>A day of changes.<br>See every difference.</h1><p>$(Escape $catalog.description)</p><div class="meta"><span>16 feature cards</span><span>1600 × 900 source renders</span><span>Drag each divider · click PNG to export</span></div></header>
<nav aria-label="Feature categories">$filterButtons</nav>
$($htmlCards -join "`n")
<footer><p>Each pair isolates the named feature. “Original” means this standalone scene with that feature disabled. Scenery is procedural, the artwork is read from a local client, and the effects use FENB's actual implementations. Synthetic actor fixtures demonstrate visibility decisions only. Animation phases, camera, source geometry and lighting stay fixed. The rainy fill sequence advances only the wetness clock.</p><a href="00-overview.png" target="_blank">All comparisons / overview ↗</a><a href="manifest.json">Render manifest ↗</a></footer></main>
<script>
document.querySelectorAll('.compare input').forEach(input=>input.addEventListener('input',()=>input.parentElement.style.setProperty('--position',input.value+'%')));
const buttons=[...document.querySelectorAll('[data-filter]')];buttons[0].classList.add('active');buttons.forEach(button=>button.addEventListener('click',()=>{buttons.forEach(b=>b.classList.toggle('active',b===button));document.querySelectorAll('article').forEach(card=>card.hidden=button.dataset.filter!=='All'&&card.dataset.group!==button.dataset.filter)}));
</script></html>
"@
[IO.File]::WriteAllText((Join-Path $outputRoot 'index.html'), $html, [Text.UTF8Encoding]::new($false))
$artworkFiles = @('World/Texture/ground_lawn001.dds', 'World/Texture/ground_n_ice02.dds', 'World/Texture/ground_clay001.dds',
    'Model/Texture/Obj_MaCoPrTr13-02.dds', 'Model/Texture/Obj_AzriaTree01-B.dds', 'Model/Texture/Obj_MaCoPrTr01-01.dds',
    'Model/Texture/Obj_AzriaTree01-1.dds', 'SFX/Texture/sfx_EffectEnchant.dds')
$manifest = [ordered]@{
    date = $catalog.date; sourceRevision = $buildManifest.sourceRevision; version = $buildManifest.version
    renderer = 'tools/render-feature-showcase.cpp'; sources = $buildManifest.sources
    description = $catalog.description; resolution = @{ width = 1600; height = 900 }; randomSeed = 2311786; animationClockMilliseconds = 15000
    artwork = @($artworkFiles | ForEach-Object { @{ file = $_; sha256 = (Get-FileHash -LiteralPath (Join-Path $artworkRoot $_) -Algorithm SHA256).Hash } })
    processing = 'Lossless GPU readback to PNG; labels and approved logo added to exported comparison panels. No color grading or generated effect pixels.'
    fixtures = 'Procedural scenery; synthetic actor layouts and paving/card identities. Tree and snow material textures use normal catalog recognition. Grass, material, tree, SFX, puddle and visibility implementations are compiled from src/.'
    cards = $manifestCards; pixelChecks = $checks
}
$manifest | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $outputRoot 'manifest.json') -Encoding UTF8
Write-Output "Created $($catalog.cards.Count) comparison cards, paired/split PNGs, overview and offline gallery: $outputRoot"
