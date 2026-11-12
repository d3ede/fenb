param([switch]$Tests,[switch]$Diagnostics)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
$toolchain=Get-ChildItem -LiteralPath '.tools' -Directory -Filter 'llvm-mingw-*-ucrt-x86_64' | Sort-Object Name -Descending | Select-Object -First 1
if (-not $toolchain) { throw 'Portable LLVM-MinGW is missing from .tools. See BUILD.txt.' }
$cxx=Join-Path $toolchain.FullName 'bin\i686-w64-mingw32-clang++.exe'
$outputDirectory=if($Diagnostics){'dist-diagnostics'}else{'dist'}
New-Item -ItemType Directory -Force -Path 'build',$outputDirectory,'src/generated' | Out-Null
node tools/embed-font.mjs
if ($LASTEXITCODE) { throw 'Font embedding failed' }
& $cxx '-std=c++20' '-O2' '-static' 'tools/shaderc.cpp' '-ld3dcompiler' '-o' 'build/shaderc.exe'
if ($LASTEXITCODE) { throw 'Shader compiler build failed' }
& '.\build\shaderc.exe' 'shaders/post.hlsl' 'src/generated/shaders.hpp'
if ($LASTEXITCODE) { throw 'HLSL compilation failed' }
& '.\build\shaderc.exe' 'shaders/water_vs.hlsl' 'src/generated/water_vs.hpp' 'WaterVS' 'vs_3_0'
if ($LASTEXITCODE) { throw 'Water VS compilation failed' }
& '.\build\shaderc.exe' 'shaders/water_ps.hlsl' 'src/generated/water_ps.hpp' 'WaterPS' 'ps_3_0'
if ($LASTEXITCODE) { throw 'Water PS compilation failed' }
$imgui='vendor/imgui-1.92.9b'
$sources=@('src/exports.cpp','src/generated/exports.cpp','src/proxy.cpp','src/generated/proxies.cpp','src/settings.cpp','src/runtime.cpp','src/effects.cpp','src/water.cpp','src/fps_limiter.cpp','src/ui.cpp',
    "$imgui/imgui.cpp","$imgui/imgui_draw.cpp","$imgui/imgui_tables.cpp","$imgui/imgui_widgets.cpp","$imgui/backends/imgui_impl_win32.cpp","$imgui/backends/imgui_impl_dx9.cpp")
$options=@('-std=c++20','-Os','-static','-shared','-fno-rtti','-ffunction-sections','-fdata-sections','-DIMGUI_DISABLE_DEMO_WINDOWS','-DIMGUI_DISABLE_DEBUG_TOOLS','-DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD','-DIMGUI_USE_BGRA_PACKED_COLOR',"-I$imgui",'-Isrc','-Wall','-Wextra','-Wno-unused-parameter')
if($Diagnostics){$options+='-DFENB_DIAGNOSTICS';$sources+='src/pipeline_probe.cpp'}
& $cxx @options @sources 'src/d3d9.def' '-Wl,--gc-sections,--kill-at,--exclude-all-symbols' '-s' '-ld3d9' '-ldxguid' '-lcomctl32' '-ldwmapi' '-lgdi32' '-limm32' '-lbcrypt' '-lwinmm' '-o' "$outputDirectory/d3d9.dll"
if ($LASTEXITCODE) { throw 'FENB build failed' }
Copy-Item -LiteralPath 'config/FENB.ini' -Destination "$outputDirectory/FENB.ini" -Force
Copy-Item -LiteralPath 'README.md' -Destination "$outputDirectory/README.md" -Force
New-Item -ItemType Directory -Force -Path "$outputDirectory/FENB-presets" | Out-Null
Get-ChildItem -LiteralPath 'config/presets' -Filter '*.ini' -File | Copy-Item -Destination "$outputDirectory/FENB-presets" -Force
Copy-Item -LiteralPath "$imgui/LICENSE.txt" -Destination "$outputDirectory/Dear-ImGui-LICENSE.txt" -Force
Copy-Item -LiteralPath 'assets/fonts/OFL.txt' -Destination "$outputDirectory/Inter-OFL.txt" -Force
Get-Item -LiteralPath "$outputDirectory/d3d9.dll" | Select-Object FullName,Length
if ($Tests) {
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/settings_test.cpp' 'src/settings.cpp' '-o' 'build/settings-test.exe'
    if ($LASTEXITCODE) { throw 'Settings test build failed' }
    & '.\build\settings-test.exe'
    if ($LASTEXITCODE) { throw 'Settings tests failed' }
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/fps_limiter_test.cpp' 'src/fps_limiter.cpp' '-lbcrypt' '-lwinmm' '-o' 'build/fps-limiter-test.exe'
    if ($LASTEXITCODE) { throw 'FPS limiter test build failed' }
    & '.\build\fps-limiter-test.exe'
    if ($LASTEXITCODE) { throw 'FPS limiter tests failed' }
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/water_test.cpp' 'src/water.cpp' '-ld3d9' '-ldxguid' '-lbcrypt' '-o' 'build/water-test.exe'
    if($LASTEXITCODE){throw 'Water test build failed'}
    & '.\build\water-test.exe'
    if($LASTEXITCODE){throw 'Water material tests failed'}
    & '.\build\water-test.exe' '--msaa'
    if($LASTEXITCODE){throw 'MSAA water material tests failed'}
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/water_scene_test.cpp' 'src/water.cpp' '-ld3d9' '-ldxguid' '-lbcrypt' '-o' 'build/water-scene-test.exe'
    if($LASTEXITCODE){throw 'Water scene test build failed'}
    & '.\build\water-scene-test.exe'
    if($LASTEXITCODE){throw 'Water scene test failed'}
    & '.\build\water-scene-test.exe' '--msaa'
    if($LASTEXITCODE){throw 'MSAA water scene test failed'}
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/dx9_test.cpp' 'src/settings.cpp' '-ld3d9' '-ldxguid' '-lgdi32' '-lgdiplus' '-lpsapi' '-o' "$outputDirectory/FENB-preview.exe"
    if ($LASTEXITCODE) { throw 'DX9 test build failed' }
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/hud_test.cpp' 'src/settings.cpp' '-ld3d9' '-ldxguid' '-o' "$outputDirectory/hud-test.exe"
    if($LASTEXITCODE){throw 'HUD test build failed'}
    & ".\$outputDirectory\hud-test.exe"
    if($LASTEXITCODE){throw 'HUD preservation test failed'}
    if($Diagnostics){
        & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/probe_test.cpp' '-ld3d9' '-ldxguid' '-o' "$outputDirectory/probe-test.exe"
        if($LASTEXITCODE){throw 'Probe test build failed'}
        & ".\$outputDirectory\probe-test.exe"
        if($LASTEXITCODE){throw 'Probe test failed'}
    }
    $runtimeOptions=@($options | Where-Object { $_ -ne '-shared' }) + @('-lwinmm')
    $runtimeSources=@('tests/runtime_test.cpp','src/settings.cpp','src/runtime.cpp','src/effects.cpp','src/water.cpp','src/fps_limiter.cpp','src/ui.cpp',
        "$imgui/imgui.cpp","$imgui/imgui_draw.cpp","$imgui/imgui_tables.cpp","$imgui/imgui_widgets.cpp","$imgui/backends/imgui_impl_win32.cpp","$imgui/backends/imgui_impl_dx9.cpp")
    if($Diagnostics){$runtimeSources+='src/pipeline_probe.cpp'}
    & $cxx @runtimeOptions @runtimeSources '-ld3d9' '-ldxguid' '-lcomctl32' '-ldwmapi' '-lgdi32' '-limm32' '-lbcrypt' '-o' 'build/runtime-test.exe'
    if ($LASTEXITCODE) { throw 'Runtime test build failed' }
    & '.\build\runtime-test.exe'
    if ($LASTEXITCODE) { throw 'Runtime tests failed' }
    $uiSources=@($runtimeSources | Where-Object { $_ -ne 'tests/runtime_test.cpp' })
    & $cxx @runtimeOptions 'tests/ui_test.cpp' @uiSources '-ld3d9' '-ldxguid' '-lcomctl32' '-ldwmapi' '-lgdi32' '-lgdiplus' '-limm32' '-lbcrypt' '-o' 'build/ui-test.exe'
    if($LASTEXITCODE){throw 'UI test build failed'}
    & '.\build\ui-test.exe'
    if($LASTEXITCODE){throw 'UI interaction tests failed'}

}
