# Header branding previews

The SVG masters are the approved purple bloom symbol and horizontal logo. Preview textures retain their alpha and are cropped around visible artwork with a small glow margin.

`symbol.bgra` and `horizontal.bgra` contain two little-endian 32-bit dimensions followed by tightly packed, straight-alpha BGRA8 pixels. The native UI preview fixture uploads these into caller-owned Direct3D 9 textures. They are preview resources, not dependencies loaded by the normal game panel.

Regenerate with `node tools/prepare-header-branding.mjs` using the local `@resvg/resvg-js` installation under `artifacts/logo-renderer`. Native builds use the checked-in textures and do not require that renderer.

The three layouts are an icon with the existing UI wordmark, the outlined horizontal brand lockup, and the symbol alone. The normal panel remains unchanged until a layout is selected. On narrow panels the proposals fall back to the compact symbol so navigation remains available.
