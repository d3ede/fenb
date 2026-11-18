# Experimental tree appearance

Reviewed replacement bark for the Flaris common-tree and Azria tree families,
plus repainted leaves and hanging vines in Flaris. These assets retain the
game's existing geometry and UV layout.
The artwork uses each original atlas's dimensions and retains source opacity
and snow coverage where applicable. The rounded, autumn and tall Flaris
tree families use the more defined painted bark. Other ordinary Flaris
trees use painted plates blended with the grain of their stock textures.

The 33 DDS inputs contain complete mip chains. Opaque bark uses DXT1 and the
cutout branch, leaf and vine atlases use DXT5. They total 2,233,168 bytes
including DDS headers. The thirteen ordinary bark atlases retain distinct
patterns and color grades. The new leaf artwork keeps the tree families'
green, olive, bronze and copper palettes. Leaf and vine masks have openings
between sprays and transparent margins where opaque artwork reached exposed
mesh edges. The mip chain preserves cutout coverage through minification.
The catalog lists a second pattern
for three shared bark sources and one shared leaf source. The object's world
position selects the pattern in those cases.
`tools/trees.json` maps exact original compressed and ARGB8 upload identities to
these assets. `node tools/embed-trees.mjs` regenerates the tracked embedded
data and identity catalog. Production has no D3DX, image decoder or external
texture-file dependency.

For authoring, compile `tools/tree-texturec.cpp` with the existing x86 toolchain
and `-municode -Isrc -ld3d9`, then use `tools/prepare-tree-assets.mjs` with the
reviewed artwork folder and an explicitly selected read-only client directory
when preparing an initial catalog. For an existing catalog, keep its source
identities and update its DDS paths and optional `alternateAsset` entries.
Set `kind` to `foliage` for a replacement governed by the Foliage switch.
The offline tools use system D3DX; it is not distributed with FENB.

Source tree art remains attributable to its original owners. These replacement
assets do not add a new license grant for underlying game artwork.
