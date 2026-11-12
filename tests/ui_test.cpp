#include "runtime.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_dx9.h"
#include "generated/ui_font.hpp"
#include <gdiplus.h>
#include <cstdio>
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                           \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
std::wstring ModuleDirectory() {
    wchar_t p[32768]{};
    GetCurrentDirectoryW(32768, p);
    return std::wstring(p) + L"\\build\\runtime-test-data\\";
}
void Log(const char *) {}
bool Screenshot(IDirect3DDevice9 *d, const wchar_t *path) {
    Com<IDirect3DSurface9> bb, read, resolved;
    d->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, bb.out());
    D3DSURFACE_DESC desc{};
    if (FAILED(bb->GetDesc(&desc)))
        return false;
    IDirect3DSurface9 *source = bb;
    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE) {
        if (FAILED(d->CreateRenderTarget(desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE,
                                         0, FALSE, resolved.out(), nullptr)) ||
            FAILED(d->StretchRect(bb, nullptr, resolved, nullptr, D3DTEXF_NONE)))
            return false;
        source = resolved;
    }
    if (FAILED(d->CreateOffscreenPlainSurface(800, 600, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, read.out(),
                                              nullptr)) ||
        FAILED(d->GetRenderTargetData(source, read)))
        return false;
    D3DLOCKED_RECT l{};
    if (FAILED(read->LockRect(&l, nullptr, D3DLOCK_READONLY)))
        return false;
    Gdiplus::GdiplusStartupInput startup;
    ULONG_PTR token = 0;
    Gdiplus::GdiplusStartup(&token, &startup, nullptr);
    UINT n = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&n, &size);
    std::vector<BYTE> codecs(size);
    auto *c = (Gdiplus::ImageCodecInfo *)codecs.data();
    Gdiplus::GetImageEncoders(n, size, c);
    CLSID png{};
    for (UINT i = 0; i < n; ++i)
        if (!wcscmp(c[i].MimeType, L"image/png"))
            png = c[i].Clsid;
    bool ok = false;
    {
        Gdiplus::Bitmap image(800, 600, l.Pitch, PixelFormat32bppRGB, (BYTE *)l.pBits);
        ok = image.Save(path, &png, nullptr) == Gdiplus::Ok;
    }
    Gdiplus::GdiplusShutdown(token);
    read->UnlockRect();
    return ok;
}
int main() {
    CreateDirectoryW(ModuleDirectory().c_str(), nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"FENBUITest";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"Native UI tests", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600,
                                nullptr, nullptr, wc.hInstance, nullptr);
    Com<IDirect3D9> root;
    root.p = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(root.p);
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.BackBufferWidth = 800;
    pp.BackBufferHeight = 600;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.hDeviceWindow = window;
    Com<IDirect3DDevice9> d;
    CHECK(SUCCEEDED(
        root->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, d.out())));
    Runtime r(d);
    r.configPath = ModuleDirectory() + L"ui-test.ini";
    r.settings = Settings{};
    r.visible = true;
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {800, 600};
    io.DeltaTime = .06f;
    io.ConfigInputTrickleEventQueue = false;
    ImFontConfig font{};
    font.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF((void *)UI_FONT, sizeof(UI_FONT), 15, &font,
                                   io.Fonts->GetGlyphRangesCyrillic());
    CHECK(ImGui_ImplDX9_Init(d));
    auto frame = [&]() {
        ImGui_ImplDX9_NewFrame();
        ImGui::NewFrame();
        DrawPanel(r);
        ImGui::Render();
        d->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff272c36, 1, 0);
        d->BeginScene();
        ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
        d->EndScene();
    };
    auto mouse = [&](float x, float y, bool down) {
        io.AddMousePosEvent(x, y);
        io.AddMouseButtonEvent(0, down);
        frame();
    };
    auto click = [&](float x, float y) {
        mouse(x, y, true);
        mouse(x, y, false);
    };
    frame();
    frame();
    auto *panel = ImGui::FindWindowByName("FENB");
    CHECK(panel && panel->Pos.x == 130 && panel->Pos.y == 40);
    CHECK(r.settings.language == Language::English);
    CHECK(Screenshot(d, L"build/FENB-main-english.png"));
    click(290, 60); // Language tab immediately to the right of Image.
    frame();
    CHECK(ImGui::IsPopupOpen("languages", ImGuiPopupFlags_AnyPopupId));
    CHECK(Screenshot(d, L"build/FENB-language-menu.png"));
    auto languagePopup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window->Pos;
    click(languagePopup.x + 50, languagePopup.y + 49); // English first, Russian second.
    frame();
    CHECK(r.settings.language == Language::Russian && r.pendingSave());
    CHECK(Screenshot(d, L"build/FENB-main-russian.png"));
    r.save();
    Settings languageSaved;
    CHECK(languageSaved.load(r.configPath) && languageSaved.language == Language::Russian);
    // Inspect the new action before opening profile popups.
    mouse(625, 60, false);
    click(625, 60);
    frame();
    CHECK(Screenshot(d, L"build/FENB-fps-menu.png"));
    CHECK(ImGui::IsPopupOpen("actions", ImGuiPopupFlags_AnyPopupId));
    CHECK(!r.fpsUnlockSupported() && !r.settings.unlockFps);
    auto fpsPopup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window->Pos;
    click(fpsPopup.x + 70, fpsPopup.y + 17);
    CHECK(!r.settings.unlockFps); // Unknown executables cannot install the simulation hook.
    click(150, 60);               // Close the action menu before the existing interaction checks.
    frame();
    // Main: navigation precedes color controls. First slider starts at y=240.
    r.settings.exposure = .8f;
    frame();
    click(440, 253); // ordinary drag changes exposure
    mouse(440, 253, true);
    CHECK(r.settings.exposure == 0); // second click resets
    frame();
    CHECK(r.settings.exposure == 0);
    mouse(440, 253, false); // held second click must not drag away from default
    click(610, 253);
    CHECK(std::abs(r.settings.exposure - .01f) < .00001f); // explicit right step
    click(556, 253);
    CHECK(r.settings.exposure == 0); // matching left step
    click(590, 253);
    io.AddKeyEvent(ImGuiMod_Ctrl, true);
    io.AddKeyEvent(ImGuiKey_A, true);
    frame();
    io.AddKeyEvent(ImGuiKey_A, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    io.AddInputCharactersUTF8("0.75");
    frame();
    io.AddKeyEvent(ImGuiKey_Enter, true);
    frame();
    io.AddKeyEvent(ImGuiKey_Enter, false);
    frame();
    CHECK(std::abs(r.settings.exposure - .75f) < .0001f);
    CHECK(panel->Pos.x == 130);
    click(640, 253);
    CHECK(r.settings.exposure == 0); // full reset hit box
    r.settings.exposure = .8f;
    r.settings.saturation = .3f;
    r.settings.waterOpacity = .7f;
    click(610, 226);
    CHECK(r.settings.exposure == 0 && r.settings.saturation == 1 && r.settings.waterOpacity == .7f);
    click(185, 226); // Collapse color: Bloom follows immediately.
    click(185, 258); // Collapse Bloom before checking the navigation.
    click(195, 512);
    CHECK(r.editorPage == 0); // The old pinned position is no longer navigation.
    click(195, 195);
    CHECK(r.editorPage == 1);
    frame();
    click(610, 168);
    CHECK(r.settings.waterOpacity == Settings{}.waterOpacity);
    CHECK(Screenshot(d, L"build/FENB-water-panel.png"));
    click(185, 168); // Expose depth/caustic controls without scrolling the full surface section.
    frame();
    CHECK(Screenshot(d, L"build/FENB-water-optics.png"));
    click(185, 132);
    CHECK(r.editorPage == 0);
    frame();
    click(195, 135);
    CHECK(r.editorPage == 2);
    frame();
    CHECK(Screenshot(d, L"build/FENB-atmosphere-panel.png"));
    click(185, 164);
    frame();
    CHECK(Screenshot(d, L"build/FENB-ssao-panel.png"));
    click(170, 131);
    CHECK(r.editorPage == 0);
    frame();
    click(195, 165);
    CHECK(r.editorPage == 3);
    frame();
    CHECK(Screenshot(d, L"build/FENB-antialias-panel.png"));
    // The spatial methods are ordered Off, FXAA, SMAA; selection never touches MSAA or FXAA tuning.
    float oldStrength = r.settings.fxaaStrength;
    r.settings.language = Language::English;
    frame();
    CHECK(Screenshot(d, L"build/FENB-antialias-english.png"));
    click(400, 196);
    frame();
    CHECK(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
    auto aaPopup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window->Pos;
    click(aaPopup.x + 40, aaPopup.y + 8 + 2 * ImGui::GetTextLineHeightWithSpacing());
    frame();
    CHECK(r.settings.smaa && !r.settings.fxaa && r.settings.msaaSamples == 0);
    CHECK(r.settings.fxaaStrength == oldStrength);
    CHECK(Screenshot(d, L"build/FENB-antialias-smaa-english.png"));
    r.settings.language = Language::Russian;
    frame();
    CHECK(Screenshot(d, L"build/FENB-antialias-smaa-russian.png"));
    click(170, 131);
    CHECK(r.editorPage == 0);
    frame();
    ImGui::SetWindowPos(panel, {20, 20});
    r.centerPanel = true;
    frame();
    CHECK(panel->Pos.x == 130 && panel->Pos.y == 40);
    CHECK(Screenshot(d, L"build/FENB-main-panel.png"));
    // A real UTF-8 named profile is written and reloaded through the actual popup widgets.
    click(603, 102);
    frame();
    CHECK(ImGui::IsPopupOpen("saveProfile", ImGuiPopupFlags_AnyPopupId));
    auto popup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window->Pos;
    click(popup.x + 40, popup.y + 49);
    io.AddInputCharactersUTF8("Тест воды");
    frame();
    click(popup.x + 60, popup.y + 82);
    frame();
    Settings profile;
    CHECK(profile.load(ModuleDirectory() + L"FENB-presets\\Тест воды.ini"));
    CHECK(profile.waterWaves == r.settings.waterWaves && profile.exposure == r.settings.exposure);
    CHECK(profile.language == Language::Russian);
    click(360, 102);
    frame();
    CHECK(Screenshot(d, L"build/FENB-profiles.png"));
    // Loading a named profile must retain the chosen UI language, even if its file says otherwise.
    profile.language = Language::English;
    CHECK(profile.save(ModuleDirectory() + L"FENB-presets\\Тест воды.ini"));
    r.settings.exposure = .63f;
    CHECK(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
    auto *profileWindow = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
    ImGui::ActivateItemByID(profileWindow->GetID("Тест воды"));
    frame();
    CHECK(r.settings.exposure == profile.exposure && r.settings.language == Language::Russian);
    CHECK(r.selectedProfile == "Тест воды");
    r.settings.preset(0);
    frame();
    CHECK(r.settings.language == Language::Russian);
    click(625, 60);
    frame();
    CHECK(ImGui::IsPopupOpen("actions", ImGuiPopupFlags_AnyPopupId));
    auto *actionsWindow = ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
    auto resetID = "###text-" + std::to_string(static_cast<int>(UiText::ResetAll));
    ImGui::ActivateItemByID(actionsWindow->GetID(resetID.c_str()));
    frame();
    CHECK(r.settings.language == Language::Russian && r.settings.exposure == 0);
    click(345, 60);
    frame();
    CHECK(ImGui::IsPopupOpen("languages", ImGuiPopupFlags_AnyPopupId));
    languagePopup = ImGui::GetCurrentContext()->OpenPopupStack.back().Window->Pos;
    click(languagePopup.x + 50, languagePopup.y + 20);
    frame();
    CHECK(r.settings.language == Language::English);
    r.save();
    CHECK(languageSaved.load(r.configPath) && languageSaved.language == Language::English);
    io.DisplaySize = {334, 600};
    r.centerPanel = true;
    frame();
    frame();
    CHECK(Screenshot(d, L"build/FENB-narrow-english.png"));
    r.settings.language = Language::Russian;
    frame();
    CHECK(Screenshot(d, L"build/FENB-narrow-russian.png"));
    // A legacy preserve-game setting must display the real native samples, even if an override differs.
    DWORD msaaQuality = 0;
    if (SUCCEEDED(root->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, pp.BackBufferFormat, TRUE,
                                                   D3DMULTISAMPLE_8_SAMPLES, &msaaQuality)) && msaaQuality) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
        r.beforeReset();
        auto multisampled = pp;
        multisampled.SwapEffect = D3DSWAPEFFECT_DISCARD;
        multisampled.MultiSampleType = D3DMULTISAMPLE_8_SAMPLES;
        auto result = d->Reset(&multisampled);
        CHECK(SUCCEEDED(result));
        r.afterReset(result);
        CHECK(r.activeMSAA() == 8);
        io.DisplaySize = {800, 600};
        r.centerPanel = true;
        r.editorPage = 3;
        r.settings.msaaSamples = 0;
        r.settings.language = Language::English;
        frame();
        frame();
        CHECK(r.settings.msaaSamples == 0);
        CHECK(Screenshot(d, L"build/FENB-antialias-native8-english.png"));
        click(400, 245);
        frame();
        CHECK(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
        CHECK(Screenshot(d, L"build/FENB-antialias-native8-menu.png"));
        io.AddKeyEvent(ImGuiKey_Escape, true);
        frame();
        io.AddKeyEvent(ImGuiKey_Escape, false);
        frame();
        r.settings.msaaSamples = 4; // A pending or failed request must not disguise the active 8x mode.
        r.settings.language = Language::Russian;
        frame();
        CHECK(Screenshot(d, L"build/FENB-antialias-native8-russian.png"));
    }
    DeleteFileW((ModuleDirectory() + L"FENB-presets\\Тест воды.ini").c_str());
    ImGui_ImplDX9_Shutdown();
    ImGui::DestroyContext();
    DestroyWindow(window);
    DeleteFileW(r.configPath.c_str());
    puts("PASS: UI double-click reset remains stable while held, per-setting steps, fixed numeric input, "
         "reset button, WaterFX/back, recenter, native screenshots.");
}
