#include "runtime.hpp"
#include "ui_branding.hpp"
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
bool HeaderTexture(IDirect3DDevice9 *device, const wchar_t *path, Com<IDirect3DTexture9> &texture,
                   float &aspect) {
    FILE *file = _wfopen(path, L"rb");
    if (!file)
        return false;
    UINT dimensions[2]{};
    bool valid = fread(dimensions, sizeof(UINT), 2, file) == 2 && dimensions[0] && dimensions[1] &&
                 dimensions[0] <= 512 && dimensions[1] <= 256;
    std::vector<unsigned char> pixels(valid ? size_t(dimensions[0]) * dimensions[1] * 4 : 0);
    valid = valid && fread(pixels.data(), 1, pixels.size(), file) == pixels.size() && fgetc(file) == EOF;
    fclose(file);
    if (!valid || FAILED(device->CreateTexture(dimensions[0], dimensions[1], 1, 0, D3DFMT_A8R8G8B8,
                                               D3DPOOL_MANAGED, texture.out(), nullptr)))
        return false;
    D3DLOCKED_RECT lock{};
    if (FAILED(texture->LockRect(0, &lock, nullptr, 0)))
        return false;
    for (UINT y = 0; y < dimensions[1]; ++y)
        memcpy(static_cast<unsigned char *>(lock.pBits) + size_t(y) * lock.Pitch,
               pixels.data() + size_t(y) * dimensions[0] * 4, size_t(dimensions[0]) * 4);
    texture->UnlockRect(0);
    aspect = float(dimensions[0]) / dimensions[1];
    return true;
}
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
    const PanelBranding *activeBranding = nullptr;
    auto frame = [&]() {
        ImGui_ImplDX9_NewFrame();
        ImGui::NewFrame();
        if (activeBranding)
            DrawPanel(r, *activeBranding);
        else
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
    frame(); frame();
    auto *panel=ImGui::FindWindowByName("FENB");
    CHECK(panel && panel->Pos.x==120 && panel->Pos.y==30);
    auto child = [&]() -> ImGuiWindow * {
        for(auto *w:ImGui::GetCurrentContext()->Windows)
            if(w->ParentWindow==panel && strstr(w->Name,"adjustments")) return w;
        return nullptr;
    };
    auto activate = [&](ImGuiWindow *w,const char *id) {
        ImGui::ActivateItemByID(w->GetID(id)); frame(); frame();
    };
    auto input = [&](const char *text) {
        io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame();
        io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);
        io.AddInputCharactersUTF8(text);frame();
        io.AddKeyEvent(ImGuiKey_Enter,true);frame();io.AddKeyEvent(ImGuiKey_Enter,false);frame();
    };
    // Header controls retain stable IDs independent of language and subpage titles.
    activate(panel,"world-tab");CHECK(r.editorPage==4);
    activate(panel,"image-tab");CHECK(r.editorPage==0);
    activate(panel,"language-ru");CHECK(r.settings.language==Language::Russian);
    activate(panel,"language-en");CHECK(r.settings.language==Language::English);
    CHECK(child());
    // Actual track gestures, held double-click reset, steps and typed values.
    const float rowY=261;
    r.settings.exposure=.8f;frame();
    click(440,rowY);mouse(440,rowY,true);
    CHECK(r.settings.exposure==0);frame();CHECK(r.settings.exposure==0);
    mouse(440,rowY,false);
    click(620,rowY);CHECK(std::abs(r.settings.exposure-.01f)<.00001f);
    click(566,rowY);CHECK(r.settings.exposure==0);
    click(590,rowY);input("0.75");CHECK(std::abs(r.settings.exposure-.75f)<.0001f);
    click(654,rowY);CHECK(r.settings.exposure==0);
    r.settings.exposure=.8f;r.settings.waterOpacity=.7f;frame();
    click(610,228);CHECK(r.settings.exposure==0 && r.settings.waterOpacity==.7f);
    // Navigation preserves all WaterFX, atmosphere and AA controls.
    activate(child(),"water-settings");CHECK(r.editorPage==1);
    activate(child(),"back");CHECK(r.editorPage==0);
    activate(child(),"scene-settings");CHECK(r.editorPage==2);
    r.editorPage=0;frame();
    activate(child(),"aa-settings");CHECK(r.editorPage==3);
    // Post-AA combo uses an animated chevron and leaves hardware MSAA independent.
    click(400,199);frame();
    CHECK(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
    auto *popup=ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
    // Select by the visible third row; the popup uses the shared font spacing.
    click(popup->Pos.x+40,popup->Pos.y+8+2*ImGui::GetTextLineHeightWithSpacing());frame();
    CHECK(r.settings.smaa && !r.settings.fxaa && r.settings.msaaSamples==0);
    // FPS is always visible, but unsupported clients cannot enable its hook.
    CHECK(!r.fpsUnlockSupported());click(638,556);CHECK(!r.settings.unlockFps);
    r.editorPage=0;frame();
    activate(panel,"menu");CHECK(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
    popup=ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
    auto appearance="###text-"+std::to_string(static_cast<int>(UiText::V2Appearance));
    activate(popup,appearance.c_str());CHECK(r.editorPage==5);
    r.settings.customCursor=true;r.settings.interfaceMotion=true;frame();
    activate(child(),"custom-cursor");CHECK(!r.settings.customCursor);
    activate(child(),"interface-motion");CHECK(!r.settings.interfaceMotion);
    r.save();Settings saved;CHECK(saved.load(r.configPath));
    CHECK(!saved.customCursor && !saved.interfaceMotion);
    // Reset all and preset loads preserve appearance, language and client controls.
    r.settings.language=Language::Russian;r.settings.exposure=.4f;frame();
    activate(panel,"reset-all");
    CHECK(r.settings.language==Language::Russian && !r.settings.customCursor && !r.settings.interfaceMotion);
    CHECK(r.settings.exposure==0);
    r.editorPage=0;frame();
    activate(panel,"+###create-profile");
    CHECK(ImGui::IsPopupOpen("saveProfile",ImGuiPopupFlags_AnyPopupId));
    popup=ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
    click(popup->Pos.x+40,popup->Pos.y+49);io.AddInputCharactersUTF8("Тест воды");frame();
    click(popup->Pos.x+60,popup->Pos.y+82);frame();
    CHECK(saved.load(ModuleDirectory()+L"FENB-presets\\Тест воды.ini"));
    CHECK(saved.language==Language::Russian);
    r.settings.exposure=.63f;frame();
    click(360,96);frame();CHECK(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
    popup=ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
    activate(popup,"Тест воды");
    CHECK(r.settings.exposure==saved.exposure && r.settings.language==Language::Russian);
    for(int i=0;i<BuiltInPresetCount;++i) {
        click(360,96);frame();CHECK(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
        popup=ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        auto id="###text-"+std::to_string(static_cast<int>(BuiltInProfileNames[i]));
        activate(popup,id.c_str());CHECK(r.settings.matchingPreset()==i);
        CHECK(r.settings.language==Language::Russian && !r.settings.customCursor && !r.settings.interfaceMotion);
    }
    // Render every production page and each expanded atmosphere section in both languages.
    CreateDirectoryW(L"build/ui-integrated",nullptr);
    io.AddMousePosEvent(-FLT_MAX,-FLT_MAX);frame();
    for(int language=0;language<2;++language) {
        r.settings.language=language?Language::Russian:Language::English;
        for(int page=0;page<6;++page) {
            r.editorPage=page;ImGui::SetScrollY(child(),0);frame();frame();
            auto path=std::wstring(L"build/ui-integrated/")+(language?L"ru-":L"en-")+std::to_wstring(page)+L".png";
            CHECK(Screenshot(d,path.c_str()));
        }
        r.editorPage=2;frame();
        for(const char *section:{"ssao","rays","sky","fog","dof","adaptation","lighting-balance","foliage","lighting","sfx"}) {
            auto *w=child();
            for(const char *id:{"ssao","rays","sky","fog","dof","adaptation","lighting-balance","foliage","lighting","sfx"})
                w->StateStorage.SetBool(ImHashStr("expanded",0,w->GetID(id)),strcmp(id,section)==0);
            ImGui::SetScrollY(w,0);frame();frame();
            auto path=std::wstring(L"build/ui-integrated/")+(language?L"ru-":L"en-")+std::wstring(section,section+strlen(section))+L".png";
            CHECK(Screenshot(d,path.c_str()));
        }
        io.DisplaySize={334,600};r.centerPanel=true;r.editorPage=0;frame();frame();
        CHECK(Screenshot(d,language?L"build/ui-integrated/ru-narrow.png":L"build/ui-integrated/en-narrow.png"));
        io.DisplaySize={800,600};r.centerPanel=true;frame();frame();
    }
    r.editorPage=4;r.settings.timeOverride=true;r.settings.timeMinutes=1110;frame();
    mouse(400,168,true);auto firstTime=r.settings.timeMinutes;
    mouse(480,168,true);CHECK(r.settings.timeMinutes!=firstTime);mouse(480,168,false);
    click(590,168);input("07:45");CHECK(r.settings.timeMinutes==465);
    DeleteFileW((ModuleDirectory()+L"FENB-presets\\Тест воды.ini").c_str());
    ImGui_ImplDX9_Shutdown();ImGui::DestroyContext();DestroyWindow(window);DeleteFileW(r.configPath.c_str());
    puts("PASS: integrated UI gestures, numeric input, resets, profiles, language, appearance persistence, FPS guard and page captures.");
}
