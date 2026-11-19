# Grass review artwork

`vanilla-meadow.png` is the current 1254 by 1254 RGBA review sprite: a strip of
overlapping curved grass leaves with distributed roots, mixed heights, painted
folds and lime highlights. It was redrawn with built-in `image_gen`, using a
locally installed vanilla Flyff sprite as a structural reference and the supplied
Universe screenshot as a color/surface reference. Full generation and refinement
prompts are in `vanilla-meadow.prompts.json`. The source DDS and screenshot are
not redistributed; no raw Universe texture was extracted.

The Simple runtime style now embeds a 1024 by 1024 DXT5 version of this sprite
with a complete mip chain. `tools/grass-artc.cpp` crops transparent margins,
resamples the source, removes low-alpha haze, pads edge RGB and preserves cutout
coverage in mipmaps. `tools/embed-grass.mjs` embeds its output in the DLL;
`build.ps1` and CMake regenerate it from the PNG. There is no runtime image
decoder, D3DX dependency or external grass texture to install. Detailed grass
continues to use its existing geometry.

The meadow review renderer samples its alpha silhouette on warped strips. It
also samples `snow-surface.png` for winter coating. It does not currently sample
`leaf-surface.png`; that material belongs to the rejected separate-leaf study.

These two earlier original surface textures are retained as study sources:

- `leaf-surface.png`: vertical pigment streaks, branching veins and folded green
  faces. The image contains surface detail; it does not contain a grass clump or
  an alpha silhouette. It is mapped onto individual modeled leaves.
- `snow-surface.png`: painted white powder and cool recessed shadows, mapped onto
  the upper part of winter foliage in the current review shader.

Both inputs are 1254 by 1254 opaque images generated with the built-in OpenAI
`image_gen` tool. Full prompts and reference roles are preserved in `prompts.json`.
The user-supplied Flyff Universe screenshot was a visual reference only and is
not redistributed. These are original study materials, not extracted Universe
textures. Rejected dense grass-card paintings are not used by this study.
