# FENB

FENB is a graphics mod for Flyff, inspired by ENBSeries. It uses a Windows x86 Direct3D 9 proxy, C++20, precompiled shaders and a native Dear ImGui settings panel.

## Controls and profiles

- **Shift + Enter** opens and closes the panel without opening game chat. Each opening centers the panel. The open panel can still be moved.
- **Effects** enables or disables all FENB rendering. Color correction, Bloom and WaterFX also have individual switches.
- Drag a slider, double-click its track to reset it, or click the number to type. The small arrow buttons use a parameter-specific step. The reset icon is shown for changed values.
- Edits save automatically to `FENB.ini`. **Ctrl + S** saves immediately.
- **Save…** beside the profile selector saves a named profile in `FENB-presets`. Existing names require the explicit **Replace profile** button. Copy compatible `.ini` files into this folder to add profiles.
- **WaterFX** opens its own compact page. **Back** returns to the image controls.

## WaterFX implementation and limits

WaterFX replaces the material only for the two water texture families identified in the Homesickness Neuz render capture. Identification uses exact first-mip SHA-256 fingerprints, cached as texture-private metadata. The draw must also match the observed fixed-function layout and transparency state, belong to the main backbuffer and occur before the HUD. Unknown textures and unsupported states retain the original renderer. No game memory or vertex-buffer readback is used.

The material supplies small vertex waves, animated normal ripples, adjustable opacity, a turquoise tint, directional-light specular glints and approximate sky-colored Fresnel reflection. Wave height, wavelength, speed, ripple strength and frequency, opacity, tint, sky contribution, glint intensity and smoothness have separate controls. Zero wave height, ripple strength or glint intensity disables that component.

The sky contribution is an analytic environment approximation. There are **no scene-object reflections, screen-space refraction, depth-based shore fading or underwater reconstruction**. Transparency reveals only geometry the original game has already drawn. Coarse water meshes limit vertex-wave detail. Sun glints use the game's enabled directional light; they do not imply that a visible sun billboard is at the same position. SunFX, SkyFX, new shadowing and depth-based atmosphere are not implemented by this change.

WaterFX adds no full-frame render targets. Its shaders run only on matched water draws; render state is captured and restored around each replacement. Resources are released on Lost Device and Reset and created again lazily. Real-client performance and appearance still need in-game comparison; native test results do not establish an FPS cost for all maps or hardware.

Restoration also preserves the input-binding changes required by [DrawPrimitiveUP](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawprimitiveup) and [DrawIndexedPrimitiveUP](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawindexedprimitiveup). Native regression tests exercise both paths.

Post-processing runs before the first screen-space HUD draw, preserving interface colors and later 3D portraits. Present and swap-chain Present remain supported.

## Build and checks

The local portable LLVM-MinGW toolchain lives under `.tools/llvm-mingw-*-ucrt-x86_64`. Run `./build.ps1 -Tests`, `./dist/FENB-preview.exe` and `node tools/verify-binary.mjs`. The test build checks settings, exact HUD colors, native water material selection and restoration, transparency, Reset, input handling and panel interactions. The water fixture test reads the locally installed DDS; it reports a skip when that optional game asset is absent. Test screenshots and configuration files stay in ignored build folders.

Use `-Diagnostics` only for the opt-in bounded pipeline capture. The normal build has no capture code. A CMake x86 build is also described in `CMakeLists.txt`; the PowerShell/LLVM build is the verified local path.

`node tools/water-catalog.mjs <World/Texture directory>` regenerates the checked-in exact water fingerprints. Proxy interfaces are generated with `node tools/generate-proxies.mjs <d3d9.h>`; do not hand-edit generated forwarding methods.

The DLL belongs beside `Neuz.exe`. Keep an existing `FENB.ini` when updating. A running game keeps its already loaded DLL and must be restarted to load a replacement.

Dear ImGui and Inter license notices ship with the build. No compatibility guarantee is made for other Flyff clients.
