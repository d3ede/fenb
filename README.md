<div align="center">

<picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/branding/approved/PNG/Dark/fenb-dark-horizontal-transparent.png">
  <source media="(prefers-color-scheme: light)" srcset="assets/branding/approved/PNG/Light/fenb-light-horizontal-transparent.png">
  <img src="assets/branding/approved/PNG/Light/fenb-light-horizontal-transparent.png" width="480" height="150" alt="FENB logo">
</picture>

# FENB

**A fresh look for the world of Flyff.**

A graphics mod inspired by ENBSeries, with adjustable effects and an in-game settings panel.

![Windows](https://img.shields.io/badge/Windows-x86%20client-9370DB)
![DirectX 9](https://img.shields.io/badge/DirectX-9-7864B3)
![Status](https://img.shields.io/badge/status-in%20development-A397BE)

**[Releases](https://github.com/d3ede/fenb/releases)** · [Installation](#installation) · [Report an issue](https://github.com/d3ede/fenb/issues)

</div>

---

## What FENB offers

| | Features |
| --- | --- |
| **Color & bloom** | Adjust exposure, contrast, saturation, temperature and soft light bloom. |
| **Light & atmosphere** | SSAO, sun rays, sky and cloud adjustments, distance and height fog, depth of field, brightness adaptation and local lighting. |
| **WaterFX** | Animated wave shading, refraction, reflections, depth-based absorption and optional caustics. |
| **Antialiasing** | FXAA, SMAA and hardware MSAA where supported. |
| **Profiles** | Tune effects live, save your own looks and switch between profiles. Settings save automatically. |
| **In-game controls** | English and Russian interface, individual effect switches and a master effects toggle. Processing preserves the game HUD. |

## Installation

**Requirements:** Windows and a compatible **32-bit DirectX 9 Flyff client** (`Neuz.exe`). No installer is needed.

1. Close the game. Use a packaged FENB build from [Releases](https://github.com/d3ede/fenb/releases), when available, or the `dist/` output of a local build.
2. Copy **`d3d9.dll`** and **`FENB.ini`** into the game folder, beside **`Neuz.exe`**. Copy **`FENB-presets/`** too if it is included with the build.
3. Launch the game normally and press **Shift + Enter** to open the panel.
4. Choose a profile or adjust the effects to your liking. Changes save automatically; **Ctrl + S** saves immediately.

Keep the third-party license notices included with the build. If the game folder already contains another `d3d9.dll`, back it up first; loading multiple graphics proxies together is not currently supported.

**Updating:** close the game before replacing the DLL. Keep your existing `FENB.ini` and `FENB-presets/` to retain your settings and profiles.

**Uninstalling:** close the game and remove FENB's `d3d9.dll`. Restore the original DLL if you backed one up. Your configuration and profiles can be kept for later.

## Compatibility

FENB is in active development and currently targets the **Homesickness Flyff client**. Other Flyff clients are not yet verified.

Depth-based effects currently need a supported **NVIDIA driver depth-copy path**. Unsupported effects fall back or remain inactive; reflections also depend on what is visible on screen. See the [rendering notes](docs/RENDERING.md) for effect details, limitations and local build instructions.

Found a problem? [Open an issue](https://github.com/d3ede/fenb/issues) with your FENB version, client, GPU and steps to reproduce it.
