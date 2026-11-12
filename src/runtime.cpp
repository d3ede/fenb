#include "runtime.hpp"
#include "backends/imgui_impl_dx9.h"
#include "backends/imgui_impl_win32.h"
#include "effects.hpp"
#include "generated/ui_font.hpp"
#include "imgui.h"
#include "state_guard.hpp"
#include "water.hpp"
#include "sfx.hpp"
#include <commctrl.h>
#ifdef FENB_DIAGNOSTICS
#include "pipeline_probe.hpp"
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
namespace {
struct ContextScope {
    ImGuiContext *previous;
    explicit ContextScope(ImGuiContext *c) : previous(ImGui::GetCurrentContext()) {
        ImGui::SetCurrentContext(c);
    }
    ~ContextScope() {
        ImGui::SetCurrentContext(previous);
    }
};
FILETIME FileTime(const std::wstring &path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data);
    return data.ftLastWriteTime;
}
} // namespace
Runtime::Runtime(IDirect3DDevice9 *device)
    : device_(device), effects_(std::make_unique<Effects>(device)),
      water_(std::make_unique<WaterFX>(device)), sfx_(std::make_unique<SfxFX>(device)) {
    configPath = ModuleDirectory() + L"FENB.ini";
    settings.load(configPath);
    recognizeProfile();
    Com<IDirect3DSwapChain9> chain;
    D3DPRESENT_PARAMETERS presentation{};
    if (SUCCEEDED(device_->GetSwapChain(0, chain.out())) &&
        SUCCEEDED(chain->GetPresentParameters(&presentation)))
        presentation_.initialize(presentation, settings.msaaSamples);
    fpsLimiter_.update(settings.unlockFps);
    environment_.update(settings, compare);
    visible = settings.openOnStart;
    configTime_ = FileTime(configPath);
#ifdef FENB_DIAGNOSTICS
    probe_ = std::make_unique<PipelineProbe>(device);
#endif
}
Runtime::~Runtime() {
    std::lock_guard lock(mutex_);
    updateCursor(false);
    resetFrame();
#ifdef FENB_DIAGNOSTICS
    probe_.reset();
#endif
    if (dirty_)
        save();
    if (subclassed_ && IsWindow(window_))
        RemoveWindowSubclass(window_, WindowProc, reinterpret_cast<UINT_PTR>(this));
    if (context_) {
        auto *previous = ImGui::GetCurrentContext();
        ImGui::SetCurrentContext(context_);
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(context_);
        ImGui::SetCurrentContext(previous == context_ ? nullptr : previous);
    }
    effects_.reset();
}
Runtime::SceneStatus Runtime::sceneStatus() const {
    auto &fx = effects_->sceneFX();
    return {fx.depthReady(), fx.skyApplied(), fx.sunOnScreen(), fx.hasLightSource()};
}
void Runtime::recognizeProfile() {
    selectedProfile.clear();
    selectedBuiltIn = settings.matchingPreset();
    if (selectedBuiltIn >= 0)
        return;
    auto directory = configPath.substr(0, configPath.find_last_of(L"\\/") + 1) + L"FENB-presets\\";
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileW((directory + L"*.ini").c_str(), &data);
    if (find == INVALID_HANDLE_VALUE)
        return;
    unsigned count = 0;
    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        Settings candidate;
        if (!candidate.load(directory + data.cFileName))
            continue;
        candidate.preserveControls(settings);
        if (candidate == settings) {
            std::wstring name = data.cFileName;
            name.resize(name.size() - 4);
            int bytes =
                WideCharToMultiByte(CP_UTF8, 0, name.data(), int(name.size()), nullptr, 0, nullptr, nullptr);
            selectedProfile.resize(bytes);
            WideCharToMultiByte(CP_UTF8, 0, name.data(), int(name.size()), selectedProfile.data(), bytes,
                                nullptr, nullptr);
            selectedProfileSettings = settings;
            break;
        }
    } while (++count < 128 && FindNextFileW(find, &data));
    FindClose(find);
}
void Runtime::updateCursor(bool custom) {
    if (custom) {
        if (!cursorHidden_)
            nativeCursor_ = GetCursor();
        SetCursor(nullptr);
        cursorHidden_ = true;
    } else if (cursorHidden_) {
        SetCursor(nativeCursor_);
        nativeCursor_ = nullptr;
        cursorHidden_ = false;
    }
}
bool Runtime::attachWindow(HWND window) {
    if (!window || !IsWindow(window))
        return false;
    if (window_ == window && subclassed_)
        return true;
    // One panel belongs to the device's main presentation window; extra windows still get effects.
    if (window_ && window_ != window)
        return false;
    window_ = window;
    subclassed_ = SetWindowSubclass(window, WindowProc, reinterpret_cast<UINT_PTR>(this),
                                    reinterpret_cast<DWORD_PTR>(this)) != FALSE;
    if (subclassed_ && context_) {
        ContextScope scope(context_);
        ImGui_ImplWin32_Shutdown();
        ImGui_ImplWin32_Init(window);
    }
    return subclassed_;
}
bool Runtime::initUI() {
    if (context_)
        return true;
    auto *previous = ImGui::GetCurrentContext();
    IMGUI_CHECKVERSION();
    context_ = ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NoMouseCursorChange;
    // The subset font lives in mapped read-only DLL data, without a duplicate heap copy.
    ImFontConfig font{};
    font.FontDataOwnedByAtlas = false;
    font.OversampleH = 2;
    font.OversampleV = 1;
    if (!io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char *>(UI_FONT), sizeof(UI_FONT), 15, &font,
                                        io.Fonts->GetGlyphRangesCyrillic()))
        io.Fonts->AddFontDefault();
    bool ok = ImGui_ImplWin32_Init(window_);
    if (ok)
        ok = ImGui_ImplDX9_Init(device_);
    if (!ok) {
        if (io.BackendPlatformUserData)
            ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext(context_);
        context_ = nullptr;
    }
    ImGui::SetCurrentContext(previous);
    return ok;
}
void Runtime::invalidate() {
    Drop(panelLogo_.p);
    effects_->invalidate();
    water_->invalidate();
    sfx_->invalidate();
    if (context_) {
        ContextScope scope(context_);
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }
}
void Runtime::beforeReset() {
    std::lock_guard lock(mutex_);
    updateCursor(false);
    environment_.reset();
    resetFrame();
#ifdef FENB_DIAGNOSTICS
    probe_->abort();
    captureDeadline_ = 0;
#endif
    lost_ = true;
    inScene_ = false;
    invalidate();
    Log("Reset: released FENB default-pool resources.");
}
void Runtime::afterReset(HRESULT result) {
    std::lock_guard lock(mutex_);
    lost_ = FAILED(result);
    if (SUCCEEDED(result) && presentation_.pending() &&
        activeMSAA() != unsigned(presentation_.expected(settings.msaaSamples)))
        message = UiText::MSAAUnavailable;
    presentation_.complete(result);
    Log(SUCCEEDED(result) ? "Reset: successful; resources will be recreated lazily."
                          : "Reset: failed; renderer remains suspended.");
}
void Runtime::afterPresent(HRESULT result) {
    std::lock_guard lock(mutex_);
    environment_.update(settings, compare);
    resetFrame();
    if (result == D3DERR_DEVICELOST || result == D3DERR_DEVICEHUNG || result == D3DERR_DEVICEREMOVED) {
        if (!lost_) {
            lost_ = true;
            invalidate();
            Log("Present: device lost; rendering suspended.");
        }
    }
}
void Runtime::resetFrame() {
    sfx_->beginFrame(settings.enabled && (settings.sfx || (settings.water && settings.sfxWake > 0)) && !compare);
    effects_->sceneFX().beginFrame();
    lastWaterDraws = waterDraws;
    waterDraws = 0;
    bool waterEnabled = settings.enabled && settings.water && !compare;
    water_->beginFrame(waterEnabled && (settings.waterRefraction > 0 || settings.waterDepth > 0 ||
                                        settings.waterSSR > 0 || settings.waterCaustics > 0),
                       (waterEnabled && settings.waterSky > 0) ||
                           (settings.enabled && settings.fog && !compare),
                       waterEnabled &&
                           (settings.waterDepth > 0 || settings.waterSSR > 0 || settings.waterCaustics > 0),
                       waterEnabled && settings.waterCaustics > 0);
    Drop(sceneBackbuffer_);
    drawTarget_ = nullptr;
    sawWorld_ = hudStarted_ = sawScreenSpace_ = false;
    invalidateDrawState();
}
void Runtime::beforeWaterDraw(UINT upStride, DrawCall draw) {
    if (!lost_ && settings.enabled && (settings.sfx || (settings.water && settings.sfxWake > 0)) && !compare && !vertexShaderActive_ &&
        fvfKnown_ && (drawFVF_ == 0x102 || drawFVF_ == 0x142) &&
        sceneBackbuffer_ && drawTarget_ == sceneBackbuffer_)
        sfx_->begin(settings, waterDraws > 0, !hudStarted_);
    if (!lost_ && settings.enabled && !compare && !vertexShaderActive_ && fvfKnown_ &&
        drawFVF_ == 0x142 && sceneBackbuffer_ && drawTarget_ == sceneBackbuffer_)
        effects_->sceneFX().beginFlare(settings);
    if (!lost_ && !hudStarted_ && settings.enabled && !compare && !vertexShaderActive_ && fvfKnown_ &&
        drawFVF_ == 0x112 && (settings.nightFoliage > 0 || settings.lightingBalance) && sceneBackbuffer_ &&
        drawTarget_ == sceneBackbuffer_)
        effects_->sceneFX().beginFoliage(settings);
    if (lost_ || hudStarted_ || !settings.enabled || !settings.water || compare || vertexShaderActive_ ||
        !fvfKnown_ || drawFVF_ != 0x152 || !sceneBackbuffer_ || drawTarget_ != sceneBackbuffer_)
        return;
    if (water_->begin(settings, upStride)) {
        ++waterDraws;
        water_->reflect(draw);
    }
}
void Runtime::environmentDraw(D3DPRIMITIVETYPE type, UINT start, UINT count) {
    if (lost_ || hudStarted_ || !settings.enabled ||
        (!(settings.water && settings.waterSky > 0) && !settings.sky && !settings.fog) || compare || vertexShaderActive_ ||
        !fvfKnown_ || drawFVF_ != 0x142 || !sceneBackbuffer_ || drawTarget_ != sceneBackbuffer_)
        return;
    if ((settings.water && settings.waterSky > 0) || settings.fog) {
        water_->captureEnvironment(type, start, count, &effects_->sceneFX(), &settings);
        if (settings.fog)
            effects_->sceneFX().skyEnvironment(water_->skyEnvironment());
    }
    Com<IDirect3DBaseTexture9> texture;
    if (SUCCEEDED(device_->GetTexture(0, texture.out())) && texture.p &&
        texture->GetType() == D3DRTYPE_TEXTURE)
        effects_->sceneFX().beginSky(settings,
                                     water_->materialTag(static_cast<IDirect3DTexture9 *>(texture.p)));
}
void Runtime::afterWaterDraw(bool up, bool indexed) {
    sfx_->end(up,indexed);
    effects_->sceneFX().endSky();
    effects_->sceneFX().endFoliage();
    water_->end(up, indexed);
}
void Runtime::emitterDraw(DrawCall draw) {
    if (lost_ || !settings.enabled || compare || vertexShaderActive_ ||
        !settings.localLighting || !sceneBackbuffer_ || drawTarget_ != sceneBackbuffer_)
        return;
    effects_->sceneFX().captureEmitter(settings, draw);
}
void Runtime::beforeDraw() {
    // Cache setters: do not query every DX9 state on thousands of world draws.
    if (hudStarted_ || lost_)
        return;
    if (!sceneBackbuffer_ && FAILED(device_->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &sceneBackbuffer_)))
        return;
    if (!targetKnown_) {
        Com<IDirect3DSurface9> target;
        if (FAILED(device_->GetRenderTarget(0, target.out())))
            return;
        drawTarget_ = target.p;
        targetKnown_ = true;
    }
    if (drawTarget_ != sceneBackbuffer_)
        return; // shadow maps / off-screen previews are not the main scene
    if (!fvfKnown_) {
        if (FAILED(device_->GetFVF(&drawFVF_)))
            return;
        fvfKnown_ = true;
    }
    if (!shaderKnown_) {
        Com<IDirect3DVertexShader9> vs;
        if (FAILED(device_->GetVertexShader(vs.out())))
            return;
        vertexShaderActive_ = vs.p != nullptr;
        shaderKnown_ = true;
    }
    const bool screenSpace = !vertexShaderActive_ && (drawFVF_ & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW;
    if (!screenSpace) {
        if (settings.enabled && (settings.sfx || (settings.water && settings.sfxWake > 0)) && !compare && !vertexShaderActive_)
            sfx_->observe();
        if (settings.enabled && !compare && !vertexShaderActive_)
            effects_->sceneFX().observe(settings);
        DWORD position = drawFVF_ & D3DFVF_POSITION_MASK;
        if (!sawWorld_ && (vertexShaderActive_ || position == D3DFVF_XYZ ||
                          (position >= D3DFVF_XYZB1 && position <= D3DFVF_XYZB5))) {
            // Sky and translucent world sprites can precede RHW sun/occlusion quads.
            // Only an opaque perspective pass establishes the main world phase.
            DWORD write = 0, depth = 0;
            D3DMATRIX projection{};
            if (SUCCEEDED(device_->GetRenderState(D3DRS_ZWRITEENABLE, &write)) && write &&
                SUCCEEDED(device_->GetRenderState(D3DRS_ZENABLE, &depth)) && depth &&
                SUCCEEDED(device_->GetTransform(D3DTS_PROJECTION, &projection)) &&
                std::abs(projection._34 - 1) < .001f && std::abs(projection._44) < .001f)
                sawWorld_ = true;
        }
        return;
    }
    DWORD screenDepth = 0, screenWrites = 0;
    if (FAILED(device_->GetRenderState(D3DRS_ZENABLE, &screenDepth)) ||
        FAILED(device_->GetRenderState(D3DRS_COLORWRITEENABLE, &screenWrites)) ||
        screenDepth || !(screenWrites & 7))
        return; // Depth-tested/color-suppressed visibility probes are not the HUD.
    sawScreenSpace_ = true;
    if (!sawWorld_)
        return; // A pre-world screen-space draw must not latch the renderer off for this frame.
    // Neuz switches from world geometry to RHW HUD/nameplates. Process once BEFORE
    // the first HUD draw. Any later 3D UI previews must remain unprocessed as well.
    hudStarted_ = true;
    if (!sawWorld_ || !settings.enabled || compare)
        return;
    if (!settings.postEnabled()) {
        if(settings.water && settings.sfxWake>0 && waterDraws>0)sfx_->snapshotWorld();
        return;
    }
    StateGuard state(device_);
    if (!state.valid())
        return;
    // This is a COLOR resolve, permitted inside BeginScene. We never copy depth.
    if (!effects_->prepare(sceneBackbuffer_, settings))
        return;
    state.isolateTargets();
    device_->SetRenderTarget(0, sceneBackbuffer_);
    const bool ownScene = !inScene_;
    if (ownScene && FAILED(device_->BeginScene()))
        return;
    effects_->draw(sceneBackbuffer_, settings);
    if (ownScene)
        device_->EndScene();
    if(settings.water && settings.sfxWake>0 && waterDraws>0)sfx_->snapshotWorld(true);
}
void Runtime::targetCleared(DWORD flags, DWORD rectangles) {
    if (lost_ || rectangles || !(flags & D3DCLEAR_TARGET)) return;
    Com<IDirect3DSurface9> target, backbuffer;
    D3DSURFACE_DESC desc{}; D3DVIEWPORT9 viewport{};
    if (FAILED(device_->GetRenderTarget(0, target.out())) ||
        FAILED(device_->GetBackBuffer(0,0,D3DBACKBUFFER_TYPE_MONO,backbuffer.out())) ||
        target.p != backbuffer.p || FAILED(target->GetDesc(&desc)) ||
        FAILED(device_->GetViewport(&viewport)) || viewport.X || viewport.Y ||
        viewport.Width != desc.Width || viewport.Height != desc.Height) return;
    // A new full world render can start inside one Present interval. Previous
    // shadow/occlusion/UI epochs must not suppress this new scene's processing.
    resetFrame();
}
void Runtime::beforePresent(IDirect3DSwapChain9 *provided, DWORD flags, HWND windowOverride) {
    std::lock_guard lock(mutex_);
    // Do not add blocking GPU work to a nonblocking Present, or draw twice inside an open scene.
    if (flags & D3DPRESENT_DONOTWAIT || inScene_ || lost_)
        return;
    if (FAILED(device_->TestCooperativeLevel()))
        return;
    Com<IDirect3DSwapChain9> implicit;
    auto *chain = provided;
    if (!chain) {
        if (FAILED(device_->GetSwapChain(0, implicit.out())))
            return;
        chain = implicit;
    }
    D3DPRESENT_PARAMETERS pp{};
    if (FAILED(chain->GetPresentParameters(&pp)))
        return;
    HWND window = windowOverride ? windowOverride : pp.hDeviceWindow;
    if (!window) {
        D3DDEVICE_CREATION_PARAMETERS cp{};
        device_->GetCreationParameters(&cp);
        window = cp.hFocusWindow;
    }
    bool main = attachWindow(window);
#ifdef FENB_DIAGNOSTICS
    if (main) {
        if (captureDeadline_ && GetTickCount64() >= captureDeadline_) {
            captureDeadline_ = 0;
            probe_->request();
        }
        probe_->boundary();
    }
#endif
    DWORD now = GetTickCount();
    bool activeEditor = false;
    if (context_) {
        ContextScope scope(context_);
        activeEditor = ImGui::IsAnyItemActive();
    }
    if (dirty_ && !activeEditor && now - lastEdit_ > 450 && (!saveFailed_ || now - lastSaveAttempt_ > 2000))
        save();
    if (now - lastConfigCheck_ > 500 && !visible && !dirty_) {
        lastConfigCheck_ = now;
        auto modified = FileTime(configPath);
        if (CompareFileTime(&modified, &configTime_) != 0) {
            settings.load(configPath);
            recognizeProfile();
            configTime_ = modified;
        }
    }
    fpsLimiter_.update(settings.unlockFps);
    bool panel = visible && main && initUI();
    if (!visible || !settings.customCursor || (main && !panel))
        updateCursor(false);
    if (!settings.enabled || !settings.postEnabled())
        effects_->idle(settings);
    // Final-frame filtering would recolor HUD. Only use this fallback if the frame
    // contains confirmed world geometry and no screen-space UI was drawn at all.
    bool process = sawWorld_ && !sawScreenSpace_ && !hudStarted_ && settings.enabled && !compare && settings.postEnabled();
    if (!panel && !process) {
        return;
    }
    Com<IDirect3DSurface9> backbuffer;
    if (FAILED(chain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, backbuffer.out())))
        return;
    StateGuard state(device_);
    if (!state.valid())
        return;
    bool prepared = process && effects_->prepare(backbuffer, settings);
    if (!panel && !prepared)
        return;
    state.isolateTargets();
    device_->SetRenderTarget(0, backbuffer);
    if (FAILED(device_->BeginScene()))
        return;
    if (prepared)
        effects_->draw(backbuffer, settings);
    if (panel) {
        ContextScope scope(context_);
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        D3DSURFACE_DESC desc{};
        backbuffer->GetDesc(&desc);
        ImGui::GetIO().DisplaySize = ImVec2(float(desc.Width), float(desc.Height));
        // Cursor ownership is explicit: the backend must not fight the game.
        ImGui::GetIO().MouseDrawCursor = false;
        ImGui::NewFrame();
        DrawPanel(*this);
        const auto &io = ImGui::GetIO();
        if (visible && settings.customCursor && GetForegroundWindow() == window_ && ImGui::IsMousePosValid() &&
            io.MousePos.x >= 0 && io.MousePos.y >= 0 && io.MousePos.x < io.DisplaySize.x &&
            io.MousePos.y < io.DisplaySize.y)
            updateCursor(true);
        else if (!visible || !settings.customCursor)
            updateCursor(false);
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }
    device_->EndScene();
}
double Runtime::gpuBytes() const {
    return double(effects_->bytes() + water_->bytes() + sfx_->bytes());
}
unsigned Runtime::activeMSAA() const {
    Com<IDirect3DSwapChain9> chain;
    D3DPRESENT_PARAMETERS p{};
    if (FAILED(device_->GetSwapChain(0, chain.out())) || FAILED(chain->GetPresentParameters(&p)))
        return 0;
    return unsigned(p.MultiSampleType);
}
bool Runtime::msaaAvailable(int samples) const {
    if (!SupportedPresentationHost())
        return false;
    if (samples <= 0)
        return true;
    Com<IDirect3DSwapChain9> chain;
    Com<IDirect3D9> root;
    D3DPRESENT_PARAMETERS p{};
    D3DDEVICE_CREATION_PARAMETERS creation{};
    if (FAILED(device_->GetSwapChain(0, chain.out())) || FAILED(chain->GetPresentParameters(&p)) ||
        p.SwapEffect != D3DSWAPEFFECT_DISCARD || FAILED(device_->GetDirect3D(root.out())) ||
        FAILED(device_->GetCreationParameters(&creation)))
        return false;
    for (auto format :
         {p.BackBufferFormat, p.EnableAutoDepthStencil ? p.AutoDepthStencilFormat : p.BackBufferFormat}) {
        DWORD quality = 0;
        if (FAILED(root->CheckDeviceMultiSampleType(creation.AdapterOrdinal, creation.DeviceType, format,
                                                    p.Windowed, D3DMULTISAMPLE_TYPE(samples), &quality)) ||
            !quality)
            return false;
    }
    return true;
}
HRESULT Runtime::presentationResult(HRESULT result, IDirect3DSwapChain9 *chain, DWORD flags) {
    std::lock_guard lock(mutex_);
    if (FAILED(result) || (flags & D3DPRESENT_DONOTWAIT) || inScene_ || !SupportedPresentationHost())
        return result;
    if (chain) {
        Com<IDirect3DSwapChain9> main;
        if (FAILED(device_->GetSwapChain(0, main.out())) || main.p != chain)
            return result;
    }
    if (presentation_.pending())
        return D3DERR_DEVICELOST;
    if (!presentation_.changed(settings.msaaSamples))
        return result;
    presentation_.observe(settings.msaaSamples);
    if (!msaaAvailable(settings.msaaSamples)) {
        message = UiText::MSAAUnavailable;
        return result;
    }
    presentation_.request(settings.msaaSamples);
    beforeReset();
    Log("Presentation: requesting host Lost Device recovery for MSAA change.");
    return D3DERR_DEVICELOST;
}
#ifdef FENB_DIAGNOSTICS
void Runtime::requestCapture() {
    captureDeadline_ = GetTickCount64() + 3000;
    message = UiText::CapturePending;
}
void Runtime::captureDraw(const char *api, UINT type, UINT primitives, UINT vertices, UINT stride,
                          const void *up) {
    probe_->draw(api, type, primitives, vertices, stride, up);
}
#endif
void Runtime::save() {
    settings.sanitize();
    lastSaveAttempt_ = GetTickCount();
    bool saved = settings.save(configPath);
    saveFailed_ = !saved;
    if (saved)
        dirty_ = false;
    message = saved ? UiText::Saved : UiText::ConfigWriteFailed;
    configTime_ = FileTime(configPath);
}
void Runtime::reload() {
    bool loaded = settings.load(configPath);
    if (loaded)
        recognizeProfile();
    fpsLimiter_.update(settings.unlockFps);
    environment_.update(settings, compare);
    if (loaded)
        dirty_ = false;
    message = loaded ? UiText::SettingsLoaded : UiText::ConfigNotFound;
    configTime_ = FileTime(configPath);
}
void Runtime::settingsChanged() {
    fpsLimiter_.update(settings.unlockFps);
    environment_.update(settings, compare);
    dirty_ = true;
    lastEdit_ = GetTickCount();
    message = UiText::None;
}
LRESULT CALLBACK Runtime::WindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR data) {
    auto *self = reinterpret_cast<Runtime *>(data);
    std::lock_guard lock(self->mutex_);
    if (msg == WM_NCDESTROY) {
        self->updateCursor(false);
        RemoveWindowSubclass(hwnd, WindowProc, id);
        self->subclassed_ = false;
        self->window_ = nullptr;
        return DefSubclassProc(hwnd, msg, w, l);
    }
    const bool keyDown = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
    const bool keyUp = msg == WM_KEYUP || msg == WM_SYSKEYUP;
    if (w == VK_F12 && self->effectsHotkeyHeld_ && (keyDown || keyUp)) {
        if (keyUp)
            self->effectsHotkeyHeld_ = false;
        return 0;
    }
    if (msg == WM_KEYDOWN && w == VK_F12 && (GetKeyState(VK_RSHIFT) & 0x8000) &&
        !(GetKeyState(VK_CONTROL) & 0x8000) && !(GetKeyState(VK_MENU) & 0x8000) &&
        !(l & (1LL << 30))) {
        self->effectsHotkeyHeld_ = true;
        self->settings.enabled = !self->settings.enabled;
        self->compare = false;
        self->settingsChanged();
        return 0;
    }
    if ((msg == WM_CHAR || msg == WM_SYSCHAR || msg == WM_UNICHAR) && (w == '\r' || w == '\n') &&
        self->suppressEnterChar_)
        return 0;
    if (keyUp && w == VK_RETURN && self->hotkeyHeld_) {
        self->hotkeyHeld_ = false;
        return 0;
    }
    if (keyDown && w == VK_RETURN && self->hotkeyHeld_)
        return 0;
    if (keyDown && w == VK_RETURN && !(l & (1LL << 30)))
        self->suppressEnterChar_ = false;
    const bool shift = (GetKeyState(VK_SHIFT) | GetKeyState(VK_LSHIFT) | GetKeyState(VK_RSHIFT)) & 0x8000;
    if (msg == WM_KEYDOWN && w == VK_RETURN && shift && !(GetKeyState(VK_CONTROL) & 0x8000) &&
        !(GetKeyState(VK_MENU) & 0x8000) && !(l & (1LL << 30))) {
        self->hotkeyHeld_ = true;
        // TranslateMessage may queue CR after KEYUP. Keep it suppressed until
        // the next ordinary Enter keydown, including when this closes the panel.
        self->suppressEnterChar_ = true;
        self->visible = !self->visible;
        if (self->visible) {
            self->centerPanel = true;
            self->editorPage = 0;
        }
        self->compare = false;
        if (self->visible && GetCapture() == hwnd)
            ReleaseCapture();
        if (!self->visible && self->dirty_)
            self->save();
        if (!self->visible)
            self->updateCursor(false);
        return 0;
    }
    bool editingText = false;
    if (self->context_) {
        ContextScope scope(self->context_);
        editingText = ImGui::GetIO().WantTextInput;
        if (self->visible)
            ImGui_ImplWin32_WndProcHandler(hwnd, msg, w, l);
    }
    if (msg == WM_KILLFOCUS) {
        self->updateCursor(false);
        self->compare = false;
        self->hotkeyHeld_ = false;
        self->suppressEnterChar_ = false;
        self->effectsHotkeyHeld_ = false;
        memset(self->forwardedKeys_, 0, sizeof(self->forwardedKeys_));
    }
    if (self->visible) {
        if (msg == WM_KEYDOWN && w == 'S' && (GetKeyState(VK_CONTROL) & 0x8000) && !(l & (1LL << 30))) {
            self->save();
            return 0;
        }
        if (msg == WM_KEYDOWN && w == VK_ESCAPE && !editingText) {
            self->visible = false;
            self->updateCursor(false);
            self->compare = false;
            if (self->dirty_)
                self->save();
            return 0;
        }
        if (msg == WM_SETCURSOR) {
            if (LOWORD(l) == HTCLIENT && self->settings.customCursor && self->context_) {
                self->updateCursor(true);
                return TRUE;
            }
            self->updateCursor(false);
        }
        bool mouse = (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST) || msg == WM_INPUT;
        bool keyboard = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN || msg == WM_CHAR || msg == WM_SYSCHAR);
        if ((msg == WM_KEYUP || msg == WM_SYSKEYUP) && w < 256) {
            bool forwarded = self->forwardedKeys_[w];
            self->forwardedKeys_[w] = false;
            if (!forwarded)
                return 0;
        }
        if (mouse || keyboard) {
            if (msg == WM_INPUT)
                return DefWindowProcW(hwnd, msg, w, l); // raw input cleanup is required
            return 0;
        }
    } else if (w < 256) {
        if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
            self->forwardedKeys_[w] = true;
        if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
            self->forwardedKeys_[w] = false;
    }
    return DefSubclassProc(hwnd, msg, w, l);
}
