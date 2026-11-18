# Meadow grass visual study

This is an isolated 3 by 3 metre review patch for a Simple grass direction closer
to the supplied Flyff Universe references. It does not replace `GrassFX`, change
Detailed grass or install anything in a game directory.

## Run

Use the existing portable LLVM-MinGW toolchain and an installed client whose
ground artwork is available locally:

```powershell
./tools/render-meadow-preview.ps1 -ClientArtworkRoot '<client-directory>' -OutputDirectory artifacts/meadow-universe-review
```

The script compiles a separate x86 D3D9 renderer in `build/` and restricts
disposable captures to `artifacts/`. It produces `summer-close.png`,
`summer-game.png`, `winter-close.png`, `winter-game.png`, matching `*-ground.png`
baselines, and `summer-atlas.png` and `winter-atlas.png`. `study.jsonl` records
geometry counts and nonempty-image checks. `manifest.json` records source and
artwork hashes, atlas dimensions and limitations. Repeated runs replace this set.

The client is read-only. Only `ground_lawn001.dds` and `ground_n_ice02.dds` under
`World/Texture/` are loaded. No game process, configuration, DLL or source texture
is changed. Native ground artwork is not redistributed with FENB.

## Artwork and sampling

The previous native 128 by 128 grass card established the desired thin, curved
direction, but magnification exposed its pixel silhouette and repeated shape.
The new original artwork is editable cubic-curve source in
`tools/meadow_artwork.hpp`. It paints two 2048 by 2048 RGBA atlases, each with four
1024 by 1024 variants. This is freshly drawn artwork, not an enlargement or blur
of the native bitmap. A discarded AI-generated draft is not used by the tool.

Each variant has twelve flat shaded leaves with distributed roots, short inner
leaves, mixed heights, pointed tips and smooth bends. Leaf widths vary along the
curve. A darker face, broad green lit face and restrained lime ridge make the
leaf read as a ribbon rather than a uniformly colored wire. Roots fade into the
substrate. Continuous pixel coverage provides filtered alpha edges, and the
renderer checks for partially covered edge pixels and transparent tile gaps.

Mips use alpha-weighted RGB filtering; RGB padding in owned texture copies avoids
black borders. Alpha blending retains the thin filtered edges after a small
rejection threshold. Hardware MSAA is enabled when available; MSAA alone does
not smooth the inside of an alpha-textured polygon.

## Current comparison

The fixed-seed summer patch still contains 119 cards. Greater leaf readability
comes from the new artwork, not an increase in summer card count. Each carrier
has four height and four width segments, totaling 32 triangles. Its root line
bends in depth so the bases do not all form an obvious straight fence. Smooth
edge and low-frequency density masks retain openings and denser areas.

| Criterion | Current observation against the Universe reference |
| --- | --- |
| Silhouette | Flat green ribbons replace the previous yellow thread-like stems. Short leaves fill some lower gaps. |
| Curvature | Cubic leaf curves and gently warped carriers retain pointed, curved ends. Close views still reveal that this is card artwork. |
| Color | Deeper green faces contrast with lime lit faces. The supplied daylight scene has stronger lighting and a different ground treatment, which are not reproduced here. |
| Density | The same 119 roots retain irregular openings. The reference field is more interwoven; this is still a small patch, not a whole-field rollout. |
| Ground visibility | Lawn remains visible between stems and through the alpha gaps. Root fading softens the horizontal card seam. |

Winter now uses 102 cards instead of the earlier 28. Its height is 82% of summer
and width 94%, rather than the previous 48% and 72%. A separate painted palette
retains olive and dry-leaf tones with cooler frost concentrated toward the lit
tips. It is no longer a uniform luma-to-gray recolor or a handful of tiny beige
marks. This is a visual proposal, not a verified live Azria fix.

The close camera is at `(0.15, 0.78, -2.8)` looking at `(0, 0.23, 0)`. The raised
camera is at `(1.9, 2.0, -2.6)` looking at `(0, 0.12, 0)`, approximating gameplay.
There is no wind, LOD, terrain-mask replay, fog, HUD or live lighting. Cards are
sorted by root depth, so intersecting translucent cards can still show ordering
artifacts. Camera-angle stability, integration and live performance require
separate validation after the visual direction is accepted.
