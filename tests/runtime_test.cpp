#include "runtime.hpp"
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
    if ((message == WM_CHAR || message == WM_SYSCHAR) && (w == '\r' || w == '\n'))
        ++enterChars;
    return DefWindowProcW(window, message, w, l);
}
}
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
        runtime.visible = true;
        runtime.settings.exposure = .75f;
        runtime.settingsChanged();
        CHECK(runtime.pendingSave());
        runtime.beforePresent(nullptr);
        CHECK(GetFileAttributesW(config.c_str()) == INVALID_FILE_ATTRIBUTES);
        int cursorBefore = cursorMessages;
        int mouseBefore = mouseMessages;
        for (int i = 0; i < 12; ++i) {
            SendMessageW(window, WM_SETCURSOR, reinterpret_cast<WPARAM>(window), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
            CHECK(GetCursor() == clientCursor);
            SendMessageW(window, WM_MOUSEMOVE, 0, MAKELPARAM(30 + i, 30));
            runtime.beforePresent(nullptr);
            CHECK(GetCursor() == clientCursor);
        }
        CHECK(cursorMessages == cursorBefore + 12);
        CHECK(mouseMessages == mouseBefore); // game input stays blocked while the panel is open
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
                memset(chord, 0, sizeof(chord));chord[VK_SHIFT]=chord[shift]=0x80;
                SetKeyboardState(chord);
                SendMessageW(window,WM_KEYDOWN,VK_RETURN,1);
                CHECK(runtime.visible == (toggle == 0));
                if(toggle==0)CHECK(runtime.editorPage==0);
                SendMessageW(window,WM_KEYDOWN,VK_RETURN,static_cast<LPARAM>(0x40000001u));
                CHECK(runtime.visible == (toggle == 0));
                SendMessageW(window,WM_CHAR,'\r',1);
                memset(chord,0,sizeof(chord));SetKeyboardState(chord); // release Shift before Enter
                SendMessageW(window,WM_KEYUP,VK_RETURN,static_cast<LPARAM>(0xC0000001u));
                SendMessageW(window,WM_CHAR,'\r',1); // delayed TranslateMessage character
                SendMessageW(window,WM_SYSCHAR,'\n',1);
            }
        }
        CHECK(enterDown == 0 && enterUp == 0 && enterChars == 0);
        SendMessageW(window,WM_KEYDOWN,VK_RETURN,1);
        SendMessageW(window,WM_CHAR,'\r',1);
        SendMessageW(window,WM_KEYUP,VK_RETURN,static_cast<LPARAM>(0xC0000001u));
        CHECK(enterDown == 1 && enterUp == 1 && enterChars == 1); // normal chat Enter is preserved
        SendMessageW(window,WM_KEYDOWN,VK_F9,1);
        CHECK(!runtime.visible); // old shortcut is gone
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
    puts("PASS: Shift+Enter without chat leakage, normal Enter preserved, single cursor, autosave, shutdown flush.");
    return 0;
}
