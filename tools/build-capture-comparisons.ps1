param(
    [Parameter(Mandatory = $true)][string]$CaptureDirectory,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../artifacts/capture-comparisons')
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
    @{ id = '01-riverbank'; title = 'By the river'; before = '3no.jpg'; after = '3yes.jpg'; label = 'FENB'; detail = 'Water, reflections and distant scenery' },
    @{ id = '02-lake'; title = 'Across the lake'; before = '2no.jpg'; after = '2yes.jpg'; label = 'FENB'; detail = 'Water and landscape' },
    @{ id = '03-town'; title = 'By the windmill'; before = '1no.jpg'; after = '1yes.jpg'; label = 'FENB'; detail = 'Town, water and distant scenery' },
    @{ id = '04-town-extra'; title = 'Another atmosphere'; before = '1no.jpg'; after = '1add.jpg'; label = 'FENB / EXTRA'; detail = 'Additional captured variant' }
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
$fonts = @{}
foreach ($size in @(16, 18, 20, 22, 28, 38, 44)) {
    $fonts[$size] = [Drawing.Font]::new('Segoe UI', $size, [Drawing.FontStyle]::Regular, [Drawing.GraphicsUnit]::Pixel)
}
$ink = '#F2F0FA'; $muted = '#ABA6BE'; $violet = '#B99AFF'
function Text($g, $value, $x, $y, $size = 22, $color = $ink) {
    $brush = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($color))
    try { $g.DrawString($value, $fonts[$size], $brush, [single]$x, [single]$y) }
    finally { $brush.Dispose() }
}
function Fill($g, $x, $y, $w, $h, $color) {
    $brush = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($color))
    try { $g.FillRectangle($brush, [int]$x, [int]$y, [int]$w, [int]$h) }
    finally { $brush.Dispose() }
}
function Canvas($width, $height) {
    $bitmap = [Drawing.Bitmap]::new($width, $height, [Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $graphics = [Drawing.Graphics]::FromImage($bitmap)
    $graphics.Clear([Drawing.ColorTranslator]::FromHtml('#0C0B12'))
    $graphics.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    return @{ bitmap = $bitmap; g = $graphics }
}
function Header($g, $title, $subtitle) {
    $g.DrawImage($logo, [Drawing.Rectangle]::new(40, 14, 310, 97))
    Text $g $title 396 29 38
    Text $g $subtitle 399 82 20 $muted
    Fill $g 52 126 1696 1 '#302A42'
}
function SaveCanvas($canvas, $name) {
    try { $canvas.bitmap.Save((Join-Path $outputRoot $name), [Drawing.Imaging.ImageFormat]::Png) }
    finally { $canvas.g.Dispose(); $canvas.bitmap.Dispose() }
}
function DrawCrop($g, $image, $x, $y, $width = 1696, $height = 928) {
    if ($width -eq 1696 -and $height -eq 928) { $g.DrawImageUnscaled($image, [int]$x, [int]$y) }
    else { $g.DrawImage($image, [Drawing.Rectangle]::new($x, $y, $width, $height)) }
}
function Footer($g, $y, $extra = '') {
    Text $g ('IN-GAME CAPTURES  /  Matching crop. No added color adjustments.' + $extra) 52 $y 18 $muted
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
            $split = Canvas 1800 1152
            Header $split.g $scene.title $scene.detail
            Text $split.g 'ORIGINAL' 52 139 20
            Text $split.g $scene.label 1172 139 20 $violet
            DrawCrop $split.g $after 52 176
            # Copy the exact left half of the original over the processed frame.
            $split.g.DrawImage($before, [Drawing.Rectangle]::new(52, 176, 848, 928), 0, 0, 848, 928, [Drawing.GraphicsUnit]::Pixel)
            Fill $split.g 899 176 2 928 '#D9C9FF'
            Footer $split.g 1119
            SaveCanvas $split ($scene.id + '-split.png')

            $pair = Canvas 1800 2176
            Header $pair.g $scene.title $scene.detail
            Text $pair.g 'ORIGINAL' 52 140 22
            DrawCrop $pair.g $before 52 180
            Text $pair.g $scene.label 52 1132 22 $violet
            DrawCrop $pair.g $after 52 1172
            Footer $pair.g 2128
            SaveCanvas $pair ($scene.id + '-pair.png')
        } finally { $before.Dispose(); $after.Dispose() }
        $exported += @{
            id = $scene.id; title = $scene.title; before = $beforeFile; after = $afterFile
            beforeSource = $scene.before; afterSource = $scene.after; label = $scene.label
        }
    }

    $overview = Canvas 1800 1816
    Header $overview.g 'See the difference' 'FENB - a graphics mod for Flyff, inspired by ENBSeries'
    Text $overview.g 'ORIGINAL' 52 145 22
    Text $overview.g 'FENB' 912 145 22 $violet
    for ($index = 0; $index -lt 3; $index++) {
        $scene = $scenes[$index]; $entry = $exported[$index]; $y = 190 + 526 * $index
        $before = [Drawing.Bitmap]::new((Join-Path $outputRoot $entry.before))
        $after = [Drawing.Bitmap]::new((Join-Path $outputRoot $entry.after))
        try {
            DrawCrop $overview.g $before 52 $y 836 457
            DrawCrop $overview.g $after 912 $y 836 457
        } finally { $before.Dispose(); $after.Dispose() }
        Text $overview.g ('0' + ($index + 1) + '  /  ' + $scene.title) 52 ($y + 469) 22
    }
    Footer $overview.g 1780
    SaveCanvas $overview '00-overview.png'

    $cards = foreach ($entry in $exported) {
@"
<section class="scene" aria-labelledby="$($entry.id)">
  <div class="scene-heading"><h2 id="$($entry.id)">$($entry.title)</h2><span>ORIGINAL <b>/</b> $($entry.label)</span></div>
  <div class="comparison" style="--position:50%">
    <img class="after" src="$($entry.after)" alt="$($entry.title) with $($entry.label)" width="1696" height="928">
    <img class="before" src="$($entry.before)" alt="$($entry.title) in the original game" width="1696" height="928">
    <span class="tag original">Original</span><span class="tag processed">$($entry.label)</span>
    <span class="divider" aria-hidden="true"><i>&lsaquo; &rsaquo;</i></span>
    <input type="range" min="0" max="100" value="50" aria-label="Compare original and $($entry.label): $($entry.title)" aria-valuetext="50% original, 50% FENB">
  </div>
  <div class="controls"><button type="button" data-position="100">Original only</button><button type="button" data-position="50">Compare</button><button type="button" data-position="0">FENB only</button><a href="$($entry.id)-pair.png">Full comparison PNG &nearr;</a></div>
</section>
"@
    }
    $html = @'
<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>FENB - See the difference</title>
<style>
:root{color-scheme:dark;font-family:Segoe UI,Arial,sans-serif;background:#0c0b12;color:#f2f0fa}*{box-sizing:border-box}body{margin:0}main{max-width:1460px;padding:34px 30px 44px;margin:auto}header{display:flex;align-items:center;gap:48px;border-bottom:1px solid #302a42;padding:0 0 26px}header img{width:260px;height:auto}h1{font-size:clamp(28px,4vw,48px);font-weight:500;margin:0 0 9px}p{color:#aba6be;line-height:1.6;margin:0}header p{max-width:680px}.intro{display:flex;justify-content:space-between;gap:20px;margin:24px 0 35px}.intro a,a{color:#c7aeff;text-decoration:none}.scene{margin:0 0 50px}.scene-heading{display:flex;align-items:center;justify-content:space-between;gap:20px;margin-bottom:16px}.scene-heading h2{font-size:25px;font-weight:500;margin:0}.scene-heading span{color:#aba6be;font-size:13px;letter-spacing:1.7px}.scene-heading b{color:#8063b0;padding:0 13px}.comparison{position:relative;aspect-ratio:1696/928;overflow:hidden;background:#17131f}.comparison img{display:block;position:absolute;inset:0;width:100%;height:100%;max-width:none;object-fit:fill}.before{clip-path:inset(0 calc(100% - var(--position)) 0 0)}.tag{position:absolute;top:20px;z-index:1;background:#100d19d9;padding:9px 14px;border:1px solid #ffffff24;border-radius:5px;font-size:13px;letter-spacing:1.1px;text-transform:uppercase;pointer-events:none}.original{left:20px}.processed{right:20px;color:#d9c9ff}.divider{position:absolute;top:0;bottom:0;left:var(--position);width:2px;background:#e0ceff;transform:translateX(-50%);pointer-events:none}.divider i{display:grid;place-items:center;position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);width:48px;height:48px;border-radius:50%;background:#100d19d9;border:2px solid #e0ceff;font-style:normal;font-size:25px;white-space:pre}.comparison input{appearance:none;-webkit-appearance:none;position:absolute;inset:0;width:100%;height:100%;margin:0;opacity:0;cursor:ew-resize;touch-action:pan-y}.comparison:focus-within{outline:3px solid #c7aeff;outline-offset:4px}.controls{display:flex;gap:8px;align-items:center;margin-top:13px;flex-wrap:wrap}button{background:#1c1728;border:1px solid #3f3356;border-radius:5px;padding:9px 15px;color:#eee8ff;font:inherit;font-size:13px;cursor:pointer}button:hover,button:focus-visible{background:#34264b}.controls a{margin-left:auto;font-size:13px}.note{padding:23px 0;border-top:1px solid #302a42;font-size:13px;max-width:100%}.note strong{color:#d8cbea;font-weight:500}@media(max-width:680px){main{padding:22px 14px}header{display:block}header img{width:205px;margin:0 0 20px}.intro{display:block;font-size:14px}.intro a{display:inline-block;margin-top:10px}.scene-heading{align-items:start;flex-direction:column;gap:8px}.tag{top:9px;font-size:10px;padding:6px 8px}.original{left:9px}.processed{right:9px}.controls a{margin:8px 0 0;width:100%}.divider i{width:34px;height:34px;font-size:19px}}
</style></head><body><main>
<header><img src="web/fenb-logo.png" alt="FENB"><div><h1>See the difference.</h1><p>A graphics mod for Flyff, inspired by ENBSeries.<br>Original game captures compared with FENB.</p></div></header>
<div class="intro"><p>Drag across each image to compare. Use the arrow keys for precise control.</p><a href="00-overview.png">View the overview PNG &nearr;</a></div>
<!-- SCENES -->
<p class="note"><strong>About these comparisons.</strong> Each pair uses the same crop and scale. HUD elements and timestamps were cropped out. No additional color, contrast, sharpening or generative edits were applied. Captures were taken moments apart; animated objects may differ. The fourth scene shows an additional captured FENB variant.</p>
</main><script>
for (const section of document.querySelectorAll('.scene')) {
  const frame = section.querySelector('.comparison'), range = frame.querySelector('input');
  const update = () => {
    const position = Number(range.value);
    frame.style.setProperty('--position', `${position}%`);
    range.setAttribute('aria-valuetext', `${position}% original, ${100 - position}% FENB`);
    frame.querySelector('.original').hidden = position === 0;
    frame.querySelector('.processed').hidden = position === 100;
    frame.querySelector('.divider').hidden = position === 0 || position === 100;
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
        processing = 'Identical integer crop only; PNG crops preserve decoded JPEG pixels. No additional grading, sharpening or generated scene content. Overview thumbnails are downscaled.'
        branding = 'Approved Refined Bloom, dark horizontal transparent PNG'
    }
    $manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $outputRoot 'manifest.json') -Encoding UTF8
    @'
FENB / CAPTURE COMPARISONS

Open gallery.html in a browser for interactive comparisons (works offline).
Keep gallery.html, crops/ and web/ together when copying the gallery.

00-overview.png       Three original / FENB comparisons on one sheet.
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
PNG boards are ready to share; nothing has been published automatically.
'@ | Set-Content -LiteralPath (Join-Path $outputRoot 'README.txt') -Encoding UTF8

    foreach ($name in $hashes.Keys) {
        if ((Get-FileHash -LiteralPath (Join-Path $sourceRoot $name) -Algorithm SHA256).Hash -ne $hashes[$name]) { throw "Source capture changed during export: $name" }
    }
    Write-Output "Exported comparisons to $outputRoot"
} finally {
    $logo.Dispose()
    foreach ($font in $fonts.Values) { $font.Dispose() }
}
