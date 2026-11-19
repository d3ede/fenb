# Meadow grass visual study

This is a 3 by 3 metre review patch for Simple grass, using refreshed vanilla-style
alpha artwork. The previous study of 485 isolated modeled leaves was rejected:
its thin, disconnected ribbons did not produce the interwoven meadow silhouette
of the supplied Flyff Universe reference. Increasing that population slightly
did not fix the missing lower leaves, painted folds or overlapping curved tips.

The offline tool remains a visual reference. Its painted strip is now also
integrated into the Simple `GrassFX` style; Detailed grass retains its previous
geometry and shading. Installation is a separate, explicitly requested action.

## Run

```powershell
./tools/render-meadow-preview.ps1 -ClientArtworkRoot '<client-directory>' -OutputDirectory artifacts/meadow-vanilla-review
```

The script builds an isolated x86 D3D9 renderer with the existing portable
LLVM-MinGW toolchain. Output is restricted to `artifacts/`. It writes summer and
winter `*-close.png` and `*-game.png` views, single `*-strip-detail.png` views,
and `native-summer-close.png` / `native-summer-game.png` comparisons using the
same planting sites and cameras. Each view has a matching ground-only baseline
and a `*-coverage.png` mask of changed pixels for checking transparent gaps.
`study.jsonl` records actual strip/triangle counts and nonempty-image checks;
`manifest.json` records source, material and native artwork hashes.

The client is read-only. Only `World/Texture/ground_lawn001.dds`,
`World/Texture/ground_n_ice02.dds` and `Model/Texture/Obj_MaCoPrGr07.dds` are read.
No client process, DLL, configuration or source texture is changed. Native
artwork is not redistributed with the project.

## Current approach

The vanilla grass sprite is a 128 by 128 alpha texture with multiple leaves,
different arching tips, green lower faces and yellow-green highlights. It
provides a structural baseline, rather than treating one smooth opaque leaf
material as the whole visual vocabulary.

The refreshed [1254 by 1254 RGBA strip](../assets/grass/vanilla-meadow.png) was
redrawn with `image_gen` from that baseline and the user's Universe reference.
It has painted veins, folded faces, curved tapered leaves, mixed heights and
short lower leaves. Roots are distributed across its width. Generation and
refinement prompts and reference roles are tracked in
[assets/grass](../assets/grass/README.md). No raw Universe texture was extracted.
D3DX currently expands this source to a 2048 by 2048 upload; that upload size
does not add detail to the source artwork.

The fixed-seed patch contains 213 independently positioned strips in summer and
195 in winter. These counts describe carrier meshes, not individual painted
leaves: each strip contains multiple blade silhouettes. There is no claim that
213 strips are less dense than the rejected 485-leaf study. Each carrier has
four height and four width segments, 32 triangles total; summer has 6,816
triangles and winter has 6,240. Width, height, orientation, bend, mirror and tone
vary across the patch. A smooth spatial mask retains irregular openings and a
soft boundary. This is a small review patch, not a whole-field performance test.

Alpha bounds trim transparent margins during sampling so roots meet the ground.
Owned texture copies receive alpha-weighted mipmaps and RGB padding, preserving
leaf color at filtered edges without modifying the source image or DDS. Low-alpha
haze in the refreshed artwork is remapped and clipped before blending; the native
comparison retains its small rejection threshold. Carriers are sorted by
root depth and render without depth writes; intersecting strips can still show
ordering artifacts. Hardware MSAA smooths carrier edges, not alpha silhouettes.

Winter keeps most planting sites, shortens the carriers and sinks their bases
slightly. Muted olive lower leaves transition to a separate painted snow surface
toward the upper parts. This is texture shading within the alpha silhouette; it
does not have physical snow thickness or simulate weather accumulation.

## Visual comparison and limitations

| Criterion | Current observation and remaining difference |
| --- | --- |
| Silhouette | Overlapping curved tips and short lower leaves replace isolated straight-looking ribbons. Native comparisons preserve the original sprite's more irregular arches. |
| Surface | The refreshed strip has painted green folds, veins and lime edges; the native 128-pixel image exposes visible steps in close views. |
| Density | The patch now reads as interwoven growth with ground openings, rather than a few separated stems. Painted leaf count must not be confused with carrier count. |
| Color | Green faces and yellow-green highlights are present; the reference's world lighting, shadows and ground treatment are not reproduced. |
| Winter | Snow is visible on upper foliage while lower leaves remain subdued. Live Azria appearance remains unverified. |

The patch cameras remain close `(0.15, 0.78, -2.8)` toward `(0, 0.23, 0)` and
raised `(1.9, 2.0, -2.6)` toward `(0, 0.12, 0)`. There is no wind, LOD,
terrain-mask replay, fog, HUD, contact shadows or live client lighting. Camera
stability and performance in the client need separate validation after visual
review. See `docs/RENDERING.md` for the runtime integration and its limitations.
