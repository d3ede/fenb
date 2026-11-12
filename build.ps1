param([switch]$Tests,[switch]$Diagnostics)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
$toolchain=Get-ChildItem -LiteralPath '.tools' -Directory -Filter 'llvm-mingw-*-ucrt-x86_64' | Sort-Object Name -Descending | Select-Object -First 1
if (-not $toolchain) { throw 'Portable LLVM-MinGW is missing from .tools. See BUILD.txt.' }
$cxx=Join-Path $toolchain.FullName 'bin\i686-w64-mingw32-clang++.exe'
$outputDirectory=if($Diagnostics){'dist-diagnostics'}else{'dist'}
New-Item -ItemType Directory -Force -Path 'build',$outputDirectory,'src/generated' | Out-Null
node tools/embed-cursor.mjs src/generated/ui_cursor.hpp
if ($LASTEXITCODE) { throw 'Cursor embedding failed' }
node tools/embed-font.mjs
if ($LASTEXITCODE) { throw 'Font embedding failed' }
node tools/embed-logo.mjs
if ($LASTEXITCODE) { throw 'Logo embedding failed' }
node tools/embed-presets.mjs
if ($LASTEXITCODE) { throw 'Profile embedding failed' }
& $cxx '-std=c++20' '-O2' '-static' 'tools/shaderc.cpp' '-ld3dcompiler' '-o' 'build/shaderc.exe'
if ($LASTEXITCODE) { throw 'Shader compiler build failed' }
if($Diagnostics){
    & '.\build\shaderc.exe' 'shaders/probe_depth.hlsl' 'src/generated/probe_depth.hpp' 'ProbeDepth' 'ps_3_0'
    if($LASTEXITCODE){throw 'Diagnostic depth shader compilation failed'}
    & '.\build\shaderc.exe' 'shaders/probe_depth.hlsl' 'src/generated/probe_vertices_vs.hpp' 'ProbeVerticesVS' 'vs_3_0'
    if($LASTEXITCODE){throw 'Diagnostic vertex shader compilation failed'}
    & '.\build\shaderc.exe' 'shaders/probe_depth.hlsl' 'src/generated/probe_vertices_ps.hpp' 'ProbeVerticesPS' 'ps_3_0'
    if($LASTEXITCODE){throw 'Diagnostic vertex readback compilation failed'}
}
& '.\build\shaderc.exe' 'shaders/post.hlsl' 'src/generated/shaders.hpp'
if ($LASTEXITCODE) { throw 'HLSL compilation failed' }
& '.\build\shaderc.exe' 'shaders/water_vs.hlsl' 'src/generated/water_vs.hpp' 'WaterVS' 'vs_3_0'
if ($LASTEXITCODE) { throw 'Water VS compilation failed' }
& '.\build\shaderc.exe' 'shaders/water_ps.hlsl' 'src/generated/water_ps.hpp' 'WaterPS' 'ps_3_0'
if ($LASTEXITCODE) { throw 'Water PS compilation failed' }
& '.\build\shaderc.exe' 'shaders/reflection_ps.hlsl' 'src/generated/reflection_ps.hpp' 'ReflectionPS' 'ps_3_0'
if ($LASTEXITCODE) { throw 'Water reflection compilation failed' }
& '.\build\shaderc.exe' 'shaders/caustics.hlsl' 'src/generated/caustics.hpp' 'CausticFieldPS' 'ps_3_0'
if ($LASTEXITCODE) { throw 'Caustics compilation failed' }
& '.\build\shaderc.exe' 'shaders/scene.hlsl' 'src/generated/scene_shaders.hpp' 'scene'
if ($LASTEXITCODE) { throw 'Scene FX shader compilation failed' }
& '.\build\shaderc.exe' 'shaders/sfx.hlsl' 'src/generated/sfx_shaders.hpp' 'sfx'
if ($LASTEXITCODE) { throw 'SFX shader compilation failed' }
& '.\build\shaderc.exe' 'shaders/smaa.hlsl' 'src/generated/smaa_shaders.hpp' 'smaa'
if ($LASTEXITCODE) { throw 'SMAA shader compilation failed' }
$imgui='vendor/imgui-1.92.9b'
$sources=@('src/exports.cpp','src/generated/exports.cpp','src/proxy.cpp','src/generated/proxies.cpp','src/settings.cpp','src/runtime.cpp','src/effects.cpp','src/water.cpp','src/scene_fx.cpp','src/fps_limiter.cpp','src/environment.cpp','src/ui.cpp',
    "$imgui/imgui.cpp","$imgui/imgui_draw.cpp","$imgui/imgui_tables.cpp","$imgui/imgui_widgets.cpp","$imgui/backends/imgui_impl_win32.cpp","$imgui/backends/imgui_impl_dx9.cpp")
$options=@('-std=c++20','-Os','-static','-shared','-fno-rtti','-ffunction-sections','-fdata-sections','-DIMGUI_DISABLE_DEMO_WINDOWS','-DIMGUI_DISABLE_DEBUG_TOOLS','-DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD','-DIMGUI_USE_BGRA_PACKED_COLOR',"-I$imgui",'-Isrc','-Wall','-Wextra','-Wno-unused-parameter')
if($Diagnostics){$options+='-DFENB_DIAGNOSTICS';$sources+='src/pipeline_probe.cpp'}
& $cxx @options @sources 'src/d3d9.def' '-Wl,--gc-sections,--kill-at,--exclude-all-symbols' '-s' '-ld3d9' '-ldxguid' '-lcomctl32' '-ldwmapi' '-lgdi32' '-limm32' '-lbcrypt' '-lwinmm' '-o' "$outputDirectory/d3d9.dll"
if ($LASTEXITCODE) { throw 'FENB build failed' }
Copy-Item -LiteralPath 'config/FENB.ini' -Destination "$outputDirectory/FENB.ini" -Force
Copy-Item -LiteralPath 'README.md' -Destination "$outputDirectory/README.md" -Force
New-Item -ItemType Directory -Force -Path "$outputDirectory/FENB-presets" | Out-Null
Copy-Item -LiteralPath "$imgui/LICENSE.txt" -Destination "$outputDirectory/Dear-ImGui-LICENSE.txt" -Force
Copy-Item -LiteralPath 'assets/fonts/OFL.txt' -Destination "$outputDirectory/Inter-OFL.txt" -Force
Copy-Item -LiteralPath 'vendor/smaa/LICENSE.txt' -Destination "$outputDirectory/SMAA-LICENSE.txt" -Force
Get-Item -LiteralPath "$outputDirectory/d3d9.dll" | Select-Object FullName,Length
if ($Tests) {
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/settings_test.cpp' 'src/settings.cpp' '-o' 'build/settings-test.exe'
    if ($LASTEXITCODE) { throw 'Settings test build failed' }
    & '.\build\settings-test.exe'
    if ($LASTEXITCODE) { throw 'Settings tests failed' }
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/environment_test.cpp' 'src/environment.cpp' 'src/fps_limiter.cpp' '-ld3d9' '-lbcrypt' '-lwinmm' '-o' 'build/environment-test.exe'
    if ($LASTEXITCODE) { throw 'Environment test build failed' }
    & '.\build\environment-test.exe'
    if ($LASTEXITCODE) { throw 'Environment tests failed' }
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/fps_limiter_test.cpp' 'src/fps_limiter.cpp' '-lbcrypt' '-lwinmm' '-o' 'build/fps-limiter-test.exe'
    if ($LASTEXITCODE) { throw 'FPS limiter test build failed' }
    & '.\build\fps-limiter-test.exe'
    if ($LASTEXITCODE) { throw 'FPS limiter tests failed' }
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/client_layout_test.cpp' 'src/fps_limiter.cpp' 'src/environment.cpp' '-ld3d9' '-lwinmm' '-o' 'build/client-layout-test.exe'
    if ($LASTEXITCODE) { throw 'Client layout test build failed' }
    & '.\build\client-layout-test.exe'
    if ($LASTEXITCODE) { throw 'Client layout tests failed' }
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/depth_policy_test.cpp' 'src/settings.cpp' 'src/fps_limiter.cpp' '-ld3d9' '-lbcrypt' '-lwinmm' '-o' 'build/depth-policy-test.exe'
    if ($LASTEXITCODE) { throw 'Depth policy test build failed' }
    & '.\build\depth-policy-test.exe'
    if ($LASTEXITCODE) { throw 'Depth policy tests failed' }
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/depth_copy_test.cpp' '-ld3d9' '-ldxguid' '-o' 'build/depth-copy-test.exe'
    if($LASTEXITCODE){throw 'Shared depth test build failed'}
    & '.\build\depth-copy-test.exe'
    if($LASTEXITCODE){throw 'Shared depth tests failed'}
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/water_test.cpp' 'src/water.cpp' 'src/scene_fx.cpp' '-ld3d9' '-ldxguid' '-lbcrypt' '-o' 'build/water-test.exe'
    if($LASTEXITCODE){throw 'Water test build failed'}
    & '.\build\water-test.exe'
    if($LASTEXITCODE){throw 'Water material tests failed'}
    & '.\build\water-test.exe' '--msaa'
    if($LASTEXITCODE){throw 'MSAA water material tests failed'}
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/water_scene_test.cpp' 'src/water.cpp' 'src/scene_fx.cpp' '-ld3d9' '-ldxguid' '-lbcrypt' '-o' 'build/water-scene-test.exe'
    if($LASTEXITCODE){throw 'Water scene test build failed'}
    & '.\build\water-scene-test.exe'
    if($LASTEXITCODE){throw 'Water scene test failed'}
    & '.\build\water-scene-test.exe' '--msaa'
    if($LASTEXITCODE){throw 'MSAA water scene test failed'}
    & '.\build\water-scene-test.exe' '--depth24'
    if($LASTEXITCODE){throw 'D24 water scene test failed'}
    & '.\build\water-scene-test.exe' '--depth24' '--msaa'
    if($LASTEXITCODE){throw 'D24 MSAA water scene test failed'}
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/scene_fx_test.cpp' 'src/effects.cpp' 'src/scene_fx.cpp' 'src/settings.cpp' '-ld3d9' '-ldxguid' '-lbcrypt' '-o' 'build/scene-fx-test.exe'
    if($LASTEXITCODE){throw 'Scene FX test build failed'}
    & '.\build\scene-fx-test.exe'
    if($LASTEXITCODE){throw 'Scene FX tests failed'}
    & '.\build\scene-fx-test.exe' '--msaa'
    if($LASTEXITCODE){throw 'MSAA scene FX tests failed'}
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/sfx_test.cpp' '-ld3d9' '-ldxguid' '-lbcrypt' '-o' 'build/sfx-test.exe'
    if($LASTEXITCODE){throw 'SFX test build failed'}
    & '.\build\sfx-test.exe'
    if($LASTEXITCODE){throw 'SFX tests failed'}
    & '.\build\sfx-test.exe' '--msaa'
    if($LASTEXITCODE){throw 'MSAA SFX tests failed'}
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/clarity_test.cpp' '-ld3d9' '-o' 'build/clarity-test.exe'
    if($LASTEXITCODE){throw 'VFX clarity test build failed'}
    & '.\build\clarity-test.exe'
    if($LASTEXITCODE){throw 'VFX clarity tests failed'}
    & '.\build\clarity-test.exe' '--msaa'
    if($LASTEXITCODE){throw 'MSAA VFX clarity tests failed'}
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/sky_detail_test.cpp' '-ld3d9' '-o' 'build/sky-detail-test.exe'
    if($LASTEXITCODE){throw 'Sky detail test build failed'}
    & '.\build\sky-detail-test.exe'
    if($LASTEXITCODE){throw 'Sky detail tests failed'}
    & '.\build\sky-detail-test.exe' '--msaa'
    if($LASTEXITCODE){throw 'MSAA sky detail tests failed'}
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/dx9_test.cpp' 'src/settings.cpp' '-ld3d9' '-ldxguid' '-lgdi32' '-lgdiplus' '-lpsapi' '-o' "$outputDirectory/FENB-preview.exe"
    if ($LASTEXITCODE) { throw 'DX9 test build failed' }
    & ".\$outputDirectory\FENB-preview.exe" '--aa-test'
    if ($LASTEXITCODE) { throw 'Live MSAA recovery tests failed' }
    & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/hud_test.cpp' 'src/settings.cpp' '-ld3d9' '-ldxguid' '-o' "$outputDirectory/hud-test.exe"
    if($LASTEXITCODE){throw 'HUD test build failed'}
    & ".\$outputDirectory\hud-test.exe"
    if($LASTEXITCODE){throw 'HUD preservation test failed'}
    if($Diagnostics){
        & $cxx '-std=c++20' '-O2' '-static' '-Isrc' '-DFENB_DIAGNOSTICS' 'tests/water_capture_test.cpp' 'src/pipeline_probe.cpp' 'src/water.cpp' 'src/scene_fx.cpp' '-ld3d9' '-ldxguid' '-lbcrypt' '-o' 'build/water-capture-test.exe'
        if($LASTEXITCODE){throw 'Water replay capture test build failed'}
        & '.\build\water-capture-test.exe'
        if($LASTEXITCODE){throw 'Water replay capture test failed'}
        & $cxx '-std=c++20' '-O2' '-static' '-Isrc' 'tests/probe_test.cpp' '-ld3d9' '-ldxguid' '-o' "$outputDirectory/probe-test.exe"
        if($LASTEXITCODE){throw 'Probe test build failed'}
        & ".\$outputDirectory\probe-test.exe"
        if($LASTEXITCODE){throw 'Probe test failed'}
    }
    $runtimeOptions=@($options | Where-Object { $_ -ne '-shared' }) + @('-lwinmm')
    $runtimeSources=@('tests/runtime_test.cpp','src/settings.cpp','src/runtime.cpp','src/effects.cpp','src/water.cpp','src/scene_fx.cpp','src/fps_limiter.cpp','src/environment.cpp','src/ui.cpp',
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
