#include "runtime.hpp"
#include "native_skate.hpp"
#include "native_skate_particles.hpp"
#include <bit>
#include <cstdio>
std::wstring ModuleDirectory() {
    wchar_t cwd[32768]{};
    GetCurrentDirectoryW(32768, cwd);
    return std::wstring(cwd) + L"\\build\\runtime-test-data\\";
}
void Log(const char *) {}
namespace {
int cursorMessages = 0, mouseMessages = 0;
int enterDown = 0, enterUp = 0, enterChars = 0;
int f12Down = 0, f12Up = 0;
HCURSOR clientCursor = nullptr;
LRESULT CALLBACK ClientProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (message == WM_SETCURSOR) {
        ++cursorMessages;
        SetCursor(clientCursor);
        return TRUE;
    }
    if (message == WM_MOUSEMOVE)
        ++mouseMessages;
    if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && w == VK_RETURN)
        ++enterDown;
    if ((message == WM_KEYUP || message == WM_SYSKEYUP) && w == VK_RETURN)
        ++enterUp;
    if (message == WM_KEYDOWN && w == VK_F12)
        ++f12Down;
    if (message == WM_KEYUP && w == VK_F12)
        ++f12Up;
    if ((message == WM_CHAR || message == WM_SYSCHAR) && (w == '\r' || w == '\n'))
        ++enterChars;
    return DefWindowProcW(window, message, w, l);
}
} // namespace
#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            printf("FAIL %d: %s\n", __LINE__, #x);                                                           \
            return 1;                                                                                        \
        }                                                                                                    \
    } while (0)
int main() {
    PresentationReload reload;
    D3DPRESENT_PARAMETERS native{};
    native.MultiSampleType = D3DMULTISAMPLE_8_SAMPLES;
    native.MultiSampleQuality = 2;
    reload.initialize(native, 4);
    CHECK(!reload.changed(4) && reload.changed(0));
    CHECK(reload.cooperative(D3D_OK) == D3D_OK);
    reload.request(0);
    CHECK(reload.pending() && reload.cooperative(D3D_OK) == D3DERR_DEVICENOTRESET);
    CHECK(reload.cooperative(D3DERR_DEVICELOST) == D3DERR_DEVICELOST);
    CHECK(reload.cooperative(D3DERR_DRIVERINTERNALERROR) == D3DERR_DRIVERINTERNALERROR);
    D3DPRESENT_PARAMETERS restored{};
    reload.parameters(restored, 0);
    CHECK(restored.MultiSampleType == native.MultiSampleType && restored.MultiSampleQuality == 2);
    reload.complete(D3DERR_DEVICELOST);
    CHECK(reload.pending());
    reload.complete(D3D_OK);
    CHECK(!reload.pending() && reload.cooperative(D3D_OK) == D3D_OK);
    CreateDirectoryW(ModuleDirectory().c_str(), nullptr);
    auto config = ModuleDirectory() + L"FENB.ini";
    DeleteFileW(config.c_str());
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    clientCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32515)); // deliberately not IDC_ARROW
    wc.lpfnWndProc = ClientProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"FENBRuntimeTest";
    RegisterClassW(&wc);
    HWND window = CreateWindowW(wc.lpszClassName, L"Runtime test", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600,
                                nullptr, nullptr, instance, nullptr);
    Com<IDirect3D9> d3d;
    d3d.p = Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(d3d.p);
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.BackBufferWidth = 800;
    pp.BackBufferHeight = 600;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.hDeviceWindow = window;
    Com<IDirect3DDevice9> device;
    CHECK(SUCCEEDED(d3d->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp,
                                      device.out())));
    {
        Runtime runtime(device);
        runtime.settings=Settings{};
        runtime.settings.enabled=runtime.settings.sfx=true;
        runtime.settings.skateTrailCustomColor=true;
        runtime.settings.skateTrailR=1;
        runtime.settings.skateTrailG=runtime.settings.skateTrailB=0;
        D3DMATRIX identity{},projection{};
        identity._11=identity._22=identity._33=identity._44=1;
        projection._11=1.25f;projection._22=1.75f;
        projection._33=1.001f;projection._34=1;projection._43=-.1f;
        device->SetTransform(D3DTS_WORLD,&identity);
        device->SetTransform(D3DTS_VIEW,&identity);
        device->SetTransform(D3DTS_PROJECTION,&projection);
        device->SetFVF(0x142);
        device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
        Com<IDirect3DVertexBuffer9> nativeStream;
        CHECK(SUCCEEDED(device->CreateVertexBuffer(256,0,0x142,D3DPOOL_DEFAULT,nativeStream.out(),nullptr)));
        CHECK(SUCCEEDED(device->SetStreamSource(0,nativeStream,0,24)));
        std::array<native_skate::Node,512> pool{};
        constexpr size_t count=6;
        for(size_t i=0;i<count;++i) {
            const float y=.9f-float(i)*.37f;
            pool[i].left={-.3f,y,4};
            pool[i].right={.3f,y,4};
            pool[i].next=i+1<count?&pool[i+1]:nullptr;
        }
        std::array<DWORD,11> nativeTrail{};
        nativeTrail[4]=count;
        nativeTrail[5]=reinterpret_cast<uintptr_t>(pool.data());
        nativeTrail[10]=reinterpret_cast<uintptr_t>(pool.data());
        const ParticleDraw geometry{D3DPT_TRIANGLESTRIP,10,nullptr,0,nullptr,D3DFMT_UNKNOWN,0,0,
                                    {D3DPT_TRIANGLESTRIP,0,10}};
        Com<IDirect3DSurface9> target,pixels;
        CHECK(SUCCEEDED(device->GetRenderTarget(0,target.out())));
        CHECK(SUCCEEDED(device->CreateOffscreenPlainSurface(800,600,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,pixels.out(),nullptr)));
        auto originalDraw=[](){return D3D_OK;};
        for(int style=0;style<=5;++style) {
            runtime.settings.skateTrailStyle=float(style);
            native_skate::Context scope{nativeTrail.data()};
            CHECK(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff202020,1,0)));
            CHECK(SUCCEEDED(device->BeginScene()));
            runtime.scene(true);
            runtime.beforeDraw(10);
            native_skate::current=&scope;
            const bool replaced=runtime.beforeWaterDraw(0,originalDraw,geometry);
            runtime.afterWaterDraw();
            native_skate::current=nullptr;
            CHECK(replaced==(style>0)&&scope.replaced==replaced);
            runtime.scene(false);
            CHECK(SUCCEEDED(device->EndScene()));
            DWORD fvf=0,blend=0;UINT offset=0,stride=0;
            Com<IDirect3DVertexBuffer9> restored;
            CHECK(SUCCEEDED(device->GetFVF(&fvf))&&fvf==0x142);
            CHECK(SUCCEEDED(device->GetRenderState(D3DRS_DESTBLEND,&blend))&&blend==D3DBLEND_INVSRCALPHA);
            CHECK(SUCCEEDED(device->GetStreamSource(0,restored.out(),&offset,&stride))&&restored.p==nativeStream.p&&offset==0&&stride==24);
            CHECK(SUCCEEDED(device->GetRenderTargetData(target,pixels)));
            D3DLOCKED_RECT lock{};
            CHECK(SUCCEEDED(pixels->LockRect(&lock,nullptr,D3DLOCK_READONLY)));
            unsigned colored=0,other=0;
            for(int y=140;y<450;++y) {
                const auto *row=reinterpret_cast<const DWORD*>(static_cast<BYTE*>(lock.pBits)+y*lock.Pitch);
                for(int x=280;x<520;++x) {
                    colored+=((row[x]>>16)&255)>38;
                    other+=(row[x]&0xffff)!=0x2020;
                }
            }
            pixels->UnlockRect();
            CHECK(other==0&&(style?colored>100:colored==0));
        }
        device->SetStreamSource(0,nullptr,0,0);
        puts("PASS: native skate draw replacement, five styles, custom tint and device state restoration.");

        struct Spark { float x,y,z; DWORD color; };
        const Spark sprite{0,0,4,0xffffffff};
        Com<IDirect3DVertexBuffer9> sparkStream;
        CHECK(SUCCEEDED(device->CreateVertexBuffer(sizeof(Spark),0,0x42,D3DPOOL_MANAGED,sparkStream.out(),nullptr)));
        void *vertices=nullptr;CHECK(SUCCEEDED(sparkStream->Lock(0,0,&vertices,0)));
        memcpy(vertices,&sprite,sizeof(sprite));CHECK(SUCCEEDED(sparkStream->Unlock()));
        Com<IDirect3DTexture9> originalArt;
        CHECK(SUCCEEDED(device->CreateTexture(1,1,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,originalArt.out(),nullptr)));
        D3DLOCKED_RECT lock{};
        CHECK(SUCCEEDED(originalArt->LockRect(0,&lock,nullptr,0)));
        *static_cast<DWORD *>(lock.pBits)=0xff808080;
        CHECK(SUCCEEDED(originalArt->UnlockRect(0)));
        device->SetTexture(0,originalArt);
        device->SetStreamSource(0,sparkStream,0,sizeof(Spark));
        device->SetFVF(0x42);
        runtime.invalidateDrawState();
        device->SetRenderState(D3DRS_POINTSPRITEENABLE,TRUE);
        device->SetRenderState(D3DRS_POINTSCALEENABLE,FALSE);
        device->SetRenderState(D3DRS_POINTSIZE,std::bit_cast<DWORD>(72.f));
        device->SetRenderState(D3DRS_POINTSIZE_MAX,std::bit_cast<DWORD>(96.f));
        device->SetRenderState(D3DRS_LIGHTING,FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
        device->SetRenderState(D3DRS_SRCBLEND,D3DBLEND_ONE);
        device->SetRenderState(D3DRS_DESTBLEND,D3DBLEND_ONE);
        device->SetRenderState(D3DRS_TEXTUREFACTOR,0xffaabbcc);
        device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
        device->SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);
        device->SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        const ParticleDraw spark{D3DPT_POINTLIST,1,nullptr,0,nullptr,D3DFMT_UNKNOWN,0,0,{D3DPT_POINTLIST,0,1}};
        std::array<uint64_t,6> silhouettes{};
        for(int style=0;style<=6;++style) {
            runtime.settings.skateParticleStyle=float(style==6?0:style);
            runtime.settings.skateParticleCustomColor=style==6;
            runtime.settings.skateParticleR=runtime.settings.skateParticleB=0;
            runtime.settings.skateParticleG=1;
            CHECK(SUCCEEDED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff101010,1,0)));
            CHECK(SUCCEEDED(device->BeginScene()));
            runtime.scene(true);runtime.beforeDraw(1);
            native_skate_particles::active=true;
            CHECK(!runtime.beforeWaterDraw(0,originalDraw,spark));
            CHECK(SUCCEEDED(device->DrawPrimitive(D3DPT_POINTLIST,0,1)));
            runtime.afterWaterDraw();native_skate_particles::active=false;
            runtime.scene(false);
            CHECK(SUCCEEDED(device->EndScene()));
            DWORD stage=0,factor=0;
            Com<IDirect3DBaseTexture9> restoredArt;
            CHECK(SUCCEEDED(device->GetTexture(0,restoredArt.out()))&&restoredArt.p==originalArt.p);
            CHECK(SUCCEEDED(device->GetRenderState(D3DRS_TEXTUREFACTOR,&factor))&&factor==0xffaabbcc);
            CHECK(SUCCEEDED(device->GetTextureStageState(0,D3DTSS_COLOROP,&stage))&&stage==D3DTOP_SELECTARG1);
            CHECK(SUCCEEDED(device->GetRenderTargetData(target,pixels)));
            CHECK(SUCCEEDED(pixels->LockRect(&lock,nullptr,D3DLOCK_READONLY)));
            uint64_t shape=14695981039346656037ull;
            std::array<unsigned,3> peak{};
            int changed=0;
            for(int y=250;y<350;++y) {
                const auto *row=reinterpret_cast<const DWORD *>(static_cast<BYTE *>(lock.pBits)+y*lock.Pitch);
                for(int x=350;x<450;++x) {
                    const DWORD rgb=row[x]&0xffffff;
                    changed+=rgb!=0x101010;
                    for(size_t c=0;c<peak.size();++c)peak[c]=std::max(peak[c],unsigned((rgb>>(16-c*8))&255));
                    shape=(shape^unsigned(rgb!=0x101010))*1099511628211ull;
                }
            }
            CHECK(SUCCEEDED(pixels->UnlockRect()));
            CHECK(changed>70);
            if(style<=5) {
                for(int i=0;i<style;++i)CHECK(silhouettes[i]!=shape);
                silhouettes[style]=shape;
            }
            if(style==6)CHECK(peak[0]==16&&peak[1]>100&&peak[2]==16);
        }
        device->SetTexture(0,nullptr);
        device->SetStreamSource(0,nullptr,0,0);
        device->SetRenderState(D3DRS_POINTSPRITEENABLE,FALSE);
        puts("PASS: native skate point sprites, five masks, custom color and draw-state restoration.");
    }
    {
        Runtime runtime(device);
        runtime.visible = true;
        runtime.settings.exposure = .75f;
        runtime.settingsChanged();
        CHECK(runtime.pendingSave());
        runtime.beforePresent(nullptr);
        CHECK(GetFileAttributesW(config.c_str()) == INVALID_FILE_ATTRIBUTES);
        int cursorBefore = cursorMessages;
        int mouseBefore = mouseMessages;
        const int initialCursorCount = ShowCursor(TRUE) - 1;
        ShowCursor(FALSE);
        SetCursor(clientCursor);
        for (int i = 0; i < 12; ++i) {
            SendMessageW(window, WM_SETCURSOR, reinterpret_cast<WPARAM>(window),
                         MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
            CHECK(GetCursor() == nullptr);
            // Neuz may reinstate its cursor directly from its frame loop, with no window message.
            SetCursor(clientCursor);
            // This fixture is hidden, so CURSORINFO can describe another application's
            // foreground queue. Check this input thread's actual visibility count.
            CHECK(ShowCursor(TRUE) - 1 < 0);
            ShowCursor(FALSE);
            SendMessageW(window, WM_MOUSEMOVE, 0, MAKELPARAM(30 + i, 30));
            runtime.beforePresent(nullptr);
            CHECK(ShowCursor(TRUE) - 1 < 0);
            ShowCursor(FALSE);
        }
        CHECK(cursorMessages == cursorBefore); // client cannot reinstate its cursor over the custom pointer
        CHECK(mouseMessages == mouseBefore); // game input stays blocked while the panel is open
        runtime.settings.customCursor = false;
        runtime.beforePresent(nullptr);
        CHECK(GetCursor() == clientCursor);
        SendMessageW(window, WM_SETCURSOR, reinterpret_cast<WPARAM>(window), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        CHECK(GetCursor() == clientCursor && cursorMessages == cursorBefore + 1);
        runtime.settings.customCursor = true;
        SendMessageW(window, WM_SETCURSOR, reinterpret_cast<WPARAM>(window), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        CHECK(GetCursor() == nullptr);
        SendMessageW(window, WM_SETCURSOR, reinterpret_cast<WPARAM>(window), MAKELPARAM(HTCAPTION, WM_MOUSEMOVE));
        CHECK(GetCursor() == clientCursor); // window borders retain native cursor handling
        SendMessageW(window, WM_SETCURSOR, reinterpret_cast<WPARAM>(window), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        CHECK(GetCursor() == nullptr);
        SendMessageW(window, WM_KILLFOCUS, 0, 0);
        CHECK(GetCursor() == clientCursor);
        SendMessageW(window, WM_SETCURSOR, reinterpret_cast<WPARAM>(window), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
        CHECK(GetCursor() == nullptr);
        SendMessageW(window, WM_KEYDOWN, VK_ESCAPE, 1);
        CHECK(!runtime.visible && GetCursor() == clientCursor);
        CHECK(ShowCursor(TRUE) - 1 == initialCursorCount);
        ShowCursor(FALSE); // The panel restores exactly the visibility count it acquired.
        runtime.visible = false;
        SendMessageW(window, WM_MOUSEMOVE, 0, MAKELPARAM(50, 50));
        CHECK(mouseMessages == mouseBefore + 1);
        runtime.visible = true;
        BYTE originalKeys[256]{}, chord[256]{};
        GetKeyboardState(originalKeys);
        for (int shift : {VK_LSHIFT, VK_RSHIFT}) {
            runtime.visible = false;
            runtime.editorPage = 1;
            for (int toggle = 0; toggle < 2; ++toggle) {
                memset(chord, 0, sizeof(chord));
                chord[VK_SHIFT] = chord[shift] = 0x80;
                SetKeyboardState(chord);
                SendMessageW(window, WM_KEYDOWN, VK_RETURN, 1);
                CHECK(runtime.visible == (toggle == 0));
                if (toggle == 0)
                    CHECK(runtime.editorPage == 0);
                SendMessageW(window, WM_KEYDOWN, VK_RETURN, static_cast<LPARAM>(0x40000001u));
                CHECK(runtime.visible == (toggle == 0));
                SendMessageW(window, WM_CHAR, '\r', 1);
                memset(chord, 0, sizeof(chord));
                SetKeyboardState(chord); // release Shift before Enter
                SendMessageW(window, WM_KEYUP, VK_RETURN, static_cast<LPARAM>(0xC0000001u));
                SendMessageW(window, WM_CHAR, '\r', 1); // delayed TranslateMessage character
                SendMessageW(window, WM_SYSCHAR, '\n', 1);
            }
        }
        CHECK(enterDown == 0 && enterUp == 0 && enterChars == 0);
        SendMessageW(window, WM_KEYDOWN, VK_RETURN, 1);
        SendMessageW(window, WM_CHAR, '\r', 1);
        SendMessageW(window, WM_KEYUP, VK_RETURN, static_cast<LPARAM>(0xC0000001u));
        CHECK(enterDown == 1 && enterUp == 1 && enterChars == 1); // normal chat Enter is preserved
        SendMessageW(window, WM_KEYDOWN, VK_F9, 1);
        CHECK(!runtime.visible); // old shortcut is gone
        for (bool panel : {false, true}) {
            runtime.visible = panel;
            const bool initial = runtime.settings.enabled;
            memset(chord, 0, sizeof(chord));
            chord[VK_RSHIFT] = chord[VK_SHIFT] = 0x80;
            SetKeyboardState(chord);
            SendMessageW(window, WM_KEYDOWN, VK_F12, 1);
            CHECK(runtime.settings.enabled != initial && runtime.visible == panel);
            SendMessageW(window, WM_KEYDOWN, VK_F12, static_cast<LPARAM>(0x40000001u));
            CHECK(runtime.settings.enabled != initial);
            memset(chord, 0, sizeof(chord));
            SetKeyboardState(chord);
            SendMessageW(window, WM_KEYUP, VK_F12, static_cast<LPARAM>(0xC0000001u));
        }
        CHECK(f12Down == 0 && f12Up == 0);
        runtime.visible = false;
        const bool initial = runtime.settings.enabled;
        for (int modifier : {0, VK_LSHIFT, VK_CONTROL, VK_MENU}) {
            memset(chord, 0, sizeof(chord));
            if (modifier)
                chord[modifier] = 0x80;
            if (modifier == VK_CONTROL || modifier == VK_MENU)
                chord[VK_RSHIFT] = 0x80;
            SetKeyboardState(chord);
            SendMessageW(window, WM_KEYDOWN, VK_F12, 1);
            SendMessageW(window, WM_KEYUP, VK_F12, static_cast<LPARAM>(0xC0000001u));
            CHECK(runtime.settings.enabled == initial);
        }
        CHECK(f12Down == 4 && f12Up == 4);
        SetKeyboardState(originalKeys);
        runtime.visible = true;
        runtime.settingsChanged();
        Sleep(500);
        runtime.beforePresent(nullptr);
        CHECK(!runtime.pendingSave());
        Settings loaded;
        CHECK(loaded.load(config));
        CHECK(loaded.exposure == .75f);
        runtime.settings.gamma = 1.2f;
        runtime.settingsChanged();
    }
    Settings after;
    CHECK(after.load(config));
    CHECK(after.gamma == 1.2f);
    auto profiles = ModuleDirectory() + L"FENB-presets\\";
    CreateDirectoryW(profiles.c_str(), nullptr);
    auto primary = profiles + L"Основной.ini";
    auto candidate = after;
    candidate.language = after.language == Language::English ? Language::Russian : Language::English;
    candidate.unlockFps = !after.unlockFps;
    candidate.msaaSamples = 4;
    CHECK(candidate.save(primary));
    {
        Runtime runtime(device);
        CHECK(runtime.selectedProfile == "Основной");
        CHECK(runtime.settings.language == after.language &&
              runtime.settings.msaaSamples == after.msaaSamples);
    }
    DeleteFileW(primary.c_str());
    after.preset(0);
    CHECK(after.save(config) && after.save(primary));
    {
        Runtime runtime(device);
        CHECK(runtime.selectedProfile.empty() && runtime.selectedBuiltIn == 0);
    }
    DeleteFileW(primary.c_str());
    RemoveDirectoryW(profiles.c_str());
    // Two clients share image tuning, without importing each other's switches.
    {
        Runtime first(device),second(device);
        first.visible=second.visible=false;
        first.settings.enabled=true;first.settings.unlockFps=false;first.settings.backgroundActivity=false;
        second.settings.enabled=false;second.settings.unlockFps=true;second.settings.backgroundActivity=true;
        first.settings.exposure=.35f;first.save();
        Sleep(520);second.beforePresent(nullptr);
        CHECK(!second.settings.enabled && second.settings.unlockFps && second.settings.backgroundActivity);
        CHECK(second.settings.exposure==.35f);
        second.settings.gamma=1.05f;second.save();
        Sleep(520);first.beforePresent(nullptr);
        CHECK(first.settings.enabled && !first.settings.unlockFps && !first.settings.backgroundActivity);
        CHECK(first.settings.gamma==1.05f);
        // Explicit Reload still applies the file to the window requesting it.
        first.reload();
        CHECK(!first.settings.enabled && first.settings.unlockFps && first.settings.backgroundActivity);
    }
    {
        Runtime runtime(device);
        runtime.configPath = ModuleDirectory() + L"missing\\FENB.ini";
        runtime.settingsChanged();
        runtime.save();
        CHECK(runtime.pendingSave());
        CHECK(runtime.message == UiText::ConfigWriteFailed);
    }
    DestroyWindow(window);
    DeleteFileW(config.c_str());
    puts("PASS: Shift+Enter without chat leakage, normal Enter preserved, single cursor, autosave, shutdown "
         "flush, independent client switches and shared image tuning.");
    return 0;
}
