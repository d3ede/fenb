# Standalone feature comparisons

The 18 November 2026 showcase renders procedural scenes outside the game using
FENB's actual DX9 grass, material, tree, particle and scene-effect implementations.
It also exercises the shipping actor-visibility predicate with synthetic actors.
The scope and captions are recorded in `tools/feature-showcase.json`.

The output includes 47 lossless 1600 × 900 source renders, 16 branded comparison
cards in stacked and split layouts, an overview, and an offline HTML gallery with
draggable comparison dividers, category filters and full-size variant links.
The additional frames show grass height/density, close-up bark, puddle filling,
rain rings, refinement fading/colors and separate actor draw distances.

## Generate

Use the portable toolchain under `.tools/` and the installed system D3DX9 texture
loader. Depth-based comparisons require the supported NVIDIA depth-copy path.
The tool fails if that path is unavailable instead of exporting an inactive
effect as a working comparison.

Run from the repository root, supplying a local client artwork directory:

```powershell
./tools/build-feature-showcase.ps1 `
    -ClientArtworkRoot 'X:/Games/Homesickness' `
    -OutputDirectory 'artifacts/feature-showcase-my-export'
```

The directory must contain `World/Texture`, `Model/Texture` and `SFX/Texture` with
the supported artwork named in `tools/render-feature-showcase.cpp`. These files
are opened read-only. The tool does not start or modify the game, its DLL,
configuration or profiles. Original client textures are not copied into source
control. All output stays under the checkout's ignored `artifacts/` directory.

An existing destination is rejected by default. To rebuild labels and the gallery
from a completed render set without running the renderer again:

```powershell
./tools/build-feature-showcase.ps1 `
    -ClientArtworkRoot 'X:/Games/Homesickness' `
    -OutputDirectory 'artifacts/feature-showcase-my-export' `
    -ReuseRaw
```

Open the resulting `index.html` locally. `pairs/` contains the stacked PNGs;
`splits/` contains the divided PNGs; `raw/` preserves the original GPU captures.
The overview is `00-overview.png`. Branding uses the approved transparent
horizontal logo without changing its proportions or palette.

## Comparison controls and provenance

Each comparison fixes source geometry, camera, lighting, random seed and animation
phase. The standalone build injects `tools/feature-showcase-clock.hpp` to freeze
the animation clock; this header is not used by normal FENB builds. Wetness alone
advances through deterministic one-second steps using the production accumulation
code. Each wetness still starts with a cleared accumulation state. The demo's
20-second fill time is recorded in its captions.

The renderer verifies 19 nonempty off/on pairs, including the extra bark and
individual-distance fixtures. It requires material, tree and particle draw paths
to activate, and asserts that the local player and peaceful NPC remain visible.
`raw/pixel-checks.json` records the number of changed pixels in each pair.
`renderer-build.json` records the source revision, version and source hashes;
`manifest.json` adds artwork hashes, output hashes, parameters and implementation
references. The hashes also distinguish local source modifications from the
recorded Git revision. Repackaging retains the original build provenance.

“Original” labels refer to the same standalone scene with the named feature off.
The SSAO regression uses explicit previous/fixed labels because both frames have
SSAO enabled. Puddle cards use dry/wet labels; color cards use game/custom labels.
The scene is a demonstration rather than a reconstruction of a complete Flyff
map. Actor shapes, paving and fading grass cards are synthetic fixtures. Tree
replacement and snow materials use normal recognition of the supplied textures.
The comparisons demonstrate rendering behavior; they do not establish live-client
performance, combat behavior or compatibility with additional clients.

## Verify changes to the tool

Run `./build.ps1 -Tests` and `node tools/verify-binary.mjs`, generate a fresh
showcase, and visually inspect the overview, full-size pairs and gallery controls.
Inspect `git diff --check` before committing tool or catalog changes. Keep render
outputs and client artwork outside commits.
