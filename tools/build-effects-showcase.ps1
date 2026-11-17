param(
    [Parameter(Mandatory = $true)][string]$CaptureRoot,
    [Parameter(Mandatory = $true)][string]$GrassRoot,
    [Parameter(Mandatory = $true)][string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$capture = (Resolve-Path -LiteralPath $CaptureRoot).Path
$grass = (Resolve-Path -LiteralPath $GrassRoot).Path
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $output) { throw 'Choose a new output directory to preserve existing captures.' }
foreach ($source in @($capture, $grass)) {
    if ($source -eq $output -or $source.StartsWith($output.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
        throw 'Output must not contain the source directory.'
    }
}
$null = New-Item -ItemType Directory -Path $output
$manifest = [Collections.Generic.List[object]]::new()

function Export-Pair([hashtable]$pair) {
    $before = [Drawing.Bitmap]::new($pair.before)
    $after = [Drawing.Bitmap]::new($pair.after)
    try {
        if ($before.Size -ne $after.Size) { throw 'Source frames have different dimensions.' }
        $r = $pair.crop
        if ($r.Length -ne 4 -or $r[0] -lt 0 -or $r[1] -lt 0 -or
            $r[0] + $r[2] -gt $before.Width -or $r[1] + $r[3] -gt $before.Height) { throw 'Invalid crop.' }
        $rectangle = [Drawing.Rectangle]::new($r[0], $r[1], $r[2], $r[3])
        $files = @(($pair.id + '-before.png'), ($pair.id + '-after.png'))
        if ($pair.height) { $files = @(($pair.id + '-low.png'), ($pair.id + '-high.png')) }
        for ($i = 0; $i -lt 2; $i++) {
            $image = if ($i) { $after } else { $before }
            # Clone exact source pixels: no canvas, scaling, sharpening or grading.
            $cropped = $image.Clone($rectangle, [Drawing.Imaging.PixelFormat]::Format24bppRgb)
            try { $cropped.Save((Join-Path $output $files[$i]), [Drawing.Imaging.ImageFormat]::Png) }
            finally { $cropped.Dispose() }
        }
        $manifest.Add(@{
            title = $pair.title; before = $files[0]; after = $files[1]
            liveClient = [bool]$pair.live; note = $pair.note; crop = $r
            beforeSource = [IO.Path]::GetFileName($pair.before); afterSource = [IO.Path]::GetFileName($pair.after)
            beforeSourceSha256 = (Get-FileHash -LiteralPath $pair.before -Algorithm SHA256).Hash
            afterSourceSha256 = (Get-FileHash -LiteralPath $pair.after -Algorithm SHA256).Hash
        })
    } finally { $before.Dispose(); $after.Dispose() }
}

$liveBefore = Join-Path $capture 'sources/azria-new-effects-off.png'
$liveAfter = Join-Path $capture 'sources/azria-new-effects-on.png'
$liveNote = 'Живой клиент. Меняются только материалы и микроконтраст; остальные эффекты и вечернее освещение одинаковы.'
foreach ($view in @(
    @('01-azria', 'Азрия — общий вид', @(320, 200, 1600, 750)),
    @('02-snow-live', 'Снег — в игре, вблизи', @(650, 700, 640, 240)),
    @('03-snow-tree-live', 'Снежное дерево — в игре', @(1540, 200, 380, 440))
)) {
    Export-Pair @{id = $view[0]; title = $view[1]; crop = $view[2]; before = $liveBefore; after = $liveAfter; live = $true; note = $liveNote}
}

$test = Join-Path $capture 'test-captures'
$testNote = 'Нативный DX9-тест на оригинальной текстуре клиента, не кадр из локации. Материалы 1.00; микроконтраст 0.45; фактура 0.65. Цветокоррекция выключена.'
foreach ($material in @(
    @('04-snow', 'snow', 'Снег — тест материала'),
    @('05-snow-needles', 'snow-needles', 'Заснеженная хвоя — тест материала'),
    @('06-snow-canopy', 'snow-canopy', 'Снег на кроне — тест материала'),
    @('07-bark', 'bark', 'Кора — тест материала'),
    @('08-stone', 'stone', 'Камень — тест материала'),
    @('09-wood', 'wood', 'Дерево — тест материала'),
    @('10-sand', 'sand', 'Песок — тест материала')
)) {
    Export-Pair @{
        id = $material[0]; title = $material[2]; crop = @(220, 210, 600, 380)
        before = (Join-Path $test ($material[1] + '-close-off.png')); after = (Join-Path $test ($material[1] + '-close-combined.png'))
        note = $testNote
    }
}
$grassNote = 'Тестовое поле DX9 с исходной текстурой грунта. Высокое качество; дальность 40; плотность 1.25; высота 0.55. MSAA 8×.'
Export-Pair @{
    id = '11-grass'; title = 'Новая объёмная трава'; crop = @(0, 300, 1000, 400)
    before = (Join-Path $grass 'field-off.png'); after = (Join-Path $grass 'field-on.png'); note = $grassNote
}
Export-Pair @{
    id = '12-grass-close'; title = 'Новая трава — вблизи'; crop = @(150, 380, 700, 300)
    before = (Join-Path $grass 'field-off.png'); after = (Join-Path $grass 'field-on.png'); note = $grassNote
}
Export-Pair @{
    id = '13-grass-height'; title = 'Высота травы: 0.20 / 0.95'; crop = @(0, 300, 1000, 400)
    before = (Join-Path $grass 'field-short.png'); after = (Join-Path $grass 'field-tall.png'); height = $true
    note = 'Тестовое поле DX9. Меняется только высота: low = 0.20, high = 0.95. Остальные параметры и освещение одинаковы.'
}
Export-Pair @{
    id = '14-microcontrast'; title = 'Микроконтраст — отдельный эффект'; crop = @(220, 210, 600, 380)
    before = (Join-Path $test 'stone-close-off.png'); after = (Join-Path $test 'stone-close-clarity.png')
    note = 'Нативный DX9-тест. Материалы и цветокоррекция выключены с обеих сторон. Микроконтраст 0.45; фактура 0.65.'
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 -LiteralPath (Join-Path $output 'manifest.json')
$sections = foreach ($entry in $manifest) {
    $title = [Net.WebUtility]::HtmlEncode($entry.title)
    $note = [Net.WebUtility]::HtmlEncode($entry.note)
    $left = if ($entry.before -like '*-low.png') { 'Высота 0.20' } else { 'До' }
    $right = if ($entry.after -like '*-high.png') { 'Высота 0.95' } else { 'После' }
    '<section><h2>' + $title + '</h2><p>' + $note + '</p><div class="pair"><a href="' + $entry.before + '">' + $left + '<img loading="lazy" src="' + $entry.before + '" alt="' + $left + '"></a><a href="' + $entry.after + '">' + $right + '<img loading="lazy" src="' + $entry.after + '" alt="' + $right + '"></a></div></section>'
}
$html = '<!doctype html><html lang="ru"><meta charset="utf-8"><title>FENB — отдельные кадры до и после</title><style>body{margin:24px auto;padding:0 24px;max-width:1500px;background:#17171c;color:#eee;font:16px system-ui}section{margin:40px 0}.pair{display:grid;grid-template-columns:1fr 1fr;gap:20px}img{display:block;max-width:100%;height:auto;margin-top:12px}a{color:#c7abeb}p{color:#aaa}@media(max-width:650px){.pair{grid-template-columns:1fr}}</style><h1>FENB · до / после</h1><p>Каждый кадр сохранён отдельным PNG. Одинаковые обрезка и масштаб 1:1; нажмите на изображение для открытия файла.</p>' + ($sections -join "`n") + '</html>'
[IO.File]::WriteAllText((Join-Path $output 'index.html'), $html, [Text.UTF8Encoding]::new($false))
@(
 'FENB — отдельные кадры для сравнения', '',
 'before = до; after = после. low / high = разная высота травы.',
 'Каждый PNG содержит один обрезанный кадр без рамок, подписей и увеличения.',
 'Парные изображения обрезаны по одинаковым координатам; сохранён исходный масштаб 1:1 и цвет каждого пикселя.',
 '01–03 сняты в живом клиенте. Остальные — нативные DX9-тесты на оригинальных текстурах, не игровые локации.',
 'Камера и свет в каждой паре одинаковы. В живой игре снегопад и объекты продолжают двигаться.',
 'В тестах материалов интенсивность = 1.00, микроконтраст = 0.45, фактура = 0.65; цветокоррекция выключена.',
 'Параметры конкретных сравнений указаны в index.html и manifest.json.'
) | Set-Content -Encoding utf8 -LiteralPath (Join-Path $output 'README.txt')
Write-Output ('Created {0} pairs / {1} separate cropped PNGs in {2}' -f $manifest.Count, ($manifest.Count * 2), $output)
