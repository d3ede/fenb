# UI v2 concept

A standalone, native x86 ImGui / DirectX 9 interface study for FENB, a graphics mod for Flyff inspired by ENBSeries. The production panel is unchanged. This concept does not load the proxy, connect to a game, read or write settings, or apply graphics effects.

Build with the existing portable toolchain:

```powershell
./tools/build-ui-v2.ps1 -Run
```

Or open `build/ui-v2/FENB-ui-v2-compact.exe` after building. Keep the executable in this separate directory; do not place it next to the production proxy DLL. The build copies the Inter and Dear ImGui notices alongside it.

The 560 × 540 panel keeps the production panel's top tabs, profile strip, dense parameter rows and section headers. Tab widths follow the translated text, with matching text-width underlines and equal hover padding. Graphite menus and neutral switches keep violet limited to the approved Refined Bloom symbol, active tab underline and interaction details. Parameter rows share fixed columns for sliders, step arrows, editable numbers and individual resets. Section headers include collapse arrows, section resets and enable switches; collapsed groups reflow vertically. Navigation and reset icons use vector strokes instead of font glyphs. Hover feedback and page transitions are intentionally subtle. The ellipsis opens Appearance, with a motion switch and a native-cursor fallback.

All controls only change disposable in-memory demo values. Try Image, its atmosphere and WaterFX subpages, World, the profile strip, English/Russian selector, and Appearance. Reset restores demo values while preserving the chosen language and appearance. Escape closes the preview. No state is saved between launches.

The cursor is a single closed notched triangle in `assets/cursors/fenb-pointer.svg`, with an ivory fill and thin muted violet outline. It has a 12 × 12 canvas, a shorter lower point and a hotspot at (1, 1). There is no tail, glow, movement smoothing or click ring. `tools/embed-cursor.mjs` generates the preview geometry from the SVG under ignored `build/ui-v2/`; do not edit that generated header. The matching editable Figma asset is in [Launcher, FENB — Cursor](https://www.figma.com/design/QG0X99mhi6CLsRmIrTWLGY/Launcher?node-id=24-11).

English is the default. `--russian` opens the Russian view. `--capture` writes sixteen screenshots (four navigation pages, two effects subpages, and hovered active/inactive Image tabs, in both languages) beside the executable and exits, returning a failure code if capture fails. Captures and binaries remain in ignored `build/` output. The version shown in the lower-left is the existing product version; “UI v2” denotes a design study, not a released product version.
