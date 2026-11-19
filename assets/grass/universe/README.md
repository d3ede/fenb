# Supplied Flyff Universe grass

These three assets were supplied by the project owner in
`flyff-grass-desktop.zip`. They are third-party game artwork, not the earlier
FENB-generated grass study. Their inclusion does not assign a new license to
the source artwork. Original licensing is unchanged.

- `env_grass.obj`: original Y-up mesh, 720 vertices, 720 triangles, 180 quads.
  Both triangle windings are present and retained. The renderer uses normal
  back-face culling to avoid drawing each side twice.
- `env_grass.png`: normal material, 256 by 128 RGBA.
- `env_grass-et01.png`: winter material, 256 by 128 RGBA.

## Import and rendering

`tools/import-grass-model.mjs` preserves every supplied position, normal, UV and
triangle index. It does not recenter the model, move its pivot or lift its
negative-Y bases. It adds a shared terrain-sampling anchor to each original
quad, derived from its two lowest vertices. The shader applies the existing
width/height controls around the original local origin and rotates instances
around that origin. Terrain heights are sampled at the quad anchors so a
four-metre patch can conform to a slope without independently stretching each
vertex to a different ground level. OBJ V is converted to the Direct3D texture
coordinate convention in the shader; atlas regions remain unchanged.

`tools/grass-artc.cpp` preserves the full 256 by 128 base RGBA image exactly.
There is no crop, UV repacking, rescaling, recoloring or lossy base compression.
It writes nine ARGB mip levels, with alpha-weighted color filtering and alpha
coverage preservation within the U=0..0.5 and U=0.5..0.75 atlas regions until
those islands become sub-texel. Both textures are embedded by
`tools/embed-grass.mjs`, uploaded lazily and recreated on device reset. The
runtime has no PNG-decoder dependency. Alpha cutout uses a 0.5 threshold and
retains depth writes; texture filtering and state are restored after the pass.

The existing Grass switch, Simple style selector, Snowy grass switch and shape,
density, distance, variation, wind and color controls remain connected. Winter
terrain selects the supplied winter atlas directly, without the old procedural
snow recoloring. Detailed grass uses its original mesh and shaders.

## Placement

The mesh represents a complete patch, not one blade. Its instance spacing is
3.5 world units at density 1 and width 0.03, adjusted by the existing density
and width controls and a bounded quality budget. All distance bands retain the
complete source topology and share the same world grid. Roots do not reseed
when the camera rotates. Terrain coverage, slope, roads, props, water and
winter recognition use the existing desktop vegetation/height replay.

The archive's three grassMap/heightMap samples remain under ignored import
artifacts. The source notes explicitly leave grid orientation, byte meanings,
height scale and spacing unverified. They are reference data, not a verified
placement map for the desktop world; no interpretation is silently assumed.
