param(
    [Parameter(Mandatory = $true)][string]$CaptureDirectory,
    [string]$OutputDirectory = (Join-Path $PSScriptRoot '../artifacts/effect-showcase')
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$sourceRoot = (Resolve-Path -LiteralPath $CaptureDirectory).Path
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
if ($outputRoot -eq $sourceRoot -or $outputRoot.StartsWith($sourceRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Choose an output directory outside the source captures.'
}
if (Test-Path -LiteralPath $outputRoot) { throw 'Output directory already exists. Choose a new destination to preserve existing exports.' }

# Matched integer crops retain the effect and exclude HUD, minimap, NPC speech,
# shortcut bar and timestamp. PNG output preserves the decoded source pixels.
$weatherCrop = [Drawing.Rectangle]::new(240, 100, 1440, 810)
$lightingCrop = [Drawing.Rectangle]::new(500, 200, 960, 540)
$waterCrop = [Drawing.Rectangle]::new(40, 350, 860, 640)
$captures = @(
    @{ source = 'flyff00009.jpg'; output = 'color-weather.png'; crop = $weatherCrop },
    @{ source = 'flyff00005.jpg'; output = 'local-lighting-off.png'; crop = $lightingCrop },
    @{ source = 'flyff00006.jpg'; output = 'local-lighting-on.png'; crop = $lightingCrop },
    @{ source = 'flyff00011.jpg'; output = 'reflection-sky-only.png'; crop = $waterCrop },
    @{ source = 'flyff00012.jpg'; output = 'reflection-performance.png'; crop = $waterCrop },
    @{ source = 'flyff00013.jpg'; output = 'reflection-quality.png'; crop = $waterCrop },
    @{ source = 'flyff00014.jpg'; output = 'water-variation-01.png'; crop = $waterCrop },
    @{ source = 'flyff00015.jpg'; output = 'water-variation-02.png'; crop = $waterCrop },
    @{ source = 'flyff00016.jpg'; output = 'water-variation-03.png'; crop = $waterCrop }
)

# Validate every source before creating any exports.
$sourceHashes = @{}
foreach ($capture in $captures) {
    $file = Join-Path $sourceRoot $capture.source
    $sourceHashes[$capture.source] = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash
    $source = [Drawing.Bitmap]::new($file)
    try {
        if ($source.Width -ne 1920 -or $source.Height -ne 1080) { throw "Unexpected capture dimensions: $($capture.source)" }
    } finally { $source.Dispose() }
}

$null = New-Item -ItemType Directory -Path $outputRoot
$exports = foreach ($capture in $captures) {
    $source = [Drawing.Bitmap]::new((Join-Path $sourceRoot $capture.source))
    try {
        $cropped = $source.Clone($capture.crop, [Drawing.Imaging.PixelFormat]::Format24bppRgb)
        try { $cropped.Save((Join-Path $outputRoot $capture.output), [Drawing.Imaging.ImageFormat]::Png) }
        finally { $cropped.Dispose() }
    } finally { $source.Dispose() }
    [ordered]@{
        source = $capture.source
        sourceSha256 = $sourceHashes[$capture.source]
        output = $capture.output
        crop = [ordered]@{ x = $capture.crop.X; y = $capture.crop.Y; width = $capture.crop.Width; height = $capture.crop.Height }
    }
}

# Keep the provenance beside local exports; only the PNGs go into the README.
$manifest = [ordered]@{
    sourceDimensions = @{ width = 1920; height = 1080 }
    processing = 'Integer crop only, native resolution, lossless PNG. No grading, retouching or generated scene content.'
    captures = @($exports)
}
$manifest | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $outputRoot 'manifest.json') -Encoding UTF8
Write-Output "Exported $($captures.Count) cropped screenshots to $outputRoot"
