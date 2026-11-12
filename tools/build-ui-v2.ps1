param([switch]$Run, [ValidateSet('all', 'compact', 'studio', 'violet')][string]$Variant = 'all')
$ErrorActionPreference = 'Stop'
$Variant = $Variant.ToLowerInvariant()
Push-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)
try {
    $toolchain = Get-ChildItem -LiteralPath '.tools' -Directory -Filter 'llvm-mingw-*-ucrt-x86_64' |
        Sort-Object Name -Descending | Select-Object -First 1
    if (-not $toolchain) { throw 'Portable LLVM-MinGW is missing from .tools.' }
    $cxx = Join-Path $toolchain.FullName 'bin/i686-w64-mingw32-clang++.exe'
    $imgui = 'vendor/imgui-1.92.9b'
    New-Item -ItemType Directory -Force -Path 'build/ui-v2' | Out-Null
    node tools/embed-cursor.mjs
    if ($LASTEXITCODE) { throw 'Cursor embedding failed' }
    $sources = @('tools/ui-v2.cpp', "$imgui/imgui.cpp", "$imgui/imgui_draw.cpp",
        "$imgui/imgui_tables.cpp", "$imgui/imgui_widgets.cpp",
        "$imgui/backends/imgui_impl_win32.cpp", "$imgui/backends/imgui_impl_dx9.cpp")
    & $cxx '-std=c++20' '-O2' '-static' '-mwindows' '-Isrc' '-Ibuild/ui-v2' "-I$imgui" '-Wall' '-Wextra' '-Wno-unused-parameter' `
        '-DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD' '-DIMGUI_USE_BGRA_PACKED_COLOR' @sources `
        '-ld3d9' '-ldwmapi' '-lgdi32' '-lgdiplus' '-limm32' '-o' 'build/ui-v2/FENB-ui-v2-build.exe'
    if ($LASTEXITCODE) { throw 'UI v2 concept build failed' }
    Copy-Item -LiteralPath "$imgui/LICENSE.txt" -Destination 'build/ui-v2/Dear-ImGui-LICENSE.txt'
    Copy-Item -LiteralPath 'assets/fonts/OFL.txt' -Destination 'build/ui-v2/Inter-OFL.txt'
    $variants = if ($Variant -eq 'all') { @('compact', 'studio', 'violet') } else { @($Variant) }
    foreach ($name in $variants) {
        $destination = Join-Path (Get-Location) "build/ui-v2/FENB-ui-v2-$name.exe"
        Copy-Item -LiteralPath 'build/ui-v2/FENB-ui-v2-build.exe' -Destination $destination
        Get-Item -LiteralPath $destination | Select-Object FullName, Length
        if ($Run -and ($Variant -ne 'all' -or $name -ne 'compact')) {
            Start-Process -FilePath $destination -WindowStyle Hidden
        }
    }
} finally {
    Pop-Location
}
