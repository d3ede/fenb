#include "runtime.hpp"
#include "ui_branding.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "backends/imgui_impl_dx9.h"
#include "ui_fonts.hpp"
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
struct UpdateTestAccess {
    static void set(Updater &updater, UpdateState state, const char *version = "", bool rollback = false) {
        updater.status_ = {state, version, rollback};
    }
};
int main() {
    CreateDirectoryW(ModuleDirectory().c_str(), nullptr);
    // The embedded fallback must remain usable without a Windows font file.
    ImGui::CreateContext();
    auto fallback = LoadPanelFonts(L"build/runtime-test-data/missing-ui-font.ttf");
    CHECK(fallback.regular && fallback.emphasis && fallback.regular != fallback.emphasis);
    for (auto *font : {fallback.regular, fallback.emphasis})
        for (ImWchar glyph : {ImWchar('A'), ImWchar(0x416), ImWchar(0xf1), ImWchar(0xe7), ImWchar(0xe3)})
            CHECK(font->GetFontBaked(13)->FindGlyphNoFallback(glyph));
    ImGui::DestroyContext();
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
    const auto fonts = LoadPanelFonts();
    CHECK(fonts.regular && fonts.emphasis && fonts.regular != fonts.emphasis);
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
    // The custom pointer also covers client space outside the panel and restores the native fallback.
    io.AddMousePosEvent(30,30);frame();
    CHECK(ImGui::GetMouseCursor()==ImGuiMouseCursor_None);
    CHECK(ImGui::GetForegroundDrawList()->VtxBuffer.Size>0);
    CHECK(Screenshot(d,L"build/FENB-custom-pointer.png"));
    r.settings.customCursor=false;frame();
    CHECK(ImGui::GetMouseCursor()!=ImGuiMouseCursor_None);
    r.settings.customCursor=true;io.AddMousePosEvent(-FLT_MAX,-FLT_MAX);frame();
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
    activate(panel,"materials-tab");CHECK(r.editorPage==8);
    activate(panel,"world-tab");CHECK(r.editorPage==4);
    activate(panel,"image-tab");CHECK(r.editorPage==0);
    for (std::size_t i : {1u, 2u, 3u, 0u}) {
        activate(panel,"##language");
        CHECK(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
        auto *languagePopup=ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
        auto option=ImHashStr("###option",0,ImHashStr(LanguageCodes[i],0,languagePopup->IDStack.back()));
        ImGui::ActivateItemByID(option);frame();frame();
        CHECK(r.settings.language==static_cast<Language>(i));
        CHECK(ImGui::GetCurrentContext()->OpenPopupStack.empty());
    }
    CHECK(child());
    ImGuiWindow *popup=nullptr;
    auto activeWindow=[&](const char *fragment)->ImGuiWindow*{
        for(auto *w:ImGui::GetCurrentContext()->Windows)if(w->Active&&strstr(w->Name,fragment))return w;
        return nullptr;
    };
    auto navigation=[&](){return activeWindow("/items_");};
    auto inspector=[&](){return activeWindow("/panel-inspector-");};
    auto scoped=[&](ImGuiWindow *w,const char *scope,const char *id){return ImHashStr(id,0,w->GetID(scope));};
    auto selectItem=[&](const char *id){ImGui::ActivateItemByID(scoped(navigation(),id,"select"));frame();frame();};
    auto control=[&](const char *scope,const char *id){ImGui::ActivateItemByID(scoped(inspector(),scope,id));frame();frame();};
    auto numeric=[&](const char *scope,float &value){const void *pointer=&value;auto row=ImHashData(&pointer,sizeof(pointer),inspector()->GetID(scope));return ImHashStr("##value",0,row);};
    CHECK(r.editorSelection[0]==4 && inspector());
    const auto body=inspector()->Pos;
    const float width=inspector()->Size.x, rowY=body.y+34;
    r.settings.exposure=.8f;frame();
    click(body.x+115,rowY+31);mouse(body.x+115,rowY+31,true);
    CHECK(r.settings.exposure==0);frame();CHECK(r.settings.exposure==0);mouse(body.x+115,rowY+31,false);
    click(body.x+width-30,rowY+11);CHECK(std::abs(r.settings.exposure-.01f)<.00001f);
    click(body.x+width-107,rowY+11);CHECK(r.settings.exposure==0);
    ImGui::ActivateItemByID(numeric("tone",r.settings.exposure));frame();input("0.75");CHECK(std::abs(r.settings.exposure-.75f)<.0001f);
    click(body.x+width-11,rowY+11);CHECK(r.settings.exposure==0);
    r.settings.exposure=.8f;r.settings.waterOpacity=.7f;r.settings.language=Language::Russian;frame();
    control("tone","reset-section");CHECK(r.settings.exposure==0&&r.settings.waterOpacity==.7f&&r.settings.language==Language::Russian);
    r.settings.language=Language::English;frame();
    const auto beforeNavigation=r.settings;
    selectItem("water");CHECK(r.editorPage==1);selectItem("method");CHECK(r.editorSelection[1]==6);
    CHECK(r.settings==beforeNavigation);
    control("method","##reflection-method");CHECK(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
    auto *methodPopup=ImGui::GetCurrentContext()->OpenPopupStack.back().Window;int methodChoice=2;
    ImGui::ActivateItemByID(ImHashStr("###option",0,ImHashData(&methodChoice,sizeof(methodChoice),methodPopup->IDStack.back())));frame();frame();
    CHECK(r.settings.waterReflectionMethod==2&&r.settings.waterSSRQuality==1);
    activate(child(),"back");CHECK(r.editorPage==0);
    selectItem("scene");CHECK(r.editorPage==2);
    r.settings.dof=false;frame();selectItem("dof");CHECK(r.sceneSelection==7&&!r.settings.dof);
    control("dof","enabled");CHECK(r.settings.dof);control("dof","enabled");CHECK(!r.settings.dof);
    r.settings.language=Language::Spanish;r.settings.aoStrength=1.2f;selectItem("ssao");control("ssao","reset-section");
    CHECK(r.settings.aoStrength==Settings{}.aoStrength&&r.settings.language==Language::Spanish);
    activate(child(),"back");selectItem("optimization");CHECK(r.editorPage==7);
    CHECK(r.editorSelection[7]==0);
    for(const char *duplicate:{"background","fps","effects"}){selectItem(duplicate);CHECK(r.editorSelection[7]==0);}
    CHECK(!r.nativeEffectOptimizationSupported());
    selectItem("decorative");CHECK(!r.settings.hideDecorativeEffects);control("decorative","enabled");CHECK(!r.settings.hideDecorativeEffects);
    selectItem("skills");CHECK(!r.settings.hideSkillEffects);control("skills","enabled");CHECK(!r.settings.hideSkillEffects);
    r.settings.hideSkillEffects=true;frame();control("skills","enabled");CHECK(!r.settings.hideSkillEffects&&!r.settings.hideDecorativeEffects);
    selectItem("shops");CHECK(!r.settings.limitShopSigns);CHECK(!r.shopOptimizationSupported());control("shops","enabled");CHECK(!r.settings.limitShopSigns);
    r.settings.language=Language::English;activate(child(),"back");selectItem("aa");CHECK(r.editorPage==3);
    r.settings.fxaa=true;r.settings.smaa=false;r.settings.msaaSamples=4;frame();
    selectItem("smaa");control("smaa","enabled");CHECK(r.settings.smaa&&!r.settings.fxaa&&r.settings.msaaSamples==4);
    selectItem("fxaa");control("fxaa","enabled");CHECK(r.settings.fxaa&&!r.settings.smaa&&r.settings.msaaSamples==4);
    r.settings.msaaSamples=0;

    // Both client controls stay visible, with stable IDs outside the actions menu.
    CHECK(!r.fpsUnlockSupported());click(638,556);CHECK(!r.settings.unlockFps);
    CHECK(!r.backgroundActivitySupported());
    activate(panel,"background-activity");CHECK(!r.settings.backgroundActivity);
    r.settings.backgroundActivity=true;frame();
    activate(panel,"background-activity");CHECK(!r.settings.backgroundActivity && !r.settings.unlockFps);
    r.settings.backgroundActivity=true;frame();
    r.editorPage=0;frame();
    activate(panel,"menu");CHECK(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
    popup=ImGui::GetCurrentContext()->OpenPopupStack.back().Window;
    auto appearance="###text-"+std::to_string(static_cast<int>(UiText::V2Appearance));
    activate(popup,appearance.c_str());CHECK(r.editorPage==5);
    r.settings.customCursor=true;r.settings.interfaceMotion=true;frame();
    control("cursor","enabled");CHECK(!r.settings.customCursor);
    selectItem("motion");control("motion","enabled");CHECK(!r.settings.interfaceMotion);
    r.save();Settings saved;CHECK(saved.load(r.configPath));
    CHECK(!saved.customCursor && !saved.interfaceMotion && saved.backgroundActivity && !saved.unlockFps);
    // Reset all and preset loads preserve appearance, language and client controls.
    for (std::size_t i=0;i<LanguageCount;++i) {
        r.settings.language=static_cast<Language>(i);r.settings.exposure=.4f;frame();
        activate(panel,"reset-all");
        CHECK(r.settings.language==static_cast<Language>(i) && !r.settings.customCursor && !r.settings.interfaceMotion);
        CHECK(r.settings.exposure==0);
        CHECK(r.settings.backgroundActivity && !r.settings.unlockFps);
    }
    r.settings.language=Language::Russian;
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
    r.editorPage=0;r.resetEditorSelection=true;frame();frame();CHECK(r.editorSelection[0]==4&&r.sceneSelection==0);
    CreateDirectoryW(L"build/ui-integrated",nullptr);
    r.settings.water=true;io.AddMousePosEvent(-FLT_MAX,-FLT_MAX);frame();
    for(int language=0;language<static_cast<int>(LanguageCount);++language){
        r.settings.language=static_cast<Language>(language);
        const std::wstring locale(LanguageCodes[language],LanguageCodes[language]+2);
        activate(panel,"##language");frame();CHECK(Screenshot(d,(L"build/ui-integrated/"+locale+L"-language.png").c_str()));ImGui::ClosePopupToLevel(0,true);frame();
        activate(panel,"menu");CHECK(Screenshot(d,(L"build/ui-integrated/"+locale+L"-actions.png").c_str()));ImGui::ClosePopupToLevel(0,true);frame();
        activate(panel,"materials-tab");r.editorSelection[8]=0;frame();frame();
        r.settings.materials=false;control("appearance","enabled");CHECK(r.settings.materials);
        ImGui::ActivateItemByID(numeric("appearance",r.settings.materialStrength));frame();input("0.9");CHECK(r.settings.materialStrength==.9f);
        control("appearance","reset-section");CHECK(r.settings.materials&&r.settings.materialStrength==Settings{}.materialStrength);
        CHECK(Screenshot(d,(L"build/ui-integrated/"+locale+L"-materials.png").c_str()));
        r.editorSelection[8]=1;frame();frame();
        ImGui::ActivateItemByID(numeric("relief",r.settings.materialRelief));frame();input("0.9");CHECK(r.settings.materialRelief==.9f);
        control("relief","reset-section");CHECK(r.settings.materialRelief==Settings{}.materialRelief);
        CHECK(Screenshot(d,(L"build/ui-integrated/"+locale+L"-material-relief.png").c_str()));
        r.editorSelection[8]=2;frame();frame();
        CHECK(Screenshot(d,(L"build/ui-integrated/"+locale+L"-material-snow.png").c_str()));
        r.editorSelection[8]=3;frame();frame();
        r.settings.sharpening=false;control("microcontrast","enabled");CHECK(r.settings.sharpening);
        ImGui::ActivateItemByID(numeric("microcontrast",r.settings.sharpeningStrength));frame();input("0.8");CHECK(r.settings.sharpeningStrength==.8f);
        ImGui::ActivateItemByID(numeric("microcontrast",r.settings.textureContrast));frame();input("0.9");CHECK(r.settings.textureContrast==.9f);
        control("microcontrast","reset-section");CHECK(r.settings.sharpening&&r.settings.sharpeningStrength==Settings{}.sharpeningStrength&&r.settings.textureContrast==Settings{}.textureContrast&&r.settings.language==static_cast<Language>(language));
        CHECK(Screenshot(d,(L"build/ui-integrated/"+locale+L"-microcontrast.png").c_str()));control("microcontrast","enabled");CHECK(!r.settings.sharpening);
        struct View{int page,item;const wchar_t *name;};
        const View views[]={{0,4,L"tone"},{0,7,L"bloom"},{1,0,L"water"},{1,6,L"reflections"},{1,7,L"trace"},{2,0,L"scene"},{3,0,L"antialiasing"},{4,0,L"time"},{4,1,L"weather"},{5,2,L"interface"},{7,0,L"optimization-shops"},{7,1,L"optimization-decorative"},{7,2,L"optimization-skills"}};
        for(const auto &view:views){r.editorPage=view.page;r.editorSelection[view.page]=view.item;r.sceneSelection=0;frame();frame();CHECK(inspector());CHECK(Screenshot(d,(L"build/ui-integrated/"+locale+L"-"+view.name+L".png").c_str()));}
        r.editorPage=2;
        for(int effect=0;effect<11;++effect){r.sceneSelection=effect;frame();frame();CHECK(inspector());}
        io.DisplaySize={334,400};r.centerPanel=true;
        for(int page:{0,1,2,3,4,5,7,8}){r.editorPage=page;frame();frame();CHECK(inspector());CHECK(inspector()->Size.x>240);CHECK(inspector()->Pos.x>=0&&inspector()->Pos.x+inspector()->Size.x<=334);}
        r.editorPage=7;
        for(int item=0;item<3;++item){r.editorSelection[7]=item;frame();frame();CHECK(Screenshot(d,(L"build/ui-integrated/"+locale+L"-optimization-"+std::to_wstring(item)+L"-narrow.png").c_str()));}
        r.editorPage=8;r.editorSelection[8]=0;frame();frame();CHECK(Screenshot(d,(L"build/ui-integrated/"+locale+L"-materials-narrow.png").c_str()));
        CHECK(Screenshot(d,(L"build/ui-integrated/"+locale+L"-narrow.png").c_str()));
        r.editorPage=0;r.editorSelection[0]=4;frame();frame();activate(child(),"##panel-navigation");CHECK(!ImGui::GetCurrentContext()->OpenPopupStack.empty());
        auto *navPopup=ImGui::GetCurrentContext()->OpenPopupStack.back().Window;CHECK(navPopup->Pos.x>=0&&navPopup->Pos.x+navPopup->Size.x<=334);
        CHECK(Screenshot(d,(L"build/ui-integrated/"+locale+L"-navigation-narrow.png").c_str()));
        ImGui::ActivateItemByID(ImHashStr("###select",0,navPopup->GetID("water")));frame();frame();CHECK(r.editorPage==1);
        io.DisplaySize={800,600};r.centerPanel=true;frame();frame();
    }
    r.editorPage=4;r.editorSelection[4]=0;r.settings.enabled=false;r.settings.timeOverride=true;r.settings.timeMinutes=1110;frame();frame();
    const auto clockID=ImHashStr("##clock",0,ImHashStr("time-of-day",0,inspector()->GetID("time")));
    ImGui::ActivateItemByID(clockID);frame();input("07:45");CHECK(r.settings.timeMinutes==465);
    control("time","reset-section");CHECK(r.settings.timeOverride&&r.settings.timeMinutes==720);
    for(int language=0;language<static_cast<int>(LanguageCount);++language){r.settings.language=static_cast<Language>(language);frame();frame();const std::wstring locale(LanguageCodes[language],LanguageCodes[language]+2);CHECK(Screenshot(d,(L"build/ui-integrated/"+locale+L"-time-without-effects.png").c_str()));}

    DeleteFileW((ModuleDirectory()+L"FENB-presets\\Тест воды.ini").c_str());
    // Production modal: stable translated IDs, one notification per version,
    // download-in-background -> restart prompt, and narrow-screen wrapping.
    r.editorPage=0;
    r.settings.enabled=true;
    for(int language=0;language<static_cast<int>(LanguageCount);++language) {
        r.settings.language=static_cast<Language>(language);
        const std::wstring locale(LanguageCodes[language],LanguageCodes[language]+2);
        r.updateNotified.clear();
        UpdateTestAccess::set(r.updater,UpdateState::Available,"0.4c");
        frame();frame();
        auto *modal=ImGui::FindWindowByName("###fenb-update");
        CHECK(modal && modal->WasActive);
        for(int settle=0;settle<30;++settle) frame(); // Inspect the fully faded modal backdrop.
        CHECK(std::abs(modal->Pos.x+modal->Size.x*.5f-(panel->Pos.x+panel->Size.x*.5f))<2);
        auto path=std::wstring(L"build/ui-integrated/")+(locale+L"-update.png");
        CHECK(Screenshot(d,path.c_str()));
        auto later="###text-"+std::to_string(static_cast<int>(UiText::UpdateLater));
        activate(modal,later.c_str());frame();CHECK(!modal->WasActive);
        frame();CHECK(!modal->WasActive);
        UpdateTestAccess::set(r.updater,UpdateState::Ready,"0.4c");frame();frame();
        CHECK(modal->WasActive);
        path=std::wstring(L"build/ui-integrated/")+(locale+L"-update-restart.png");
        CHECK(Screenshot(d,path.c_str()));
        io.DisplaySize={360,400};frame();frame();
        CHECK(modal->Pos.x>=0 && modal->Pos.x+modal->Size.x<=360);
        CHECK(modal->Pos.y>=0 && modal->Pos.y+modal->Size.y<=400);
        path=std::wstring(L"build/ui-integrated/")+(locale+L"-update-narrow.png");
        CHECK(Screenshot(d,path.c_str()));
        activate(modal,later.c_str());
        io.DisplaySize={800,600};frame();
        UpdateTestAccess::set(r.updater,UpdateState::Installed,"0.4b",true);
        r.updateDialog=true;frame();frame();
        CHECK(modal->WasActive);
        path=std::wstring(L"build/ui-integrated/")+(locale+L"-update-rollback.png");
        CHECK(Screenshot(d,path.c_str()));
        activate(modal,later.c_str());
        UpdateTestAccess::set(r.updater,UpdateState::Idle);
    }
    ImGui_ImplDX9_Shutdown();ImGui::DestroyContext();DestroyWindow(window);DeleteFileW(r.configPath.c_str());
    puts("PASS: integrated UI gestures, language, profiles, update/restart/rollback dialogs and page captures.");
}
