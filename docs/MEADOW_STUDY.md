# Meadow grass visual study

This is a review prototype for a different Simple grass direction. It does not
replace `GrassFX`, change Detailed grass or install anything in a game directory.
The previously proposed compact, broad-leaf tuft is rejected: its shared root,
thick folded leaves and restricted bends produce decorative plants rather than
the thin, arching grass in the supplied native Flyff reference.

## Run

Use the existing portable LLVM-MinGW toolchain and an installed client whose
artwork is available locally:

```powershell
./tools/render-meadow-preview.ps1 -ClientArtworkRoot '<client-directory>' -OutputDirectory artifacts/meadow-study
```

Only the supplied artwork is read. No game process, configuration, DLL or source
texture is changed. The script restricts disposable captures to `artifacts/`.
It compiles a separate x86 D3D9 renderer in `build/` and writes four captures:
`summer-close.png`, `summer-game.png`, `winter-close.png`, `winter-game.png`.
Each has a matching `*-ground.png` baseline. `study.jsonl` records the card and
triangle counts and nonempty-image checks; `manifest.json` records the source
revision, renderer hash, native artwork hashes and limitations. Repeated runs
replace the capture set and metadata.

The tool reads `Obj_MaCoPrGr07.dds` from
`Model/Texture/`, and `ground_lawn001.dds` and `ground_n_ice02.dds` from
`World/Texture/`. These are native client assets, not redistributed FENB assets.

## Current comparison

The 3 by 3 metre coordinate patch contains 119 summer cards and 28 winter cards
with the fixed seed. Each card has four curved height segments. The silhouette
comes from native painted alpha artwork, not a mesh of opaque wide leaves.
Short, medium and tall cards are mixed; position, orientation and scale vary.
A smooth edge fade and low-frequency density mask create openings and denser
areas. The opaque native band texture is deliberately excluded because it
produced solid hedge-like bases. The short fan artwork is also excluded: its
dense pointed base became a noticeable small shrub, especially in the snow test.

| Criterion | Current observation against the supplied reference |
| --- | --- |
| Silhouette | Thin native stems and arching tips replace the thick tuft leaves. Repeated atlas shapes are still recognizable at close range. |
| Curvature | Painted stems supply continuous curves; the carrier adds restrained depth bow and twist. This is no longer the rejected shrub mesh. |
| Color | Green bases and pale yellow-green upper stems are readable. Directional shading varies with card orientation. Live terrain lighting is not reproduced. |
| Density | Uneven overlapping groups retain open ground. The reference has more interwoven fine stems in its densest areas; this study does not establish the final density. |
| Ground visibility | The native lawn remains visible between stems and through the cards. Winter intentionally leaves substantially more snow exposed. |

Winter does more than recolor a dense summer field gray. It retains only about
a quarter of the cards, reduces height to 48% and width to 72%, and uses pale dry
stems with restrained frost on the brighter upper portions. This tests a much
quieter silhouette than the dense gray blanket in the supplied Azria screenshot.
It is an art-direction proposal, not a verified live Azria fix.

Filtered alpha edges use blending after a small alpha rejection threshold. RGB
padding in owned texture copies avoids black transparent-border fringes; source
DDS files and alpha values remain unchanged. Cards are sorted by root depth, so
intersections can still show ordering artifacts. The fixed-function vertex path
uses a Shader Model 2 pixel shader. Hardware MSAA is enabled when available.

The close camera is at `(0.15, 0.78, -2.8)` looking at `(0, 0.23, 0)`. The raised
camera is at `(1.9, 2.0, -2.6)` looking at `(0, 0.12, 0)`, approximating a gameplay
view. These are isolated flat-patch renders, not captures from the running client.
There is no wind, LOD, terrain-mask replay, fog, HUD or live lighting. Camera-angle
stability, integration and performance still require separate validation after
the visual direction is accepted. No whole-field rollout is part of this study.
