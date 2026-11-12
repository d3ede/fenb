# FENB

FENB is a graphics mod for Flyff, inspired by ENBSeries. It uses a Windows x86 Direct3D 9 proxy, C++20, precompiled shaders and a native Dear ImGui settings panel.

## Controls and profiles

- **Shift + Enter** opens and closes the panel without opening game chat. Each opening centers the panel and returns to the main image page. The open panel can still be moved.
- **Effects** enables or disables all FENB rendering. Color correction, Bloom and WaterFX also have individual switches.
- Drag a slider, double-click its track to reset it, or click the number to type. The small arrow buttons use a parameter-specific step. The reset icon is shown for changed values.
- Edits save automatically to `FENB.ini`. **Ctrl + S** saves immediately.
- **Create…** beside the profile selector, or **Create profile…** inside its dropdown, saves the current settings under a name. Type the name and press **Save**; the profile appears in the selector. For a loaded custom profile the toolbar offers **Save…**. Existing names require the explicit **Replace profile** button. Files live in `FENB-presets`; compatible `.ini` files can also be copied into that folder.
- **WaterFX →** immediately after Bloom opens the water page. Its navigation arrow shares the reset-icon column. **Back** returns to the image controls.
- The reset icon in each section heading resets only that section's parameters. Effect enable switches, other sections, the global switch and the FPS option are preserved.

**Золотой** is the balanced starting profile: moderate wave shading and ripples, restrained glints, depth absorption, detailed SSR and restrained caustics. Five comparison profiles keep the same post-processing and optical settings, changing the indicated component:

| Profile | Comparison |
| --- | --- |
| Тест — гладкая вода | No macro-wave normals or ripples |
| Тест — мелкая рябь | Fine ripples, no macro-wave normals |
| Тест — длинные волны | Slow broad wave normals with little ripple |
| Тест — смешанные волны | Stronger wave normals and ripples |
| Тест — без SSR | Gold settings with only world SSR disabled |

The four earlier profiles (**Спокойная вода**, **Мягкая бирюза**, **Прозрачная вода**, **Насыщенная вода**) remain available. Profiles have native renderer verification; the new set is not live-client visual tuning or a promise of Flyff Universe parity. Selecting a profile is optional: installing the update does not overwrite `FENB.ini`.

## Native client frame pacing

The original flag-only unlock accelerated gameplay because the client calls its simulation once per main-loop iteration. The replacement installs a gate on the verified application's `FrameMove` virtual method before clearing the native frame limiter. Rendering remains independent; simulation uses the original 16 ms cadence (62.5 updates/s) and receives a fixed 0.016 s delta. Real wall clocks and network timers are not scaled. Short stalls allow bounded catch-up; long stalls start a fresh cadence without replaying a loading screen. Render-time animation interpolation is not added.

Enable **Снять лимит FPS** in the panel's actions menu, or set `unlock_fps=1` under `[General]`. It is off by default, independent of the effects switch, and preserved when changing image presets. Unknown clients are left unchanged. This implementation is pinned to the inspected Homesickness `Neuz.exe` SHA-256 `ce32e350c6002496acf9e8774275784971186e6144901f7c2c7d9b37a4aa41a1`; it verifies relocated frame gates, the application and its update method, and code/data protections. The in-memory vtable entry and limiter flag are restored when disabled or the last device is released. No EXE file or executable instructions are patched. Driver and VSync limits still apply.

Native tests invoke the actual x86 callback through a disposable vtable and compare simulation counts and elapsed time at 30, 60, 144, 500 and 1000 render FPS. These tests do not establish live gameplay compatibility: movement, attacks, jumping, world and UI animation still require comparison in the supported client. A high render count does not imply interpolated simulation or a guaranteed 500 FPS.

If an older loaded DLL has the flag-only unlock enabled, turn it off before continuing. Replacing the DLL takes effect only after restarting the client. Regression tests use disposable mapped images and callbacks; they do not modify a running game or installation.

## WaterFX implementation and limits

WaterFX replaces the material only for the two water texture families identified in the Homesickness Neuz render capture. Identification uses exact first-mip SHA-256 fingerprints, cached as texture-private metadata. The draw must also match the observed fixed-function layout and transparency state, belong to the main backbuffer and occur before the HUD. Unknown textures and unsupported states retain the original renderer. No game memory or vertex-buffer readback is used.

The material supplies animated wave normals, two drifting and rotated normal-map layers, adjustable opacity, a turquoise tint, GGX directional-light glints and Fresnel reflection. **Neuz mesh positions are preserved:** the wave-height control changes shading normals without lifting independently drawn tiles or river skirts. No geometric displacement or tessellation is added. Camera-relative interpolated positions keep geometric derivatives stable at large world coordinates. A deterministic 32-wave spectrum generates a single 128-pixel normal texture with a complete mip chain once. The original water texture RGB remains a separate, low default contribution; texture alpha preserves cutouts. Ambient light, material properties and the enabled directional light illuminate the water body. The analytic sky fallback follows that illumination at night.

Opacity is controlled by FENB and texture cutout coverage. Legacy vertex and material alpha are not multiplied into it: a zero vertex alpha channel previously made the material almost invisible and defeated the opacity control. The native water regression fixture now uses zero vertex alpha and verifies that both opacity settings still produce distinct visible results.

Refraction uses a GPU-only color snapshot taken once before the first matched water draw in each frame. It does not sample the live render target or recursively refract previously drawn FENB water. With valid depth, the material computes the optical path between the water surface and opaque scene, applies channel-dependent absorption, softens the shoreline and fades fine normals at contact. Refracted samples above the water plane or in front of its surface are rejected. Without valid depth, it retains uniform transparency and conservative distortion; foreground color leakage is still possible in that fallback. Only geometry already drawn before the snapshot is available.

Sky reflection captures the positively identified `skybox01` and `cloud01` fixed-function geometry into a 128-pixel cubemap, updated at most four times per second. Only their original DrawPrimitive calls are repeated, with six camera orientations, the original texture stages and no depth buffer; no world scene is replayed or retained. The water samples this actual sky/cloud rendering with its reflected normal. Unknown sky textures or unsupported paths use the analytic gradient. A frame without a matched sky cannot reuse a stale map from another scene. The native fixture verifies the capture, cubemap selection, main-target preservation and state restoration; appearance in the live client still needs checking after restart.

Depth-based optics currently use the optional NVIDIA driver path: registered D16/D24 scene depth is copied into an INTZ texture through [NvAPI_D3D9_StretchRectEx](https://docs.nvidia.com/nvapi/group__dx.html). The native fixture verifies actual sampling of the resolved depth and rendering with both D16 and D24 at zero and 8x MSAA. The copied source remains the actual depth surface used for scene rendering. Driver functions are loaded only from the system directory; no NVAPI redistributable is required. Unsupported drivers, formats, projections or failed copies use the fallback material. RESZ support alone is not assumed.

SSR traces the copied opaque depth with 12 or 24 steps and a bounded refinement search. Its ray uses the actual geometric plane, independently of animated wave normals. Accepted world-color samples receive no additional screen-pixel warp; waves change lighting and Fresnel strength without relocating reflected objects. A source-format-aware depth tolerance and silhouette-discontinuity rejection discard loose hits. Steep skirts suppress reflection, refraction distortion and specular glints. Refraction rejection tests the local plane in view space, including sloped river sections. Missing hits fade to the sky environment.

SSR still sees only visible, previously rendered geometry: off-screen objects, hidden surfaces and later transparent passes cannot be reconstructed. There are **no complete planar world reflections or reflection scene replay**. D16 precision, screen-space disocclusion and defects already present in the source geometry can still produce artifacts; stability tests are not proof that every map is seam-free. Sun glints use the enabled directional light. SunFX, SkyFX, new shadowing and depth-based atmosphere remain separate work.

The optional **24-bit water depth** action (`precise_water_depth=1`, enabled by default) upgrades only an automatically allocated D16 buffer in the pinned Homesickness client, at CreateDevice/Reset. Format, depth-stencil and the existing MSAA quality are checked before use. Allocation failure retries the original D16 parameters; Lost Device is propagated. Color format, dimensions and MSAA are unchanged. The option requires a client restart. It adds roughly 32 MiB of driver depth storage at 1920x1080 with 8x MSAA; actual driver allocation may differ. Distant shallow-water stripes may come from D16 precision, but fixing every reported beach artifact is not yet live-verified.

Optional caustics reconstruct receiving positions and normals from copied opaque depth and evaluate refracted directional-light focusing in a separate reduced-resolution pass. A finite-difference ray-map Jacobian gives bright and dark concentration patterns, with a small wavelength-dependent IOR variation controlled by **Dispersion**. The field is sampled only by underwater transmission; SSR reads the unchanged scene snapshot. Focusing uses an approximate two-world-unit slab rather than a full ray trace through arbitrary water geometry. Real optical path length fades the result; directional light controls day/night strength. Flat normals produce no patterned focusing. This is not volumetric scattering, light-occlusion tracing or a generic RGB screen filter. Existing profiles default to zero caustics; Gold and the five comparison profiles enable 0.45.

Color and depth optics need one full-size 32-bit color target and one INTZ depth texture, copied at most once per water-containing frame. Together with the 384 KiB sky cubemap and 85 KiB normal mip chain, this is about 15.2 MiB of owned GPU resources at 1920×1009 before the optional caustic field. The field adds about 0.46 MiB at quarter dimensions or 1.85 MiB at half dimensions and one pass per water-containing frame. There is no CPU image readback or persistent copy of game geometry in production. Zero refraction, absorption, SSR and caustics together release the color target; zero absorption, SSR and caustics release the depth texture; zero sky contribution releases the cubemap. Normals retain their small managed texture until Reset or teardown. The pixel shader fits the 512-slot Shader Model 3 budget. Resources are released for Lost Device/Reset and recreated lazily. Native fixtures check object reflections, shallow/deep transmission, caustic receiver isolation, day/night strength, flat-water neutrality, both field resolutions, resource release and restoration; they do not establish live FPS cost or visual parity with another game.

The stability fixture also compares zero and maximum wave amplitude: reflected-object support must overlap by at least 90%, and a constant-color rendering of two water levels joined by a vertical skirt must remain pixel-identical. It runs at the origin and near world coordinates (7000, 100, 3300), with both non-MSAA and 8x MSAA paths.

Restoration also preserves the input-binding changes required by [DrawPrimitiveUP](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawprimitiveup) and [DrawIndexedPrimitiveUP](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-drawindexedprimitiveup). Native regression tests exercise both paths.

Post-processing runs before the first screen-space HUD draw, preserving interface colors and later 3D portraits. Present and swap-chain Present remain supported.

## Build and checks

The local portable LLVM-MinGW toolchain lives under `.tools/llvm-mingw-*-ucrt-x86_64`. Run `./build.ps1 -Tests`, `./dist/FENB-preview.exe` and `node tools/verify-binary.mjs`. The test build checks settings, exact HUD colors, native water material selection and restoration, transparency, Reset, input handling and panel interactions. The water fixture test reads the locally installed DDS; it reports a skip when that optional game asset is absent. Test screenshots and configuration files stay in ignored build folders.

Use `-Diagnostics` only for the opt-in bounded pipeline capture. The normal build has no capture code. The diagnostic menu delays a three-frame capture by three seconds so the panel can be closed and a wake or splash reproduced. Native rings and splash particles are still rendered by the game; their draw states must be captured before integration. A CMake x86 build is also described in `CMakeLists.txt`; the PowerShell/LLVM build is the verified local path.

`node tools/water-catalog.mjs <World/Texture directory>` regenerates the checked-in exact water fingerprints. Proxy interfaces are generated with `node tools/generate-proxies.mjs <d3d9.h>`; do not hand-edit generated forwarding methods.

The DLL belongs beside `Neuz.exe`. Keep an existing `FENB.ini` when updating. A running game keeps its already loaded DLL and must be restarted to load a replacement.

Dear ImGui and Inter license notices ship with the build. No compatibility guarantee is made for other Flyff clients.
