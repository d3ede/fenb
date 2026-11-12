#include "runtime.hpp"
#include "backends/imgui_impl_dx9.h"
#include "backends/imgui_impl_win32.h"
#include "effects.hpp"
#include "generated/ui_font.hpp"
#include "imgui.h"
#include "state_guard.hpp"
#include "water.hpp"
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
      water_(std::make_unique<WaterFX>(device)) {
    configPath = ModuleDirectory() + L"FENB.ini";
    settings.load(configPath);
    fpsLimiter_.update(settings.unlockFps);
    visible = settings.openOnStart;
    configTime_ = FileTime(configPath);
#ifdef FENB_DIAGNOSTICS
    probe_ = std::make_unique<PipelineProbe>(device);
#endif
}
Runtime::~Runtime() {
    std::lock_guard lock(mutex_);
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
    effects_->invalidate();
    water_->invalidate();
    if (context_) {
        ContextScope scope(context_);
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }
}
void Runtime::beforeReset() {
    std::lock_guard lock(mutex_);
    resetFrame();
#ifdef FENB_DIAGNOSTICS
    probe_->abort();
#endif
    lost_ = true;
    inScene_ = false;
    invalidate();
    Log("Reset: released FENB default-pool resources.");
}
void Runtime::afterReset(HRESULT result) {
    std::lock_guard lock(mutex_);
    lost_ = FAILED(result);
    Log(SUCCEEDED(result) ? "Reset: successful; resources will be recreated lazily."
                          : "Reset: failed; renderer remains suspended.");
}
void Runtime::afterPresent(HRESULT result) {
    std::lock_guard lock(mutex_);
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
    lastWaterDraws = waterDraws;
    waterDraws = 0;
    water_->beginFrame(settings.enabled && settings.water && !compare && settings.waterRefraction > 0,
                       settings.enabled && settings.water && !compare && settings.waterSky > 0);
    Drop(sceneBackbuffer_);
    drawTarget_ = nullptr;
    sawWorld_ = hudStarted_ = false;
    invalidateDrawState();
}
void Runtime::beforeWaterDraw(UINT upStride) {
    if (lost_ || hudStarted_ || !settings.enabled || !settings.water || compare || vertexShaderActive_ ||
        !fvfKnown_ || drawFVF_ != 0x152 || !sceneBackbuffer_ || drawTarget_ != sceneBackbuffer_)
        return;
    if (water_->begin(settings, upStride))
        ++waterDraws;
}
void Runtime::environmentDraw(D3DPRIMITIVETYPE type, UINT start, UINT count) {
    if (lost_ || hudStarted_ || !settings.enabled || !settings.water || settings.waterSky <= 0 || compare ||
        vertexShaderActive_ || !fvfKnown_ || drawFVF_ != 0x142 || !sceneBackbuffer_ || drawTarget_ != sceneBackbuffer_)
        return;
    water_->captureEnvironment(type, start, count);
}
void Runtime::afterWaterDraw(bool up, bool indexed) {
    water_->end(up, indexed);
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
        DWORD position = drawFVF_ & D3DFVF_POSITION_MASK;
        if (vertexShaderActive_ || position == D3DFVF_XYZ ||
            (position >= D3DFVF_XYZB1 && position <= D3DFVF_XYZB5))
            sawWorld_ = true;
        return;
    }
    // Neuz switches from world geometry to RHW HUD/nameplates. Process once BEFORE
    // the first HUD draw. Any later 3D UI previews must remain unprocessed as well.
    hudStarted_ = true;
    if (!sawWorld_ || !settings.enabled || compare || (!settings.bloom && !settings.color))
        return;
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
    if (main)
        probe_->boundary();
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
            configTime_ = modified;
        }
    }
    fpsLimiter_.update(settings.unlockFps);
    bool panel = visible && main && initUI();
    // Final-frame filtering would recolor HUD. Only use this fallback if the frame
    // contains confirmed world geometry and no screen-space UI was drawn at all.
    bool process =
        sawWorld_ && !hudStarted_ && settings.enabled && !compare && (settings.bloom || settings.color);
    if (!panel && !process) {
        if (!settings.enabled || (!settings.bloom && !settings.color))
            effects_->invalidate();
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
        // Neuz owns a Win32 .cur cursor. Sharing it avoids a second software cursor
        // and competing SetCursor(nullptr)/SetCursor(gameCursor) calls.
        ImGui::GetIO().MouseDrawCursor = false;
        ImGui::NewFrame();
        DrawPanel(*this);
        ImGui::Render();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
    }
    device_->EndScene();
}
double Runtime::gpuBytes() const {
    return double(effects_->bytes() + water_->bytes());
}
#ifdef FENB_DIAGNOSTICS
void Runtime::requestCapture() {
    probe_->request();
    message = "Запрошен захват 3 кадров DX9";
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
    message = saved ? "Сохранено" : "Ошибка записи FENB.ini";
    configTime_ = FileTime(configPath);
}
void Runtime::reload() {
    bool loaded = settings.load(configPath);
    fpsLimiter_.update(settings.unlockFps);
    if (loaded)
        dirty_ = false;
    message = loaded ? "Настройки загружены" : "Файл настроек не найден";
    configTime_ = FileTime(configPath);
}
void Runtime::settingsChanged() {
    fpsLimiter_.update(settings.unlockFps);
    dirty_ = true;
    lastEdit_ = GetTickCount();
    message.clear();
}
LRESULT CALLBACK Runtime::WindowProc(HWND hwnd, UINT msg, WPARAM w, LPARAM l, UINT_PTR id, DWORD_PTR data) {
    auto *self = reinterpret_cast<Runtime *>(data);
    std::lock_guard lock(self->mutex_);
    if (msg == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, WindowProc, id);
        self->subclassed_ = false;
        self->window_ = nullptr;
        return DefSubclassProc(hwnd, msg, w, l);
    }
    const bool keyDown = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
    const bool keyUp = msg == WM_KEYUP || msg == WM_SYSKEYUP;
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
        if (self->visible)
            self->centerPanel = true;
        self->compare = false;
        if (self->visible && GetCapture() == hwnd)
            ReleaseCapture();
        if (!self->visible && self->dirty_)
            self->save();
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
        self->compare = false;
        self->hotkeyHeld_ = false;
        self->suppressEnterChar_ = false;
        memset(self->forwardedKeys_, 0, sizeof(self->forwardedKeys_));
    }
    if (self->visible) {
        if (msg == WM_KEYDOWN && w == 'S' && (GetKeyState(VK_CONTROL) & 0x8000) && !(l & (1LL << 30))) {
            self->save();
            return 0;
        }
        if (msg == WM_KEYDOWN && w == VK_ESCAPE && !editingText) {
            self->visible = false;
            self->compare = false;
            if (self->dirty_)
                self->save();
            return 0;
        }
        // Always let the game's WM_SETCURSOR handler run. NoMouseCursorChange
        // prevents the ImGui backend from replacing it even over UI controls.
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
