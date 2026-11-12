param(
    [Parameter(Mandatory = $true)][string]$CaptureDirectory,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../artifacts/capture-comparisons-borderless')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$sourceRoot = (Resolve-Path -LiteralPath $CaptureDirectory).Path
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
if ($outputRoot -eq $sourceRoot -or $outputRoot.StartsWith($sourceRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Choose an output directory outside the source captures.'
}
if (Test-Path -LiteralPath $outputRoot) { throw 'Output directory already exists. Choose a new destination to preserve existing exports.' }

# The same integer crop removes HUD, shortcut bar, quest marker and timestamp
# from all supplied 1920 x 1080 captures. Scene pixels receive no grading.
$crop = [Drawing.Rectangle]::new(224, 64, 1696, 928)
$scenes = @(
    @{ id = '01-riverbank'; title = 'River'; before = '3no.jpg'; after = '3yes.jpg'; label = 'FENB' },
    @{ id = '02-lake'; title = 'Lake'; before = '2no.jpg'; after = '2yes.jpg'; label = 'FENB' },
    @{ id = '03-town'; title = 'Town'; before = '1no.jpg'; after = '1yes.jpg'; label = 'FENB' },
    @{ id = '04-town-extra'; title = 'Town / extra'; before = '1no.jpg'; after = '1add.jpg'; label = 'FENB / EXTRA' }
)
$hashes = @{}
foreach ($name in ($scenes.before + $scenes.after | Sort-Object -Unique)) {
    $file = Join-Path $sourceRoot $name
    $hashes[$name] = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
    $image = [Drawing.Image]::FromFile($file)
    try { if ($image.Width -ne 1920 -or $image.Height -ne 1080) { throw "Unexpected capture dimensions: $name" } }
    finally { $image.Dispose() }
}

$logoPath = Join-Path $PSScriptRoot '../assets/branding/approved/PNG/Dark/fenb-dark-horizontal-transparent.png'
$logo = [Drawing.Image]::FromFile([IO.Path]::GetFullPath($logoPath))
$null = New-Item -ItemType Directory -Path $outputRoot
$null = New-Item -ItemType Directory -Path (Join-Path $outputRoot 'crops')
$null = New-Item -ItemType Directory -Path (Join-Path $outputRoot 'web')
$font = [Drawing.Font]::new('Arial', 32, [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
if ($font.Name -ne 'Arial' -or -not $font.Bold) { throw 'Arial Bold is required; do not silently substitute a different typeface.' }
$ink = '#FFFFFF'
function Text($g, $value, $x, $y, $color = $ink) {
    $brush = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($color))
    $shadow = [Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(190, 0, 0, 0))
    try {
        $g.DrawString($value, $font, $shadow, [single]($x + 2), [single]($y + 2))
        $g.DrawString($value, $font, $brush, [single]$x, [single]$y)
    } finally { $brush.Dispose(); $shadow.Dispose() }
}
function Canvas($width, $height) {
    $bitmap = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.Clear([Drawing.ColorTranslator]::FromHtml('#111111'))
    $graphics.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    return @{ bitmap = $bitmap; g = $graphics }
}
function Brand($g, $y, $extra) {
    $g.DrawImage($logo, [Drawing.Rectangle]::new(1448, ($y + 16), 224, 70))
    if ($extra) { Text $g 'EXTRA' 1556 ($y + 88) }
}
function SaveCanvas($canvas, $name) {
    try { $canvas.bitmap.Save((Join-Path $outputRoot $name), [Drawing.Imaging.ImageFormat]::Png) }
    finally { $canvas.g.Dispose(); $canvas.bitmap.Dispose() }
}
function DrawCrop($g, $image, $x, $y) {
    $g.DrawImageUnscaled($image, [int]$x, [int]$y)
}
$exported = @()
try {
    foreach ($name in $hashes.Keys) {
        $source = [Drawing.Bitmap]::new((Join-Path $sourceRoot $name))
        try {
            $cropped = $source.Clone($crop, [Drawing.Imaging.PixelFormat]::Format24bppRgb)
            try { $cropped.Save((Join-Path $outputRoot ('crops/' + [IO.Path]::GetFileNameWithoutExtension($name) + '.png')), [Drawing.Imaging.ImageFormat]::Png) }
            finally { $cropped.Dispose() }
        } finally { $source.Dispose() }
    }
    Copy-Item -LiteralPath $logoPath -Destination (Join-Path $outputRoot 'web/fenb-logo.png')

    foreach ($scene in $scenes) {
        $beforeFile = 'crops/' + [IO.Path]::GetFileNameWithoutExtension($scene.before) + '.png'
        $afterFile = 'crops/' + [IO.Path]::GetFileNameWithoutExtension($scene.after) + '.png'
        $before = [Drawing.Bitmap]::new((Join-Path $outputRoot $beforeFile))
        $after = [Drawing.Bitmap]::new((Join-Path $outputRoot $afterFile))
        try {
            $split = Canvas 1696 928
            DrawCrop $split.g $after 0 0
            # Copy the exact left half of the original over the processed frame.
            $split.g.DrawImage($before, [Drawing.Rectangle]::new(0, 0, 848, 928), 0, 0, 848, 928, [Drawing.GraphicsUnit]::Pixel)
            Text $split.g 'ORIGINAL' 24 22
            Brand $split.g 0 ($scene.after -eq '1add.jpg')
            SaveCanvas $split ($scene.id + '-split.png')

            $pair = Canvas 1696 1856
            DrawCrop $pair.g $before 0 0
            DrawCrop $pair.g $after 0 928
            Text $pair.g 'ORIGINAL' 24 22
            Brand $pair.g 928 ($scene.after -eq '1add.jpg')
            SaveCanvas $pair ($scene.id + '-pair.png')
        } finally { $before.Dispose(); $after.Dispose() }
        $exported += @{
            id = $scene.id; title = $scene.title; before = $beforeFile; after = $afterFile
            beforeSource = $scene.before; afterSource = $scene.after; label = $scene.label
        }
    }

    Copy-Item -LiteralPath (Join-Path $outputRoot '01-riverbank-split.png') -Destination (Join-Path $outputRoot '00-cover.png')

    $cards = foreach ($entry in $exported) {
        $extraTag = if ($entry.afterSource -eq '1add.jpg') { '<span class="extra">EXTRA</span>' } else { '' }
@"
<section class="scene" aria-labelledby="$($entry.id)">
  <div class="scene-heading"><h2 id="$($entry.id)">$($entry.title)</h2><span>ORIGINAL <b>/</b> $($entry.label)</span></div>
  <div class="comparison" style="--position:50%">
    <div class="after-side"><img class="capture" src="$($entry.after)" alt="$($entry.title) with $($entry.label)" width="1696" height="928"><img class="scene-logo" src="web/fenb-logo.png" alt="FENB">$extraTag</div>
    <div class="before-side"><img class="capture" src="$($entry.before)" alt="$($entry.title) in the original game" width="1696" height="928"><span class="original">ORIGINAL</span></div>
    <input type="range" min="0" max="100" value="50" aria-label="Compare original and $($entry.label): $($entry.title)" aria-valuetext="50% original, 50% FENB">
  </div>
  <div class="controls"><button type="button" data-position="100">Original only</button><button type="button" data-position="50">Compare</button><button type="button" data-position="0">FENB only</button><a href="$($entry.id)-pair.png">Full comparison PNG &nearr;</a></div>
</section>
"@
    }
    $html = @'
<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>FENB comparisons</title>
<style>
:root{color-scheme:dark;font-family:Arial,Helvetica,sans-serif;background:#111;color:#fff}
*{box-sizing:border-box}body{margin:0}main{max-width:1744px;padding:20px 24px 32px;margin:auto}
header{display:flex;align-items:center;justify-content:space-between;gap:24px;margin-bottom:24px}
header img{width:224px;height:70px}h1{font-size:22px;font-weight:800;margin:0}
p{color:#aaa;line-height:1.5;margin:0}a{color:#c3aaff;text-decoration:none}
.intro{display:flex;justify-content:space-between;gap:20px;margin:0 0 36px;font-size:14px}
.scene{margin:0 0 36px}.scene-heading{display:flex;align-items:center;justify-content:space-between;gap:20px;margin-bottom:12px}
.scene-heading h2{font-size:24px;font-weight:800;margin:0}.scene-heading span{font-size:16px;font-weight:700}
.scene-heading b{color:#888;padding:0 10px}
.comparison{position:relative;aspect-ratio:1696/928;overflow:hidden;background:#111}
.before-side,.after-side{position:absolute;inset:0;pointer-events:none}
.capture{display:block;position:absolute;inset:0;width:100%;height:100%;max-width:none;object-fit:fill}
.before-side{clip-path:inset(0 calc(100% - var(--position)) 0 0)}
.after-side{clip-path:inset(0 0 0 var(--position))}
.original,.extra{position:absolute;font-size:32px;font-weight:700;text-shadow:2px 2px 2px #000b}
.original{top:22px;left:24px}.extra{top:88px;right:24px}
.scene-logo{position:absolute;right:24px;top:16px;width:224px;height:auto}
.comparison input{appearance:none;-webkit-appearance:none;position:absolute;inset:0;width:100%;height:100%;margin:0;opacity:0;cursor:ew-resize;touch-action:pan-y}
.comparison:focus-within{outline:3px solid #c3aaff;outline-offset:4px}
.controls{display:flex;gap:8px;align-items:center;margin-top:12px;flex-wrap:wrap}
button{background:#222;border:0;padding:10px 16px;color:#fff;font:700 14px Arial,Helvetica,sans-serif;cursor:pointer}
button:hover,button:focus-visible{background:#444}.controls a{margin-left:auto;font-size:14px;font-weight:700}
.note{font-size:13px}
@media(max-width:680px){main{padding:16px 12px}header img{width:160px;height:50px}h1{font-size:16px}.intro{display:block}.intro a{display:inline-block;margin-top:8px}.scene-heading h2{font-size:20px}.scene-heading span{font-size:13px}.original,.extra{font-size:16px}.original{left:10px;top:12px}.scene-logo{width:128px;right:10px;top:6px}.extra{top:46px;right:10px}.controls a{margin:8px 0 0;width:100%}}
</style></head><body><main>
<header><img src="web/fenb-logo.png" alt="FENB"><h1>ORIGINAL / FENB</h1></header>
<div class="intro"><p>Drag to compare. Arrow keys also work.</p><a href="00-cover.png">Comparison PNG &nearr;</a></div>
<!-- SCENES -->
<p class="note">Same crop. Original screenshot colors. Captures were taken moments apart; animated objects may differ. Extra is an additional captured FENB variant.</p>
</main><script>
for (const section of document.querySelectorAll('.scene')) {
  const frame = section.querySelector('.comparison'), range = frame.querySelector('input');
  const update = () => {
    const position = Number(range.value);
    frame.style.setProperty('--position', `${position}%`);
    range.setAttribute('aria-valuetext', `${position}% original, ${100 - position}% FENB`);
  };
  range.addEventListener('input', update);
  for (const button of section.querySelectorAll('[data-position]')) button.addEventListener('click', () => { range.value = button.dataset.position; update(); });
}
</script></body></html>
'@
    [IO.File]::WriteAllText((Join-Path $outputRoot 'gallery.html'), $html.Replace('<!-- SCENES -->', ($cards -join "`n")), [Text.UTF8Encoding]::new($false))
    $manifest = @{
        crop = @{ x = $crop.X; y = $crop.Y; width = $crop.Width; height = $crop.Height }
        sourceDimensions = @{ width = 1920; height = 1080 }
        sourceSha256 = $hashes; scenes = $exported
        processing = 'Identical integer crop only; PNG crops preserve decoded JPEG pixels at native resolution. No additional grading, sharpening or generated scene content.'
        branding = 'Approved Refined Bloom, dark horizontal transparent PNG'
        typography = 'Arial Bold, 32px; verified family and weight before rendering'
        layout = 'Borderless native-resolution scenes; original label on original, approved logo only over FENB. No panels, padding or divider lines.'
    }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'manifest.json') -Encoding UTF8
    @'
FENB / CAPTURE COMPARISONS

Open gallery.html in a browser for interactive comparisons (works offline).
Keep gallery.html, crops/ and web/ together when copying the gallery.

00-cover.png          River comparison, ready to share.
01-03-*-split.png     Half-original / half-FENB image, matched framing.
01-03-*-pair.png      Full original above full FENB, native crop resolution.
04-town-extra-*       Original compared with the additional captured variant.
crops/               Clean, lossless PNG crops without layout or labels.
manifest.json        Source hashes, crop coordinates and pair mapping.

Source mapping: no = original; yes = FENB; add = additional FENB variant.
Original captures remain unchanged. No added grading, contrast, sharpening,
retouching or generated scene content. Animated objects may differ between
captures. Native crop: x=224, y=64, width=1696, height=928.

The logo is the approved FENB Refined Bloom artwork.
All PNGs are borderless. The logo appears only on the FENB scene.
PNG boards are ready to share; nothing has been published automatically.
'@ | Set-Content -LiteralPath (Join-Path $outputRoot 'README.txt') -Encoding UTF8

    foreach ($name in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $sourceRoot $name) -Algorithm SHA256).Hash -ne $hashes[$name]) { throw "Source capture changed during export: $name" }
    }
    Write-Output "Exported comparisons to $outputRoot"
} finally {
    $logo.Dispose()
    $font.Dispose()
}
