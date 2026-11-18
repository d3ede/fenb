# Meadow grass visual study

This is a 3 by 3 metre study of individually rooted, textured grass leaves for
review against the supplied Flyff Universe references. It does not replace
`GrassFX`, change Detailed grass or install anything in a game directory.

## Run

```powershell
./tools/render-meadow-preview.ps1 -ClientArtworkRoot '<client-directory>' -OutputDirectory artifacts/meadow-leaves-review
```

The script uses the existing portable LLVM-MinGW toolchain to build an isolated
x86 D3D9 renderer. Captures are restricted to `artifacts/`. It writes summer and
winter `*-close.png` and `*-game.png` patch views, matching `*-ground.png`
baselines, and `summer-leaf-detail.png` / `winter-leaf-detail.png` single-leaf
views. `study.jsonl` records actual leaf and triangle counts and image checks;
`manifest.json` records source, material and native ground hashes.

Only the client's `World/Texture/ground_lawn001.dds` and `ground_n_ice02.dds` are
read. No game process, client texture, DLL or configuration is changed. Native
client artwork is not redistributed with FENB.

## Current geometry and materials

The dense card experiment was rejected. It retained the same planting-site count
but hid many additional leaves inside each painted card, so it still increased
the visible density and exposed flat faces at some angles.

The current fixed-seed patch has 485 separate summer leaves, about 54 per square
metre of the entire patch, and 403 winter leaves. This raises summer leaf density
by about 24% from the previous 392-leaf review while retaining the same irregular
density mask, leaf shapes and materials. Each leaf has its own jittered
root, height, direction, bend and twist. Roots follow a smooth irregular density
mask and a soft boundary. There is no shared ornamental-plant root and no painted
undergrowth adding an unreported second population. Ground remains visible
between the actual modeled leaves.

Each leaf is a tapered ribbon with eight height segments and two folded faces,
32 triangles total. A painted [leaf surface](../assets/grass/leaf-surface.png)
supplies pigment changes, fibers and veins; varying UV windows and orientation
lighting make the faces differ. Silhouettes come from geometry, so hardware MSAA
can smooth their actual edges. There is no grass alpha-card painting or vector
atlas generator in this path. Summer has 15,520 triangles in this review patch;
this is not a tested whole-field performance budget.

Winter roots are sunk into the snow and the leaves are shorter. A separate
expanded coating follows the upper leaf surface, with rounded width variation,
a raised surface and a softly blended starting edge. Its separate painted
[snow material](../assets/grass/snow-surface.png) gives white powder and cool
recesses. Lower stems remain partly exposed. This is attached geometry, not a
gray recolor, and it is not a simulation of accumulating weather. The winter
patch has 20,324 triangles including the coating.

The materials are original `image_gen` outputs. Their full prompts and provenance
are tracked under [assets/grass](../assets/grass/README.md); no raw Universe
texture has been extracted or used.

## Visual review and limitations

| Criterion | Current comparison target and remaining limitation |
| --- | --- |
| Silhouette | Narrow pointed leaves with independent roots replace visible card-shaped clumps. The source reference still has a different mix of leaf shapes. |
| Curvature | Continuous polynomial bends and twist are sampled into eight height segments; curves can still reveal segmentation in an extreme close-up. |
| Color | Painted pigment and veins replace clean gradients; world lighting, shadowing and ground treatment from the reference are not reproduced. |
| Density | Actual leaf count is explicit and substantially lower than the previous multi-leaf card field. The patch is intentionally open for density review. |
| Ground visibility | Soil or snow is visible between independent leaves, with no opaque card base or hidden painted lower layer. |

The patch cameras are unchanged: close `(0.15, 0.78, -2.8)` toward `(0, 0.23, 0)`,
and raised `(1.9, 2.0, -2.6)` toward `(0, 0.12, 0)`. Single-leaf views inspect the
same geometry and materials at a closer scale. There is no wind, LOD, terrain-mask
replay, fog, HUD, contact shadows or live client lighting. Full client performance,
camera-angle stability and live Azria appearance need separate validation after
visual review. No whole-field rollout is part of this study.
