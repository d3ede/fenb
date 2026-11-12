// Isolated visual study. Deliberately has no Runtime/Settings dependency.
#include "common.hpp"
#include "localization.hpp"
#include "version.hpp"
#include "generated/ui_font.hpp"
#include "generated/ui_logo.hpp"
#include "imgui.h"
#include "ui_cursor.hpp"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx9.h"
#include <gdiplus.h>
#include <cstdio>
#include <filesystem>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
constexpr int Width = 800, Height = 640;
constexpr float PanelWidth = 560, PanelHeight = 540;
constexpr ImU32 Ink = IM_COL32(223, 224, 229, 255), Muted = IM_COL32(143, 146, 155, 255);
constexpr ImU32 Violet = IM_COL32(174, 148, 210, 255), Line = IM_COL32(51, 53, 59, 255);
constexpr ImU32 Surface = IM_COL32(32, 33, 37, 255);
Language language = Language::English;
bool motion = true, customCursor = true, running = true, captureFrames = false;
int page = 0, subpage = 0, profile = 0, weather = 0;
bool switches[8] = {true, true, true, true, true, true, true, true};
float values[14] = {.38f, .72f, .50f, .55f, .58f, .32f, .45f, .20f, .65f, .42f, .18f, .35f, .50f, .35f};
double pageStart = 0;
IDirect3DTexture9 *logo = nullptr;
ImVec2 origin{};
ImDrawList *draw = nullptr;

const char *Tr(UiText id) { return Translate(language, id); }
ImVec2 P(float x, float y) { return {origin.x + x, origin.y + y}; }
ImU32 Mix(ImU32 a, ImU32 b, float t) {
    auto av = ImGui::ColorConvertU32ToFloat4(a), bv = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32({av.x + (bv.x - av.x) * t, av.y + (bv.y - av.y) * t,
        av.z + (bv.z - av.z) * t, av.w + (bv.w - av.w) * t});
}
float Animate(const char *key, float target, float speed = 15) {
    auto *storage = ImGui::GetStateStorage();
    auto id = ImGui::GetID(key);
    float v = storage->GetFloat(id, target);
    v = motion && !captureFrames ? v + (target - v) * (1 - std::exp(-ImGui::GetIO().DeltaTime * speed)) : target;
    storage->SetFloat(id, v);
    return v;
}
void Text(float x, float y, const char *s, float size = 14, ImU32 color = Ink) {
    draw->AddText(ImGui::GetFont(), size, P(std::floor(x), std::floor(y)), color, s);
}
void Text(float x, float y, UiText id, float size = 14, ImU32 color = Ink) {
    Text(x, y, Tr(id), size, color);
}
void Rect(float x, float y, float w, float h, ImU32 color, float rounding = 6) {
    draw->AddRectFilled(P(x, y), P(x + w, y + h), color, rounding);
}
bool Hit(const char *id, float x, float y, float w, float h) {
    ImGui::SetCursorScreenPos(P(x, y));
    bool pressed = ImGui::InvisibleButton(id, {w, h}, ImGuiButtonFlags_EnableNav);
    if (ImGui::IsItemFocused() && ImGui::GetIO().NavVisible)
        draw->AddRect(P(x - 2, y - 2), P(x + w + 2, y + h + 2), Violet, 4);
    return pressed;
}
void Tip(UiText id) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(300);
        ImGui::TextUnformatted(Tr(id));
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
void Switch(const char *id, float x, float y, bool &value) {
    ImGui::PushID(id);
    if (Hit("switch", x - 4, y - 6, 42, 30) ||
        (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter))) value = !value;
    const float t = Animate("position", value ? 1.f : 0.f);
    Rect(x, y, 28, 15, Mix(IM_COL32(56, 58, 64, 255), IM_COL32(86, 89, 98, 255), t), 8);
    draw->AddCircleFilled(P(x + 7.5f + 13 * t, y + 7.5f), 4.5f,
        Mix(Muted, IM_COL32(224, 225, 231, 255), t), 20);
    Tip(value ? UiText::V2On : UiText::V2Off);
    ImGui::PopID();
}

void Slider(const char *id, float y, UiText label, float &value, bool clock = false) {
    ImGui::PushID(id);
    Text(28, y + 5, label, 13, Muted);
    Hit("track", 203, y, 257, 25);
    bool hover = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    if (active && ImGui::IsMouseDown(0))
        value = std::clamp((ImGui::GetIO().MousePos.x - P(210, 0).x) / 242, 0.f, 1.f);
    if (ImGui::IsItemFocused()) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) value = std::max(0.f, value - .01f);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) value = std::min(1.f, value + .01f);
    }
    float t = Animate("hover", hover || active ? 1.f : 0.f);
    draw->AddLine(P(210, y + 13), P(452, y + 13), IM_COL32(75, 77, 84, 255), 1.5f);
    draw->AddLine(P(210, y + 13), P(210 + 242 * value, y + 13),
        Mix(IM_COL32(130, 133, 143, 255), Violet, t), 2);
    float knob = 210 + 242 * value;
    Rect(knob - 2, y + 8, 4, 10, active ? Violet : Ink, 1);
    Rect(475, y + 2, 55, 23, IM_COL32(39, 40, 45, 255), 3);
    char number[24];
    if (clock) {
        int minutes = std::min(1439, int(value * 1440));
        std::snprintf(number, sizeof(number), "%02d:%02d", minutes / 60, minutes % 60);
    } else std::snprintf(number, sizeof(number), "%.2f", value);
    Text(486, y + 6, number, 12, Ink);
    ImGui::PopID();
}
void Section(const char *id, float y, UiText title, int toggle = -1) {
    ImGui::PushID(id);
    Rect(16, y, 528, 29, IM_COL32(38, 39, 44, 255), 3);
    Text(28, y + 7, title, 13);
    if (toggle >= 0) Switch("enable", 504, y + 7, switches[toggle]);
    ImGui::PopID();
}
void ChangePage(int next, int section = 0) {
    page = next; subpage = section; pageStart = ImGui::GetTime();
}
void NavRow(const char *id, float y, UiText title, int target) {
    bool clicked = Hit(id, 16, y, 528, 29);
    if (ImGui::IsItemHovered()) Rect(16, y, 528, 29, IM_COL32(38, 39, 44, 255), 3);
    Text(28, y + 7, title, 13);
    Text(514, y + 5, "→", 15, Muted);
    draw->AddLine(P(16, y + 29), P(544, y + 29), Line);
    if (clicked) ChangePage(0, target);
}
void ResetDemo() {
    const float defaults[] = {.38f, .72f, .50f, .55f, .58f, .32f, .45f, .20f, .65f, .42f, .18f, .35f, .50f, .35f};
    std::copy(std::begin(defaults), std::end(defaults), values);
    std::fill(std::begin(switches), std::end(switches), true);
    switches[7] = true; profile = weather = 0;
}
void Effects() {
    if (subpage != 0) {
        if (Hit("back", 16, 91, 90, 27)) ChangePage(0);
        Text(26, 98, UiText::BackButton, 12, Muted);
        if (subpage == 1) {
            Section("sky", 133, UiText::V2Sky, 2);
            Slider("haze", 174, UiText::V2Fog, values[5]);
            Slider("horizon", 203, UiText::Horizon, values[6]);
            Section("rays", 251, UiText::V2Rays, 3);
            Slider("ray-strength", 292, UiText::V2Intensity, values[7]);
            Slider("ray-radius", 321, UiText::V2Radius, values[8]);
            Section("depth", 369, UiText::DepthOfField, 4);
            Slider("focus", 410, UiText::Focus, values[11]);
            Slider("blur", 439, UiText::Blur, values[12]);
        } else {
            Section("water", 133, UiText::Surface, 2);
            Slider("waves", 174, UiText::WaveHeight, values[5]);
            Slider("speed", 203, UiText::Speed, values[6]);
            Slider("ripples", 232, UiText::Ripples, values[7]);
            Section("depth", 280, UiText::DepthReflections, 3);
            Slider("refraction", 321, UiText::Refraction, values[8]);
            Slider("opacity", 350, UiText::Opacity, values[11]);
            Slider("reflection", 379, UiText::SkyReflection, values[12]);
        }
        return;
    }
    NavRow("atmosphere", 91, UiText::LightAtmosphere, 1);
    NavRow("water", 123, UiText::V2Water, 2);
    Section("grade", 168, UiText::ColorCorrection, 1);
    Slider("exposure", 205, UiText::Exposure, values[2]);
    Slider("contrast", 234, UiText::Contrast, values[3]);
    Slider("saturation", 263, UiText::Saturation, values[4]);
    Slider("temperature", 292, UiText::Temperature, values[5]);
    Section("bloom", 337, UiText::V2Bloom, 0);
    Slider("bloom-strength", 374, UiText::V2Intensity, values[0]);
    Slider("threshold", 403, UiText::V2Threshold, values[1]);
    Slider("radius", 432, UiText::V2Radius, values[6]);
    Text(28, 476, UiText::V2AA, 12, Muted);
    Text(369, 476, "SMAA", 12, Ink);
    Switch("aa", 504, 474, switches[4]);
}
void World() {
    Section("time", 100, UiText::V2Time, 5);
    Slider("time-of-day", 146, UiText::V2Time, values[9], true);
    Text(28, 184, UiText::V2TimeNote, 12, Muted);
    Section("weather", 232, UiText::V2Weather, 6);
    const UiText labels[] = {UiText::V2Clear, UiText::V2Rain, UiText::V2Snow};
    for (int i = 0; i < 3; ++i) {
        ImGui::PushID(i);
        float x = 28 + i * 168.f;
        if (Hit("weather", x, 279, 152, 28)) weather = i;
        Rect(x, 279, 152, 28, weather == i ? IM_COL32(55, 57, 64, 255) : IM_COL32(37, 38, 43, 255), 3);
        Text(x + 15, 287, labels[i], 12, weather == i ? Ink : Muted);
        ImGui::PopID();
    }
    Section("sky", 349, UiText::V2Sky, 2);
    Slider("haze", 395, UiText::V2Fog, values[10]);
    Slider("clouds", 424, UiText::Clouds, values[11]);
}
void Profiles() {
    Text(20, 105, UiText::V2Profiles, 16);
    const UiText titles[] = {UiText::V2Natural, UiText::V2Golden, UiText::V2Moonlit};
    const UiText notes[] = {UiText::V2NaturalNote, UiText::V2GoldenNote, UiText::V2MoonlitNote};
    for (int i = 0; i < 3; ++i) {
        ImGui::PushID(i);
        float y = 146 + i * 91.f;
        if (Hit("profile", 16, y, 528, 76)) profile = i;
        Rect(16, y, 528, 76, ImGui::IsItemHovered() ? IM_COL32(40, 41, 47, 255) : Surface, 4);
        draw->AddRect(P(16, y), P(544, y + 76), profile == i ? IM_COL32(90, 91, 104, 255) : Line, 4);
        Text(30, y + 16, titles[i], 14);
        Text(30, y + 42, notes[i], 12, Muted);
        draw->AddCircle(P(515, y + 37), 6, profile == i ? Violet : Muted, 24);
        if (profile == i) draw->AddCircleFilled(P(515, y + 37), 3, Violet, 16);
        ImGui::PopID();
    }
    Text(28, 452, UiText::V2ProfileHint, 12, Muted);
}
void DrawPointer(ImDrawList *d, ImVec2 tip, float scale = 1) {
    ImVec2 polygon[4];
    for (int i = 0; i < 4; ++i)
        polygon[i] = {tip.x + (CursorPoints[i].x - 1) * scale, tip.y + (CursorPoints[i].y - 1) * scale};
    // One concave path, triangulated by ImGui: no overlapping hand-built triangles.
    d->AddConcavePolyFilled(polygon, 4, CursorFill);
    d->AddPolyline(polygon, 4, CursorEdge, CursorWeight * scale, ImDrawFlags_Closed);
}
void Appearance() {
    Section("appearance", 100, UiText::V2Appearance);
    Text(28, 151, UiText::V2Cursor, 14);
    Text(28, 176, UiText::V2CursorNote, 11, Muted);
    Switch("custom-cursor", 504, 152, customCursor);
    draw->AddLine(P(28, 208), P(532, 208), Line);
    Text(28, 232, UiText::V2Motion, 14);
    Text(28, 257, UiText::V2MotionNote, 11, Muted);
    Switch("motion", 504, 233, motion);
    Rect(16, 307, 528, 179, IM_COL32(26, 27, 31, 255), 4);
    draw->AddRect(P(16, 307), P(544, 486), Line, 4);
    Text(32, 330, UiText::V2CursorSample, 14);
    Text(32, 357, UiText::V2CursorSampleNote, 11, Muted);
    DrawPointer(draw, P(45, 409));
    Text(70, 410, "14 × 14 px", 11, Muted);
    DrawPointer(draw, P(442, 391), 4);
}
void Cursor() {
    auto p = ImGui::GetIO().MousePos;
    const auto size = ImGui::GetIO().DisplaySize;
    if (!customCursor || p.x < 0 || p.y < 0 || p.x >= size.x || p.y >= size.y) return;
    ImGui::SetMouseCursor(ImGuiMouseCursor_None);
    // Immediate hotspot tracking. No interpolation, halo, trail or click ring.
    DrawPointer(ImGui::GetForegroundDrawList(), {std::floor(p.x), std::floor(p.y)});
}
void Panel() {
    const auto display = ImGui::GetIO().DisplaySize;
    origin = {(display.x - PanelWidth) / 2, (display.y - PanelHeight) / 2};
    draw = ImGui::GetBackgroundDrawList();
    draw->AddRectFilled({0, 0}, display, IM_COL32(26, 28, 33, 255));
    Text(0, -23, UiText::V2CompactStudy, 10, Muted);
    for (int i = 10; i > 0; --i)
        draw->AddRect(P(-float(i), -float(i)), P(PanelWidth + i, PanelHeight + i), IM_COL32(0, 0, 0, 8), 5.f + i);
    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize({PanelWidth, PanelHeight});
    ImGui::Begin("FENB-v2-compact", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    draw = ImGui::GetWindowDrawList();
    Rect(0, 0, PanelWidth, PanelHeight, IM_COL32(29, 30, 34, 255), 5);
    draw->AddRect(P(0, 0), P(PanelWidth, PanelHeight), Line, 5);
    if (logo) {
        float h = 28.f * UI_LOGO_HEIGHT / UI_LOGO_WIDTH;
        draw->AddImage(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(logo))), P(11, 22 - h / 2), P(39, 22 + h / 2));
    }
    Text(47, 15, "FENB", 13, Ink);
    const UiText tabs[] = {UiText::Image, UiText::WorldTab};
    const float xs[] = {103, 220}, widths[] = {106, 55};
    for (int i = 0; i < 2; ++i) {
        ImGui::PushID(i);
        if (Hit("tab", xs[i], 7, widths[i], 30)) ChangePage(i);
        bool selected = page == i;
        if (ImGui::IsItemHovered()) Rect(xs[i], 7, widths[i], 30, IM_COL32(39, 40, 45, 255), 3);
        Text(xs[i] + 9, 15, tabs[i], 13, selected ? Ink : Muted);
        if (selected) Rect(xs[i] + 9, 37, widths[i] - 18, 2, Violet, 1);
        ImGui::PopID();
    }
    const char *languages[] = {Tr(UiText::V2English), Tr(UiText::V2Russian)};
    for (int i = 0; i < 2; ++i) {
        ImGui::PushID(i + 10);
        if (Hit("language", 331 + i * 31.f, 8, 28, 27)) language = i ? Language::Russian : Language::English;
        Text(336 + i * 31.f, 16, languages[i], 11, int(language == Language::Russian) == i ? Ink : Muted);
        ImGui::PopID();
    }
    Switch("master", 408, 15, switches[7]);
    Text(442, 16, UiText::Effects, 11, Muted);
    if (Hit("appearance", 500, 8, 25, 27)) ChangePage(3);
    for (int dot = 0; dot < 3; ++dot)
        draw->AddCircleFilled(P(506 + dot * 5.f, 22), 1, page == 3 ? Ink : Muted, 8);
    Tip(UiText::V2Appearance);
    if (Hit("close", 529, 8, 23, 27)) running = false;
    Text(536, 13, "×", 15, Muted);
    Tip(UiText::V2CloseHelp);
    draw->AddLine(P(1, 44), P(559, 44), Line);
    Text(16, 62, UiText::Profile, 12, Muted);
    if (Hit("profiles", 89, 53, 314, 27)) ChangePage(2);
    Rect(89, 53, 314, 27, IM_COL32(39, 40, 45, 255), 3);
    const UiText profiles[] = {UiText::V2Natural, UiText::V2Golden, UiText::V2Moonlit};
    Text(100, 61, profiles[profile], 12);
    draw->AddLine(P(383, 64), P(387, 68), Muted, 1);
    draw->AddLine(P(387, 68), P(391, 64), Muted, 1);
    if (Hit("reset", 421, 53, 123, 27)) ResetDemo();
    if (ImGui::IsItemHovered()) Rect(421, 53, 123, 27, IM_COL32(40, 41, 46, 255), 3);
    Text(433, 61, UiText::V2Reset, 11, Muted);
    float enter = motion && !captureFrames ? std::clamp(float(ImGui::GetTime() - pageStart) / .14f, 0.f, 1.f) : 1;
    float shift = std::floor(3 * (1 - enter) * (1 - enter));
    origin.y += shift;
    ImGui::PushID(page);
    if (page == 0) Effects();
    if (page == 1) World();
    if (page == 2) Profiles();
    if (page == 3) Appearance();
    ImGui::PopID();
    origin.y -= shift;
    draw->AddLine(P(1, 509), P(559, 509), Line);
    Text(16, 520, FENBVersion, 11, Muted);
    Text(66, 520, UiText::V2Sandbox, 10, Muted);
    Text(510, 520, "UI v2", 10, Muted);
    ImGui::End();
    Cursor();
}

LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    if (ImGui_ImplWin32_WndProcHandler(window, message, w, l)) return 1;
    if (message == WM_DESTROY) { PostQuitMessage(0); return 0; }
    if (message == WM_SYSCOMMAND && (w & 0xfff0) == SC_KEYMENU) return 0;
    return DefWindowProcW(window, message, w, l);
}
bool Capture(IDirect3DDevice9 *device, const std::filesystem::path &path) {
    Com<IDirect3DSurface9> source, staging;
    if (FAILED(device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, source.out())) ||
        FAILED(device->CreateOffscreenPlainSurface(Width, Height, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, staging.out(), nullptr)) ||
        FAILED(device->GetRenderTargetData(source, staging))) return false;
    D3DLOCKED_RECT lock{};
    if (FAILED(staging->LockRect(&lock, nullptr, D3DLOCK_READONLY))) return false;
    UINT count = 0, size = 0;
    Gdiplus::GetImageEncodersSize(&count, &size);
    std::vector<BYTE> data(size);
    auto *codecs = reinterpret_cast<Gdiplus::ImageCodecInfo *>(data.data());
    Gdiplus::GetImageEncoders(count, size, codecs);
    bool result = false;
    for (UINT i = 0; i < count; ++i) {
        if (wcscmp(codecs[i].MimeType, L"image/png")) continue;
        Gdiplus::Bitmap bitmap(Width, Height, lock.Pitch, PixelFormat32bppRGB, static_cast<BYTE *>(lock.pBits));
        result = bitmap.Save(path.c_str(), &codecs[i].Clsid, nullptr) == Gdiplus::Ok;
        break;
    }
    staging->UnlockRect();
    return result;
}
} // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR command, int) {
    bool capture = std::string(command).find("--capture") != std::string::npos;
    captureFrames = capture;
    if (std::string(command).find("--russian") != std::string::npos) language = Language::Russian;
    SetProcessDPIAware();
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    wc.lpszClassName = L"FENB.UI.V2.Concept";
    if (!RegisterClassExW(&wc)) return 1;
    RECT rect{0, 0, Width, Height};
    constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    AdjustWindowRect(&rect, style, FALSE);
    HWND window = CreateWindowW(wc.lpszClassName, L"FENB / UI v2 concept", style,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);
    if (!window) return 2;
    Com<IDirect3D9> root;
    root.p = Direct3DCreate9(D3D_SDK_VERSION);
    if (!root.p) return 3;
    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = Width;
    pp.BackBufferHeight = Height;
    pp.hDeviceWindow = window;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    Com<IDirect3DDevice9> device;
    if (FAILED(root->CreateDevice(0, D3DDEVTYPE_HAL, window, D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &pp, device.out()))) return 4;
    Com<IDirect3DTexture9> symbol;
    if (FAILED(device->CreateTexture(UI_LOGO_WIDTH, UI_LOGO_HEIGHT, 1, 0, D3DFMT_A8R8G8B8,
        D3DPOOL_MANAGED, symbol.out(), nullptr))) return 5;
    D3DLOCKED_RECT locked{};
    if (FAILED(symbol->LockRect(0, &locked, nullptr, 0))) return 6;
    for (unsigned y = 0; y < UI_LOGO_HEIGHT; ++y)
        memcpy(static_cast<BYTE *>(locked.pBits) + y * locked.Pitch, UI_LOGO_BGRA + y * UI_LOGO_WIDTH, UI_LOGO_WIDTH * 4);
    symbol->UnlockRect(0);
    logo = symbol;
    ImGui::CreateContext();
    auto &io = ImGui::GetIO();
    io.IniFilename = io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImFontConfig font;
    font.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF(const_cast<unsigned char *>(UI_FONT), sizeof(UI_FONT), 14, &font,
        io.Fonts->GetGlyphRangesCyrillic());
    ImGui::StyleColorsDark();
    auto &theme = ImGui::GetStyle();
    theme.WindowPadding = {0, 0};
    theme.WindowBorderSize = 0;
    theme.WindowRounding = 8;
    theme.Colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(Surface);
    theme.Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(Ink);
    theme.Colors[ImGuiCol_NavCursor] = ImGui::ColorConvertU32ToFloat4(Violet);
    if (!ImGui_ImplWin32_Init(window) || !ImGui_ImplDX9_Init(device)) return 7;
    Gdiplus::GdiplusStartupInput startup;
    ULONG_PTR gdiplus = 0;
    if (Gdiplus::GdiplusStartup(&gdiplus, &startup, nullptr) != Gdiplus::Ok) return 8;
    wchar_t exePath[32768]{};
    GetModuleFileNameW(nullptr, exePath, 32768);
    const auto output = std::filesystem::path(exePath).parent_path();
    if (!capture) ShowWindow(window, SW_SHOWDEFAULT);
    int frame = 0, result = 0;
    while (running) {
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (message.message == WM_QUIT) running = false;
        }
        if (!running) break;
        if (!capture && IsIconic(window)) { Sleep(30); continue; }
        HRESULT status = device->TestCooperativeLevel();
        if (status == D3DERR_DEVICELOST) { Sleep(30); continue; }
        if (status == D3DERR_DEVICENOTRESET) {
            ImGui_ImplDX9_InvalidateDeviceObjects();
            if (FAILED(device->Reset(&pp))) { Sleep(30); continue; }
            ImGui_ImplDX9_CreateDeviceObjects();
        }
        if (capture) {
            int view = (frame / 4) % 6;
            page = view < 4 ? view : 0;
            subpage = view < 4 ? 0 : view - 3;
            language = frame < 24 ? Language::English : Language::Russian;
        }
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) running = false;
        Panel();
        const char *utf8Title = Tr(UiText::V2Window);
        int titleLength = MultiByteToWideChar(CP_UTF8, 0, utf8Title, -1, nullptr, 0);
        std::wstring title(titleLength, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8Title, -1, title.data(), titleLength);
        SetWindowTextW(window, title.c_str());
        ImGui::Render();
        device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff15131c, 1, 0);
        if (SUCCEEDED(device->BeginScene())) {
            ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
            device->EndScene();
            if (capture && frame % 4 == 3) {
                std::wstring name = (language == Language::Russian ? L"russian-" : L"english-") +
                    std::to_wstring((frame / 4) % 6) + L".png";
                if (!Capture(device, output / name)) { result = 9; break; }
            }
        }
        device->Present(nullptr, nullptr, nullptr, nullptr);
        if (capture && ++frame == 48) break;
    }
    Gdiplus::GdiplusShutdown(gdiplus);
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroyWindow(window);
    UnregisterClassW(wc.lpszClassName, instance);
    return result;
}
