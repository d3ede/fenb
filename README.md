# FENB

FENB is a graphics mod for Flyff, inspired by ENBSeries. It uses a Windows x86 Direct3D 9 proxy, C++20, precompiled shaders and a native Dear ImGui settings panel.

## Controls and profiles

- **Shift + Enter** opens and closes the panel without opening game chat. Each opening centers the panel. The open panel can still be moved.
- **Effects** enables or disables all FENB rendering. Color correction, Bloom and WaterFX also have individual switches.
- Drag a slider, double-click its track to reset it, or click the number to type. The small arrow buttons use a parameter-specific step. The reset icon is shown for changed values.
- Edits save automatically to `FENB.ini`. **Ctrl + S** saves immediately.
- **Create…** beside the profile selector, or **Create profile…** inside its dropdown, saves the current settings under a name. Type the name and press **Save**; the profile appears in the selector. For a loaded custom profile the toolbar offers **Save…**. Existing names require the explicit **Replace profile** button. Files live in `FENB-presets`; compatible `.ini` files can also be copied into that folder.
- **WaterFX »** immediately after Bloom opens the water page. **Back** returns to the image controls.
- The reset icon in each section heading resets only that section's parameters. Effect enable switches, other sections, the global switch and the FPS option are preserved.

Four visual presets are included: **Спокойная вода** (calmer surface), **Мягкая бирюза** (soft turquoise), **Прозрачная вода** (clear water), and **Насыщенная вода** (stronger color). The calm preset accompanies the revised material, with reduced waves, glints and original texture detail. Its new shader has native DX9 fixture verification; it has not yet been visually checked in the running client. The other three presets were tuned with the earlier material. None adds scene reflections or underwater caustics.

## Optional client FPS unlock

Open the panel's actions menu (three dots) and enable **Снять лимит FPS**, or set `unlock_fps=1` under `[General]` in `FENB.ini`. The setting is off by default, applies independently of the effects switch and survives visual preset changes. INI edits apply while the panel is closed, like the existing image settings.

The unlock supports only the inspected Homesickness `Neuz.exe` revision with SHA-256 `ce32e350c6002496acf9e8774275784971186e6144901f7c2c7d9b37a4aa41a1`. FENB verifies the executable file, PE identity, relocated frame-gate instructions and writable limiter data before changing the client's existing in-memory limiter flag. Unknown revisions and modified code are left unchanged; the menu entry is unavailable. No executable file or machine code is patched, and VSync and driver limits remain in force.

The original client waits for a 16 ms frame budget (about 62.5 FPS). FENB clears that flag while enabled and restores its original value with a fresh wait budget when disabled or when the last FENB device is released. Multiple devices share ownership, and scene changes that reset the flag are handled on the next presentation. This removes that particular cap; it does not guarantee 500 FPS or establish game-logic compatibility at high frame rates. A DLL update requires restarting the client. Native tests use disposable mapped images and do not modify a running game.

## WaterFX implementation and limits

WaterFX replaces the material only for the two water texture families identified in the Homesickness Neuz render capture. Identification uses exact first-mip SHA-256 fingerprints, cached as texture-private metadata. The draw must also match the observed fixed-function layout and transparency state, belong to the main backbuffer and occur before the HUD. Unknown textures and unsupported states retain the original renderer. No game memory or vertex-buffer readback is used.

The material supplies small vertex waves, crossed domain-warped normal ripples, adjustable opacity, a turquoise tint, GGX directional-light specular glints and approximate sky-colored Fresnel reflection. Long-wave normals are evaluated per pixel independently of mesh density; high-frequency ripples fade at distance. Original water texture RGB is a separate, low default contribution, so the legacy animation's repeated stripes do not dominate the surface. Texture alpha still preserves cutouts. Each component has its own control.

Opacity is controlled by FENB and texture cutout coverage. Legacy vertex and material alpha are not multiplied into it: a zero vertex alpha channel previously made the material almost invisible and defeated the opacity control. The native water regression fixture now uses zero vertex alpha and verifies that both opacity settings still produce distinct visible results.

Refraction uses a GPU-only color snapshot taken once before the first matched water draw in each frame. The material distorts that snapshot locally with its view-space normal and combines transmission, body color and angle-dependent Fresnel reflection. It does not sample the live render target or refract previously drawn FENB water recursively. A failed/unsupported copy falls back to alpha-blended water. Zero refraction bypasses the copy and releases the target. Only geometry drawn before the snapshot can appear underwater; a small distortion can still pull foreground/shore colors into the surface because there is no depth rejection.

Sky reflection captures the positively identified `skybox01` and `cloud01` fixed-function geometry into a 128-pixel cubemap, updated at most four times per second. Only their original DrawPrimitive calls are repeated, with six camera orientations, the original texture stages and no depth buffer; no world scene is replayed or retained. The water samples this actual sky/cloud rendering with its reflected normal. Unknown sky textures or unsupported paths use the analytic gradient. A frame without a matched sky cannot reuse a stale map from another scene. The native fixture verifies the capture, cubemap selection, main-target preservation and state restoration; appearance in the live client still needs checking after restart.

There are **no scene-object reflections, depth-based shore fading, physically measured absorption thickness or underwater reconstruction**. On the tested adapter, INTZ format support succeeds but RESZ depth resolve is unavailable; no depth-derived effect is advertised on the strength of INTZ support alone. The original MSAA depth surface is preserved. Coarse water meshes still limit geometric wave detail. Sun glints use the game's enabled directional light; they do not imply that a visible sun billboard is at the same position. SunFX, SkyFX, new shadowing and depth-based atmosphere are not implemented by this change.

With refraction enabled, WaterFX adds one full-size 32-bit color target (about 7.4 MiB at 1920×1009) and one color resolve per water-containing frame. The sky cubemap adds 384 KiB and at most twelve small repeated draws per update for the two observed sky/cloud passes. There is no world-scene replay, CPU image readback or persistent copy of game geometry. Zero refraction releases the frame target; zero sky reflection releases the cubemap. Shaders run only on matched water draws; render state, samplers and targets are restored around each replacement. Resources are released on Lost Device and Reset and recreated lazily. Real-client performance and appearance still need in-game comparison; native test results do not establish an FPS cost for all maps or hardware.

Restoration also preserves the input-binding changes required by [DrawPrimitiveUP](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawprimitiveup) and [DrawIndexedPrimitiveUP](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawindexedprimitiveup). Native regression tests exercise both paths.

Post-processing runs before the first screen-space HUD draw, preserving interface colors and later 3D portraits. Present and swap-chain Present remain supported.

## Build and checks

The local portable LLVM-MinGW toolchain lives under `.tools/llvm-mingw-*-ucrt-x86_64`. Run `./build.ps1 -Tests`, `./dist/FENB-preview.exe` and `node tools/verify-binary.mjs`. The test build checks settings, exact HUD colors, native water material selection and restoration, transparency, Reset, input handling and panel interactions. The water fixture test reads the locally installed DDS; it reports a skip when that optional game asset is absent. Test screenshots and configuration files stay in ignored build folders.

Use `-Diagnostics` only for the opt-in bounded pipeline capture. The normal build has no capture code. A CMake x86 build is also described in `CMakeLists.txt`; the PowerShell/LLVM build is the verified local path.

`node tools/water-catalog.mjs <World/Texture directory>` regenerates the checked-in exact water fingerprints. Proxy interfaces are generated with `node tools/generate-proxies.mjs <d3d9.h>`; do not hand-edit generated forwarding methods.

The DLL belongs beside `Neuz.exe`. Keep an existing `FENB.ini` when updating. A running game keeps its already loaded DLL and must be restarted to load a replacement.

Dear ImGui and Inter license notices ship with the build. No compatibility guarantee is made for other Flyff clients.
