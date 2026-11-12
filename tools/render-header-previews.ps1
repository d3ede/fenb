$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath (Split-Path -Parent $PSScriptRoot)
$toolchain = Get-ChildItem -LiteralPath '.tools' -Directory -Filter 'llvm-mingw-*-ucrt-x86_64' |
    Sort-Object Name -Descending | Select-Object -First 1
if (-not $toolchain) { throw 'Portable LLVM-MinGW toolchain not found' }
$compiler = Join-Path $toolchain.FullName 'bin/i686-w64-mingw32-clang++.exe'
$imgui = 'vendor/imgui-1.92.9b'
$sources = @('tests/ui_test.cpp', 'src/settings.cpp', 'src/runtime.cpp', 'src/effects.cpp',
    'src/water.cpp', 'src/scene_fx.cpp', 'src/fps_limiter.cpp', 'src/ui.cpp',
    "$imgui/imgui.cpp", "$imgui/imgui_draw.cpp", "$imgui/imgui_tables.cpp",
    "$imgui/imgui_widgets.cpp", "$imgui/backends/imgui_impl_win32.cpp", "$imgui/backends/imgui_impl_dx9.cpp")
$options = @('-std=c++20', '-Os', '-static', '-fno-rtti', '-DIMGUI_DISABLE_DEMO_WINDOWS',
    '-DIMGUI_DISABLE_DEBUG_TOOLS', '-DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD', '-DIMGUI_USE_BGRA_PACKED_COLOR',
    "-I$imgui", '-Isrc', '-Wall', '-Wextra', '-Wno-unused-parameter')
& $compiler @options @sources '-ld3d9' '-ldxguid' '-lcomctl32' '-ldwmapi' '-lgdi32' '-limm32' '-lbcrypt' '-lgdiplus' '-lwinmm' '-o' 'build/header-logo-preview.exe'
if ($LASTEXITCODE) { throw 'Header preview fixture build failed' }
& '.\build\header-logo-preview.exe'
if ($LASTEXITCODE) { throw 'Header preview fixture failed' }
& $compiler '-std=c++20' '-O2' '-static' 'tools/header-logo-gallery.cpp' '-lgdiplus' '-lgdi32' '-o' 'build/header-logo-gallery.exe'
if ($LASTEXITCODE) { throw 'Header gallery build failed' }
& '.\build\header-logo-gallery.exe'
if ($LASTEXITCODE) { throw 'Header gallery rendering failed' }
