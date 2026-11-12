#pragma once

struct IDirect3DTexture9;
class Runtime;

// Caller-owned preview textures; the normal panel keeps its existing text header.
struct PanelBranding {
    enum class Layout { TextOnly, SymbolText, Horizontal, SymbolOnly };
    Layout layout = Layout::TextOnly;
    IDirect3DTexture9 *symbol = nullptr;
    IDirect3DTexture9 *horizontal = nullptr;
    float symbolAspect = 1;
    float horizontalAspect = 1;
};

void DrawPanel(Runtime &runtime, const PanelBranding &branding);
