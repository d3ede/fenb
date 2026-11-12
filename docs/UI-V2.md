# UI v2 concepts

A standalone, native x86 ImGui / DirectX 9 interface study for FENB, a graphics mod for Flyff inspired by ENBSeries. The revised A design is now also used by the production panel. The standalone concept still does not load the proxy, connect to a game, read or write settings, or apply graphics effects.

The production adaptation retains every effect parameter, supported-client checks, numeric input, step arrows, resets, built-in and named profiles, autosave and keyboard shortcuts. Long sections scroll within the fixed panel. It uses the same muted violet palette and rectangular ivory-lavender slider thumbs, a symbol-only header, EN/RU buttons, compact reset and profile creation controls, and a persistent Unlock FPS footer toggle. Appearance is available from the ellipsis menu; its cursor and motion options are saved as `custom_cursor` and `interface_motion` under `[Interface]`. Profiles and effect resets preserve these preferences. Narrow displays use a second header row so controls do not overlap. `tools/embed-cursor.mjs` generates both the production header and the standalone preview header from the same editable SVG.

Build with the existing portable toolchain:

```powershell
./tools/build-ui-v2.ps1
./tools/build-ui-v2.ps1 -Variant studio -Run
./tools/build-ui-v2.ps1 -Variant violet -Run
```

The default build produces three separately launchable previews from the same source:

| Executable in `build/ui-v2/` | Design |
| --- | --- |
| `FENB-ui-v2-studio.exe` | Revised A: compact 560 × 540 settings panel based on B, with top tabs, the production symbol-only logo, muted violet controls and 2 px corners. |
| `FENB-ui-v2-violet.exe` | Variant B: the compact 560 × 540 layout with saturated violet slider fills, light lavender thumbs, purple enabled switches and animated selection colors. |
| `FENB-ui-v2-compact.exe` | The previous neutral compact study for comparison. |

`-Variant` builds the selected launch target; `-Run` opens it. With the default `all` variant, `-Run` opens A and B. Keep these executables in the separate preview directory; do not place them next to the production proxy DLL. The build copies the Inter and Dear ImGui notices alongside them. Executable names select the initial design; `--studio` and `--violet` can also select it explicitly.

The panels keep the production panel's top tabs, profile strip, dense parameter rows and section headers. Tab widths follow the translated text, with matching text-width underlines and equal hover padding. Variant B retains graphite menus and uses saturated `#8F53EF` violet for active controls, with `#C7A6FF` highlights; the neutral study keeps gray switches and slider fills. Revised A uses muted `#9074B1` violet, `#BEABD5` highlights, thinner slider fills and 5 × 10 px rectangular thumbs in ivory lavender (`#EBE2F7`). It replaces the earlier sidebar and navigation cards with the compact top tabs and simple navigation rows from B. Parameter rows are 28 px tall, on one continuous background. Controls and the outer panel use 2 px corners; switches retain their pill shape. Promotional subtitles and extra preview labels are omitted.

Variant A uses the same approved symbol texture as the production panel, with a 30 px width, 12 px left inset and no separator before the tabs. There is no separate FENB wordmark. The preview reuses `src/generated/ui_logo.hpp`; the approved kit and production logo embedding remain unchanged.

Its profile strip ends with a 24 × 24 reset icon and a localized “Reset all” tooltip. The profile selector occupies the remaining width, with an 8 px gap before the reset control.

All variants share fixed columns for sliders, step arrows, editable numbers and individual resets. Section headers include collapse arrows, section resets and enable switches; collapsed groups reflow vertically. Navigation and reset icons use vector strokes instead of font glyphs. Switches and hover feedback animate smoothly, with a short page-entry transition. Dropdown chevrons rotate smoothly on opening and closing; disabling motion makes the change immediate. The ellipsis opens Appearance, with a motion switch and a native-cursor fallback. In revised A these occupy two compact 34 px rows, without cursor samples or descriptive captions. An independent Unlock FPS toggle stays visible at the right of the footer on every page; it only changes preview state and does not affect real frame pacing.

All controls only change disposable in-memory demo values. Try Image, its atmosphere and WaterFX subpages, World, the profile strip, English/Russian selector, and Appearance. Reset restores demo values while preserving the chosen language and appearance. Escape closes the preview. No state is saved between launches.

The cursor is a single closed notched triangle in `assets/cursors/fenb-pointer.svg`, with an ivory fill and thin muted violet outline. It has a 12 × 12 canvas, a shorter lower point and a hotspot at (1, 1). There is no tail, glow, movement smoothing or click ring. `tools/embed-cursor.mjs` generates the preview geometry from the SVG under ignored `build/ui-v2/`; do not edit that generated header. The matching editable Figma asset is in [Launcher, FENB — Cursor](https://www.figma.com/design/QG0X99mhi6CLsRmIrTWLGY/Launcher?node-id=24-11).

English is the default. `--russian` opens the Russian view. `--capture` writes eighteen screenshots (four navigation pages, two effects subpages, hovered active/inactive Image navigation items, and the open antialiasing dropdown, in both languages) and exits, returning a failure code if capture fails. A writes captures under `build/ui-v2/studio/`; B uses `build/ui-v2/violet/`; the neutral design writes beside its executable. Captures and binaries remain in ignored `build/` output. The version shown in the lower-left is the existing product version; “UI v2” denotes a design study, not a released product version.
