# UI v2 concepts

A standalone, native x86 ImGui / DirectX 9 interface study for FENB, a graphics mod for Flyff inspired by ENBSeries. The production panel is unchanged. This concept does not load the proxy, connect to a game, read or write settings, or apply graphics effects.

Build with the existing portable toolchain:

```powershell
./tools/build-ui-v2.ps1
./tools/build-ui-v2.ps1 -Variant studio -Run
./tools/build-ui-v2.ps1 -Variant violet -Run
```

The default build produces three separately launchable previews from the same source:

| Executable in `build/ui-v2/` | Design |
| --- | --- |
| `FENB-ui-v2-studio.exe` | Revised A: compact 560 × 540 settings panel based on B, with top tabs, the approved horizontal logo, muted violet controls and 2 px corners. |
| `FENB-ui-v2-violet.exe` | Variant B: the compact 560 × 540 layout with saturated violet slider fills, light lavender thumbs, purple enabled switches and animated selection colors. |
| `FENB-ui-v2-compact.exe` | The previous neutral compact study for comparison. |

`-Variant` builds the selected launch target; `-Run` opens it. With the default `all` variant, `-Run` opens A and B. Keep these executables in the separate preview directory; do not place them next to the production proxy DLL. The build copies the Inter and Dear ImGui notices alongside them. Executable names select the initial design; `--studio` and `--violet` can also select it explicitly.

The panels keep the production panel's top tabs, profile strip, dense parameter rows and section headers. Tab widths follow the translated text, with matching text-width underlines and equal hover padding. Variant B retains graphite menus and uses saturated `#8F53EF` violet for active controls, with `#C7A6FF` highlights; the neutral study keeps gray switches and slider fills. Revised A uses muted `#9074B1` violet, `#BEABD5` highlights, thinner slider fills and smaller thumbs. It replaces the earlier sidebar and navigation cards with the compact top tabs and simple navigation rows from B. Parameter rows are 28 px tall, on one continuous background. Controls and the outer panel use 2 px corners; switches retain their pill shape. Promotional subtitles and extra preview labels are omitted.

Variant A uses the approved horizontal lockup as one image, preserving its lettering, proportions and glow. `tools/embed-ui-v2-logo.mjs` embeds the existing `assets/branding/header-preview/horizontal.bgra` texture into ignored `build/ui-v2/ui_studio_logo.hpp`. This runs as part of the preview build and requires no image renderer or runtime asset files. The approved kit and production logo embedding remain unchanged.

All variants share fixed columns for sliders, step arrows, editable numbers and individual resets. Section headers include collapse arrows, section resets and enable switches; collapsed groups reflow vertically. Navigation and reset icons use vector strokes instead of font glyphs. Switches and hover feedback animate smoothly, with a short page-entry transition. The ellipsis opens Appearance, with a motion switch and a native-cursor fallback.

All controls only change disposable in-memory demo values. Try Image, its atmosphere and WaterFX subpages, World, the profile strip, English/Russian selector, and Appearance. Reset restores demo values while preserving the chosen language and appearance. Escape closes the preview. No state is saved between launches.

The cursor is a single closed notched triangle in `assets/cursors/fenb-pointer.svg`, with an ivory fill and thin muted violet outline. It has a 12 × 12 canvas, a shorter lower point and a hotspot at (1, 1). There is no tail, glow, movement smoothing or click ring. `tools/embed-cursor.mjs` generates the preview geometry from the SVG under ignored `build/ui-v2/`; do not edit that generated header. The matching editable Figma asset is in [Launcher, FENB — Cursor](https://www.figma.com/design/QG0X99mhi6CLsRmIrTWLGY/Launcher?node-id=24-11).

English is the default. `--russian` opens the Russian view. `--capture` writes sixteen screenshots (four navigation pages, two effects subpages, and hovered active/inactive Image navigation items, in both languages) and exits, returning a failure code if capture fails. A writes captures under `build/ui-v2/studio/`; B uses `build/ui-v2/violet/`; the neutral design writes beside its executable. Captures and binaries remain in ignored `build/` output. The version shown in the lower-left is the existing product version; “UI v2” denotes a design study, not a released product version.
