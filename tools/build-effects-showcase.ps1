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
if (Test-Path -LiteralPath $output) { throw 'Choose a new output directory to preserve existing comparisons.' }
foreach ($inputRoot in @($capture, $grass)) {
    if ($inputRoot.StartsWith($output.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase) -or $inputRoot -eq $output) {
        throw 'Output must not contain the source directory.'
    }
}
$null = New-Item -ItemType Directory -Path $output
$logoPath = Join-Path $PSScriptRoot '../assets/branding/approved/PNG/Dark/fenb-dark-horizontal-transparent.png'
$logo = [Drawing.Bitmap]::new([IO.Path]::GetFullPath($logoPath))
$fonts = @{}
foreach ($size in @(17, 19, 22, 26, 38)) {
    $fonts[$size] = [Drawing.Font]::new('Arial', $size, [Drawing.FontStyle]::Regular, [Drawing.GraphicsUnit]::Pixel)
}
$white = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#F0EDF6'))
$muted = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#AAA8B5'))
$accent = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#BA9CD9'))
$border = [Drawing.Pen]::new([Drawing.ColorTranslator]::FromHtml('#39343F'), 1)
$manifest = [Collections.Generic.List[object]]::new()

function Write-Line($g, [string]$value, [int]$x, [int]$y, [int]$size, $brush = $white) {
    $g.DrawString($value, $fonts[$size], $brush, [single]$x, [single]$y)
}
function Native-Crop($g, $image, [int]$x, [int]$y, [int[]]$rectangle, [int]$scale = 1) {
    if ($rectangle.Length -ne 4 -or $rectangle[0] -lt 0 -or $rectangle[1] -lt 0 -or
        $rectangle[0] + $rectangle[2] -gt $image.Width -or $rectangle[1] + $rectangle[3] -gt $image.Height) {
        throw 'Crop exceeds the original screenshot.'
    }
    $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
    $g.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::Half
    $dest = [Drawing.Rectangle]::new($x, $y, $rectangle[2] * $scale, $rectangle[3] * $scale)
    $g.DrawImage($image, $dest, $rectangle[0], $rectangle[1], $rectangle[2], $rectangle[3], [Drawing.GraphicsUnit]::Pixel)
    $g.DrawRectangle($border, $dest)
}
function Image-Card([hashtable]$card) {
    $before = [Drawing.Bitmap]::new($card.before)
    $after = [Drawing.Bitmap]::new($card.after)
    $detailBefore = if ($card.detailBefore) { [Drawing.Bitmap]::new($card.detailBefore) } else { $before }
    $detailAfter = if ($card.detailAfter) { [Drawing.Bitmap]::new($card.detailAfter) } else { $after }
    $bitmap = [Drawing.Bitmap]::new(1920, 1080, [Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $g = [Drawing.Graphics]::FromImage($bitmap)
    try {
        if ($before.Size -ne $after.Size) { throw 'Comparison frames have different dimensions.' }
        $g.Clear([Drawing.ColorTranslator]::FromHtml('#17171C'))
        $g.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
        Write-Line $g $card.title 40 22 38
        Write-Line $g $card.subtitle 42 76 19 $muted
        $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.DrawImage($logo, [Drawing.Rectangle]::new(1610, 8, 256, 80))
        Write-Line $g $card.left 40 120 26
        Write-Line $g $card.right 980 120 26 $accent
        if ($card.live) {
            # Equal, explicit HUD crops; both live frames use the same resampling.
            $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $g.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::Half
            foreach ($entry in @(@{image = $before; x = 40}, @{image = $after; x = 980})) {
                $g.DrawImage($entry.image, [Drawing.Rectangle]::new($entry.x, 160, 900, 428), 320, 190, 1600, 760, [Drawing.GraphicsUnit]::Pixel)
                $g.DrawRectangle($border, $entry.x, 160, 900, 428)
            }
            Write-Line $g 'Один ракурс и время суток. Погода и подвижные объекты продолжают анимацию.' 42 612 19 $muted
            Write-Line $g 'Снег вблизи · одинаковый фрагмент, увеличение 2×' 42 730 22
            Native-Crop $g $detailBefore 40 776 $card.crop 2
            Native-Crop $g $detailAfter 980 776 $card.crop 2
        } else {
            Native-Crop $g $before 40 160 @(50, 70, 900, 580)
            Native-Crop $g $after 980 160 @(50, 70, 900, 580)
            Write-Line $g $card.detailTitle 42 754 22
            Native-Crop $g $detailBefore 40 792 $card.crop 2
            Native-Crop $g $detailAfter 980 792 $card.crop 2
        }
        Write-Line $g $card.footer 42 1032 17 $muted
        $file = $card.id + '.png'
        $bitmap.Save((Join-Path $output $file), [Drawing.Imaging.ImageFormat]::Png)
        $manifest.Add(@{
            file = $file; title = $card.title; liveClient = [bool]$card.live
            before = [IO.Path]::GetFileName($card.before); after = [IO.Path]::GetFileName($card.after)
            beforeSha256 = (Get-FileHash -LiteralPath $card.before -Algorithm SHA256).Hash
            afterSha256 = (Get-FileHash -LiteralPath $card.after -Algorithm SHA256).Hash
            note = $card.footer; detailCrop = $card.crop
        })
    } finally {
        $g.Dispose(); $bitmap.Dispose()
        if ($card.detailBefore) { $detailBefore.Dispose() }
        if ($card.detailAfter) { $detailAfter.Dispose() }
        $before.Dispose(); $after.Dispose()
    }
}

try {
    $liveBefore = Join-Path $capture 'sources/azria-new-effects-off.png'
    $liveAfter = Join-Path $capture 'sources/azria-new-effects-on.png'
    Image-Card @{
        id = '01-azria-new-materials'; title = 'Азрия · новые материалы и микроконтраст'
        subtitle = 'Живой клиент · исходные текстуры игры · фиксированное вечернее освещение'
        before = $liveBefore; after = $liveAfter; live = $true; crop = @(780, 740, 450, 100)
        left = 'Без новых эффектов'; right = 'Материалы + микроконтраст'
        footer = 'Меняются только материалы и микроконтраст. Остальные эффекты FENB и время суток одинаковы.'
    }
    Image-Card @{
        id = '02-azria-full-fenb'; title = 'Азрия · обработка FENB'
        subtitle = 'Живой клиент · одинаковый ракурс · текущие настройки пользователя'
        before = (Join-Path $capture 'sources/azria-original-matched.png'); after = $liveAfter
        live = $true; crop = @(780, 740, 450, 100)
        left = 'Обработка выключена'; right = 'FENB включён'
        footer = 'Сравнение всей активной обработки FENB: здесь меняются также цвет и другие включённые эффекты.'
    }
    $test = Join-Path $capture 'test-captures'
    $materials = @(
        @('03-snow', 'snow', 'Снег · поверхность и мелкая фактура'),
        @('04-snow-trees', 'snow-needles', 'Снежные деревья · хвоя и крона'),
        @('05-tree-bark', 'bark', 'Кора дерева · рельеф и чёткость'),
        @('06-stone', 'stone', 'Камень · рельеф и микроконтраст'),
        @('07-wood', 'wood', 'Дерево · рельеф и чёткость'),
        @('08-sand', 'sand', 'Песок · мелкая фактура')
    )
    foreach ($item in $materials) {
        $card = @{
            id = $item[0]; title = $item[2]; subtitle = 'DX9-тест FENB · оригинальная текстура клиента · одинаковые свет и камера'
            before = (Join-Path $test ($item[1] + '-close-off.png')); after = (Join-Path $test ($item[1] + '-close-combined.png'))
            left = 'Исходный материал'; right = 'Материал + микроконтраст'; crop = @(300, 360, 450, 100)
            detailTitle = 'Детали · одинаковый фрагмент, увеличение 2×'
            footer = 'Тестовый 3D-рендер, не кадр из локации. Материалы: 1.00; микроконтраст: 0.45; фактура: 0.65. Без цветокоррекции.'
        }
        if ($item[1] -eq 'snow-needles') {
            $card.detailBefore = Join-Path $test 'snow-canopy-close-off.png'
            $card.detailAfter = Join-Path $test 'snow-canopy-close-combined.png'
            $card.detailTitle = 'Снег на кроне · одинаковый фрагмент, увеличение 2×'
        }
        Image-Card $card
    }
    Image-Card @{
        id = '09-dense-grass'; title = 'Объёмная трава · покрытие поля'
        subtitle = 'DX9-тест FENB · исходная текстура грунта · одинаковые камера, рельеф и свет'
        before = (Join-Path $grass 'field-off.png'); after = (Join-Path $grass 'field-on.png')
        left = 'Новая трава выключена'; right = 'Новая трава включена'; crop = @(300, 450, 450, 100)
        detailTitle = 'Травинки вблизи · одинаковый фрагмент, увеличение 2×'
        footer = 'Тестовое поле, не кадр из локации. Высокое качество, дальность 40, плотность 1.25, высота 0.55. MSAA 8×.'
    }
    Image-Card @{
        id = '10-grass-height'; title = 'Трава · настройка высоты'
        subtitle = 'DX9-тест FENB · меняется только высота травы'
        before = (Join-Path $grass 'field-short.png'); after = (Join-Path $grass 'field-tall.png')
        left = 'Высота 0.20'; right = 'Высота 0.95'; crop = @(300, 450, 450, 100)
        detailTitle = 'Травинки вблизи · одинаковый фрагмент, увеличение 2×'
        footer = 'Тестовое поле, не кадр из локации. Камера, рельеф, плотность, качество и освещение одинаковы. MSAA 8×.'
    }
    Image-Card @{
        id = '11-microcontrast'; title = 'Микроконтраст · отдельный эффект'
        subtitle = 'DX9-тест FENB · обработка материалов и цветокоррекция выключены с обеих сторон'
        before = (Join-Path $test 'stone-close-off.png'); after = (Join-Path $test 'stone-close-clarity.png')
        left = 'Микроконтраст выключен'; right = 'Микроконтраст включён'; crop = @(300, 360, 450, 100)
        detailTitle = 'Детали · одинаковый фрагмент, увеличение 2×'
        footer = 'Тестовый 3D-рендер, не кадр из локации. Микроконтраст 0.45; фактура 0.65. Исходная текстура, геометрия и свет одинаковы.'
    }
    $manifest | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 -LiteralPath (Join-Path $output 'manifest.json')
    $links = foreach ($entry in $manifest) {
        $title = [Net.WebUtility]::HtmlEncode($entry.title)
        '<a href="' + $entry.file + '"><img loading="lazy" src="' + $entry.file + '" alt="' + $title + '"><span>' + $title + '</span></a>'
    }
    $html = '<!doctype html><html lang="ru"><meta charset="utf-8"><title>FENB — сравнение эффектов</title><style>body{background:#17171c;color:#eeebf4;font:16px system-ui;margin:32px}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(360px,1fr));gap:24px}a{color:#c7abeb;text-decoration:none}img{width:100%;display:block}span{display:block;padding:12px 0}p{color:#aaa8b5}</style><h1>FENB · сравнение эффектов</h1><p>Нажмите на изображение, чтобы открыть PNG в полном разрешении. Первые два сравнения сняты в живом клиенте; остальные — нативные DX9-тесты.</p><main>' + ($links -join "`n") + '</main></html>'
    [IO.File]::WriteAllText((Join-Path $output 'index.html'), $html, [Text.UTF8Encoding]::new($false))
    @'
FENB — сравнение новых эффектов

Все изображения — PNG 1920×1080, пригодные для пересылки.
01 — новые материалы и микроконтраст в живом клиенте, при одинаковой остальной обработке.
02 — вся активная обработка FENB в живом клиенте.
03–08 — нативные DX9-тесты материалов на оригинальных текстурах клиента.
09–10 — нативное тестовое поле с новой 3D-травой и разной высотой.
11 — отдельное сравнение микроконтраста, без обработки материалов.

В каждой паре одинаковые камера, текстура и освещение. В живой игре снегопад и объекты продолжают двигаться.
Нижние фрагменты увеличены ровно в 2 раза методом ближайшего соседа. Цвета исходных кадров не ретушировались.
Параметры и тип сцены подписаны на каждом изображении. Кадры 03–11 не являются скриншотами игровых локаций.
Исходные полноэкранные скриншоты с игровым интерфейсом в набор для пересылки не включены.
'@ | Set-Content -Encoding utf8 -LiteralPath (Join-Path $output 'README.txt')
    Write-Output ('Created {0} comparison PNGs in {1}' -f $manifest.Count, $output)
} finally {
    $logo.Dispose(); $white.Dispose(); $muted.Dispose(); $accent.Dispose(); $border.Dispose()
    foreach ($font in $fonts.Values) { $font.Dispose() }
}
