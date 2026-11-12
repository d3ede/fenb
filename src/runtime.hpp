#pragma once
#include "settings.hpp"
#include "fps_limiter.hpp"
#include "environment.hpp"
#include "draw_call.hpp"
#include "depth_policy.hpp"
#include "presentation_reload.hpp"
struct ImGuiContext;
struct PanelBranding;
class Effects;
class WaterFX;
class SfxFX;
#ifdef FENB_DIAGNOSTICS
class PipelineProbe;
#endif

class Runtime {
  public:
    struct SceneStatus {
        bool depth, sky, sun, light;
    };
    SceneStatus sceneStatus() const;
    explicit Runtime(IDirect3DDevice9 *device);
    ~Runtime();
    void beforePresent(IDirect3DSwapChain9 *chain, DWORD flags = 0, HWND windowOverride = nullptr);
    void afterPresent(HRESULT result);
    void beforeReset();
    void afterReset(HRESULT result);
    void scene(bool begin) {
        inScene_ = begin;
    }
    bool inScene() const {
        return inScene_;
    }
    Settings settings;
    bool visible = false;
    bool compare = false;
    bool centerPanel = true;
    unsigned waterDraws = 0, lastWaterDraws = 0;
    int editorPage = 0;
    char profileName[128]{};
    UiText profileError = UiText::None;
    std::string selectedProfile;
    int selectedBuiltIn = -1;
    Settings selectedProfileSettings;
    UiText message = UiText::None;
    std::wstring configPath;
    double gpuBytes() const;
    PanelBranding panelBranding();
    unsigned activeMSAA() const;
    bool msaaAvailable(int samples) const;
    void nativePresentation(const D3DPRESENT_PARAMETERS &p) {
        presentation_.initialize(p, settings.msaaSamples);
    }
    HRESULT presentationResult(HRESULT result, IDirect3DSwapChain9 *chain = nullptr, DWORD flags = 0);
    HRESULT cooperativeResult(HRESULT result) const {
        return presentation_.cooperative(result);
    }
    template <class Call> HRESULT resetPresentation(D3DPRESENT_PARAMETERS *p, Call &&call) {
        if (!p || !SupportedPresentationHost())
            return call(p);
        auto adjusted = *p;
        presentation_.parameters(adjusted, settings.msaaSamples);
        auto result = WithPresentationSettings(device_, &adjusted, settings, std::forward<Call>(call));
        if (SUCCEEDED(result))
            *p = adjusted;
        return result;
    }
    void save();
    void reload();
    void settingsChanged();
    bool fpsUnlockSupported() {
        return fpsLimiter_.supported();
    }
    bool environmentSupported() { return environment_.supported(); }
    float environmentMinutes() const { return environment_.minutes(settings); }
    void beforeDraw();
    void targetCleared(DWORD flags, DWORD rectangles);
    void environmentDraw(D3DPRIMITIVETYPE type, UINT start, UINT count);
    void beforeWaterDraw(UINT upStride, DrawCall);
    void afterWaterDraw(bool up = false, bool indexed = false);
    void emitterDraw(DrawCall);
    void drawFVF(DWORD value) {
        drawFVF_ = value;
        fvfKnown_ = true;
    }
    void drawShader(bool active) {
        vertexShaderActive_ = active;
        shaderKnown_ = true;
    }
    void drawTarget(DWORD index, IDirect3DSurface9 *target) {
        if (index == 0) {
            drawTarget_ = target;
            targetKnown_ = true;
        }
    }
    void invalidateDrawState() {
        fvfKnown_ = shaderKnown_ = targetKnown_ = false;
    }
#ifdef FENB_DIAGNOSTICS
    void requestCapture();
    void captureDraw(const char *api, UINT type, UINT primitives, UINT vertices, UINT stride = 0,
                     const void *up = nullptr);
#endif
    bool pendingSave() const {
        return dirty_;
    }

  private:
    static LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    bool attachWindow(HWND window);
    bool initUI();
    void updateCursor(bool custom);
    void recognizeProfile();
    void invalidate();
    void resetFrame();
    IDirect3DDevice9 *device_; // borrowed: owner destroys Runtime before releasing the real device
    std::unique_ptr<Effects> effects_;
    std::unique_ptr<WaterFX> water_;
    std::unique_ptr<SfxFX> sfx_;
    FpsLimiter fpsLimiter_;
    Environment environment_;
    PresentationReload presentation_;
#ifdef FENB_DIAGNOSTICS
    std::unique_ptr<PipelineProbe> probe_;
    ULONGLONG captureDeadline_ = 0;
#endif
    ImGuiContext *context_ = nullptr;
    Com<IDirect3DTexture9> panelLogo_;
    HWND window_ = nullptr;
    HCURSOR nativeCursor_ = nullptr; // borrowed; the client owns its cursor resource
    bool cursorHidden_ = false;
    unsigned cursorHideCalls_ = 0;
    bool lost_ = false, inScene_ = false, subclassed_ = false;
    std::recursive_mutex mutex_;
    bool forwardedKeys_[256]{};
    bool hotkeyHeld_ = false;
    bool effectsHotkeyHeld_ = false;
    bool suppressEnterChar_ = false;
    DWORD lastConfigCheck_ = 0;
    FILETIME configTime_{};
    bool dirty_ = false, saveFailed_ = false;
    DWORD lastEdit_ = 0, lastSaveAttempt_ = 0;
    IDirect3DSurface9 *sceneBackbuffer_ = nullptr;
    IDirect3DSurface9 *drawTarget_ = nullptr; // borrowed from current native binding
    DWORD drawFVF_ = 0;
    bool fvfKnown_ = false, shaderKnown_ = false, targetKnown_ = false, vertexShaderActive_ = false;
    bool sawWorld_ = false, hudStarted_ = false, sawScreenSpace_ = false;
};
void DrawPanel(Runtime &runtime);
