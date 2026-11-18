# Experimental tree appearance

Reviewed replacement bark artwork for the Flaris common-tree and Azria tree
families. These assets retain the game's existing geometry and UV layout.
The modified artwork was prepared with ImageGen and restored source alpha/snow
coverage, then reduced to each original atlas's dimensions.

The 21 DDS inputs contain complete mip chains: DXT1 for opaque bark and DXT5 for
the cutout branch atlas. They total 1,226,416 bytes including DDS headers.
Thirteen of the replacements are for ordinary trees, with distinct patterns and
color grades. For three shared ordinary source textures, the catalog lists a
second pattern selected by the object's world position.
`tools/trees.json` maps exact original compressed and ARGB8 upload identities to
these assets. `node tools/embed-trees.mjs` regenerates the tracked embedded
data and identity catalog. Production has no D3DX, image decoder or external
texture-file dependency.

For authoring, compile `tools/tree-texturec.cpp` with the existing x86 toolchain
and `-municode -Isrc -ld3d9`, then use `tools/prepare-tree-assets.mjs` with the
reviewed artwork folder and an explicitly selected read-only client directory
when preparing an initial catalog. For an existing catalog, keep its source
identities and update its DDS paths and optional `alternateAsset` entries.
The offline tools use system D3DX; it is not distributed with FENB.

Source tree art remains attributable to its original owners. These replacement
assets do not add a new license grant for underlying game artwork.
