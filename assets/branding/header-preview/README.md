# Header branding previews

The SVG masters are the approved purple bloom symbol and horizontal logo. Preview textures retain their alpha and are cropped around visible artwork with a small glow margin.

`symbol.bgra` and `horizontal.bgra` contain two little-endian 32-bit dimensions followed by tightly packed, straight-alpha BGRA8 pixels. The native UI preview fixture uploads these into caller-owned Direct3D 9 textures. `node tools/embed-logo.mjs` embeds the symbol into `src/generated/ui_logo.hpp`; the normal panel needs no external image files. `build.ps1` regenerates this header, and CMake builds use the tracked header, as with the UI font.

Regenerate with `node tools/prepare-header-branding.mjs` using the local `@resvg/resvg-js` installation under `artifacts/logo-renderer`. Native builds use the checked-in textures and do not require that renderer.

The three preview layouts are an icon with the existing UI wordmark, the outlined horizontal brand lockup, and the symbol alone. The approved normal panel uses the symbol alone, with the balanced spacing and violet active-tab highlight. Runtime owns its texture, releases it before device loss/reset, and recreates it lazily. On narrow panels the alternative proposals also fall back to the compact symbol so navigation remains available.
