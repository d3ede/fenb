// Isolated visual study. Deliberately has no Runtime/Settings dependency.
#include "common.hpp"
#include "localization.hpp"
#include "version.hpp"
#include "generated/ui_font.hpp"
#include "generated/ui_logo.hpp"
#include "imgui.h"
#include "ui_cursor.hpp"
#include "ui_studio_logo.hpp"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx9.h"
#include <gdiplus.h>
#include <cstdio>
#include <filesystem>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace {
int Width = 800, Height = 640;
constexpr float PanelWidth = 560, PanelHeight = 540;
constexpr ImU32 Ink = IM_COL32(223, 224, 229, 255), Muted = IM_COL32(143, 146, 155, 255);
constexpr ImU32 Line = IM_COL32(51, 53, 59, 255);
ImU32 Violet = IM_COL32(174, 148, 210, 255);
ImU32 Purple = IM_COL32(143, 83, 239, 255), Lavender = IM_COL32(199, 166, 255, 255);
bool studio = false, vivid = false;
constexpr ImU32 Surface = IM_COL32(32, 33, 37, 255);
Language language = Language::English;
bool motion = true, customCursor = true, running = true, captureFrames = false;
int page = 0, subpage = 0, profile = 0, weather = 0;
enum Toggle { BloomOn, GradeOn, SkyOn, RaysOn, DepthOn, WaterOn, ReflectionOn,
    TimeOn, WeatherOn, WorldSkyOn, AaOn, MasterOn, ToggleCount };
bool switches[ToggleCount];
int aaMethod = 1;
enum Parameter { Exposure, Contrast, Saturation, Temperature, Bloom, Threshold, Radius,
    Haze, Horizon, Rays, RayRadius, Focus, Blur, Waves, Speed, Ripples, Refraction, Opacity,
    Reflection, Time, WorldHaze, Clouds, ParameterCount };
constexpr float defaults[ParameterCount] = {.50f, .55f, .58f, .32f, .38f, .72f, .45f,
    .32f, .45f, .20f, .65f, .35f, .50f, .32f, .45f, .20f, .65f, .35f,
    .50f, .42f, .18f, .35f};
float values[ParameterCount];
double pageStart = 0;
IDirect3DTexture9 *logo = nullptr;
IDirect3DTexture9 *studioLogo = nullptr;
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
float TextWidth(const char *text, float size) {
    return ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0, text).x;
}
void CenterText(float x, float y, float w, const char *text, float size, ImU32 color) {
    Text(x + std::floor((w - TextWidth(text, size)) * .5f), y, text, size, color);
}
void Rect(float x, float y, float w, float h, ImU32 color, float rounding = 6) {
    draw->AddRectFilled(P(x, y), P(x + w, y + h), color, studio ? std::min(rounding, 2.f) : rounding);
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
// Small icons are strokes, never font glyphs: identical shapes in EN and RU.
void Chevron(float x, float y, int direction, ImU32 color = Muted) {
    if (direction == 0) {
        draw->AddLine(P(x - 3, y - 1.5f), P(x, y + 1.5f), color, 1.2f);
        draw->AddLine(P(x, y + 1.5f), P(x + 3, y - 1.5f), color, 1.2f);
    } else {
        draw->AddLine(P(x - direction * 1.5f, y - 3), P(x + direction * 1.5f, y), color, 1.2f);
        draw->AddLine(P(x + direction * 1.5f, y), P(x - direction * 1.5f, y + 3), color, 1.2f);
    }
}
void ResetIcon(float x, float y, ImU32 color) {
    draw->PathArcTo(P(x, y), 4.5f, -2.35f, 2.65f, 18);
    draw->PathStroke(color, 1.2f);
    draw->AddLine(P(x - 3.2f, y - 3.2f), P(x - 3.2f, y - 6), color, 1.2f);
    draw->AddLine(P(x - 3.2f, y - 3.2f), P(x, y - 3.2f), color, 1.2f);
}
bool IconButton(const char *id, float x, float y, int icon, UiText tip, bool emphasized = true) {
    bool pressed = Hit(id, x, y, 24, 24);
    bool hover = ImGui::IsItemHovered();
    if (hover) Rect(x, y, 24, 24, vivid ? IM_COL32(62, 43, 91, 255) : IM_COL32(48, 50, 56, 255), 3);
    ImU32 color = hover ? (vivid ? Lavender : Ink) : emphasized ? Muted : IM_COL32(85, 88, 98, 255);
    if (icon == 2) ResetIcon(x + 12, y + 12, color);
    else Chevron(x + 12, y + 12, icon, color);
    Tip(tip);
    return pressed;
}
void Switch(const char *id, float x, float y, bool &value) {
    ImGui::PushID(id);
    if (Hit("switch", x - 4, y - 6, 42, 30) ||
        (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter))) value = !value;
    const float t = Animate("position", value ? 1.f : 0.f);
    draw->AddRectFilled(P(x, y), P(x + 28, y + 15),
        Mix(IM_COL32(56, 58, 64, 255), vivid ? Purple : IM_COL32(86, 89, 98, 255), t), 8);
    draw->AddCircleFilled(P(x + 7.5f + 13 * t, y + 7.5f), 4.5f,
        Mix(Muted, IM_COL32(224, 225, 231, 255), t), 20);
    Tip(value ? UiText::V2On : UiText::V2Off);
    ImGui::PopID();
}

void Slider(float y, UiText label, Parameter parameter, bool clock = false) {
    ImGui::PushID(parameter);
    float &value = values[parameter];
    Text(42, y + 6, label, 13, Muted);
    Hit("track", 201, y, 205, 26);
    bool hover = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    if (active && ImGui::IsMouseDown(0))
        value = std::clamp((ImGui::GetIO().MousePos.x - P(207, 0).x) / 192, 0.f, 1.f);
    float step = clock ? 1.f / 1440 : .01f;
    if (ImGui::IsItemFocused()) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) value = std::max(0.f, value - step);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) value = std::min(1.f, value + step);
    }
    float t = Animate("hover", hover || active ? 1.f : 0.f);
    draw->AddLine(P(207, y + 13), P(399, y + 13), IM_COL32(69, 72, 80, 255), 1.5f);
    float neutral = 207 + 192 * defaults[parameter];
    float knob = std::floor(207 + 192 * value);
    draw->AddLine(P(neutral, y + 10), P(neutral, y + 16), IM_COL32(93, 96, 106, 255), 1);
    if (vivid) {
        Rect(207, y + (studio ? 12 : 11), std::max(0.f, knob - 207), studio ? 2 : 4,
            Mix(Purple, Lavender, t * .45f), 1);
        const float radius = studio ? 3 : 4;
        draw->AddCircleFilled(P(knob, y + 13), radius,
            studio || active ? Lavender : IM_COL32(220, 199, 255, 255), 20);
        draw->AddCircle(P(knob, y + 13), radius, Purple, 20, 1);
    } else {
        draw->AddLine(P(207, y + 13), P(knob, y + 13), Mix(IM_COL32(130, 133, 143, 255), Violet, t), 2);
        Rect(knob - 2, y + 8, 4, 10, active ? Violet : Ink, 1);
    }

    ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
    if (IconButton("less", 414, y + 1, -1, clock ? UiText::TimeInputHelp : UiText::NumberInputHelp))
        value = std::max(0.f, value - step);
    ImGui::PopItemFlag();
    char number[24];
    if (clock) {
        int minutes = std::min(1439, int(value * 1440));
        std::snprintf(number, sizeof(number), "%02d:%02d", minutes / 60, minutes % 60);
    } else std::snprintf(number, sizeof(number), "%.2f", value);
    ImGui::SetCursorScreenPos(P(438, y + 2));
    ImGui::SetNextItemWidth(48);
    ImGui::PushFont(nullptr, 12);
    float pad = std::max(2.f, (48 - TextWidth(number, 12)) * .5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {pad, 4});
    if (clock) {
        if (ImGui::InputText("##value", number, sizeof(number), ImGuiInputTextFlags_AutoSelectAll)) {
            int hours, minutes; char tail;
            if (std::sscanf(number, "%d:%d%c", &hours, &minutes, &tail) == 2 &&
                hours >= 0 && hours < 24 && minutes >= 0 && minutes < 60)
                value = (hours * 60.f + minutes) / 1440;
        }
    } else if (ImGui::InputFloat("##value", &value, 0, 0, "%.2f", ImGuiInputTextFlags_AutoSelectAll))
        value = std::isfinite(value) ? std::clamp(value, 0.f, 1.f) : defaults[parameter];
    ImGui::PopStyleVar();
    ImGui::PopFont();
    Tip(clock ? UiText::TimeInputHelp : UiText::NumberInputHelp);
    ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
    if (IconButton("more", 486, y + 1, 1, clock ? UiText::TimeInputHelp : UiText::NumberInputHelp))
        value = std::min(1.f, value + step);
    ImGui::PopItemFlag();
    if (IconButton("reset-value", 516, y + 1, 2, UiText::ResetValue,
        std::abs(value - defaults[parameter]) > .001f)) value = defaults[parameter];
    ImGui::PopID();
}
bool Section(const char *id, float y, UiText title, int toggle = -1, bool *reset = nullptr) {
    ImGui::PushID(id);
    auto *state = ImGui::GetStateStorage();
    auto key = ImGui::GetID("expanded");
    bool expanded = state->GetBool(key, true);
    Rect(16, y, 528, 30, IM_COL32(36, 38, 43, 255), 4);
    if (Hit("heading", 16, y, reset ? 452 : 528, 30) && reset) {
        expanded = !expanded;
        state->SetBool(key, expanded);
    }
    if (ImGui::IsItemHovered() && reset) Rect(16, y, 452, 30, vivid ? IM_COL32(48, 39, 64, 255) : IM_COL32(42, 44, 50, 255), 4);
    if (reset) Chevron(28, y + 15, expanded ? 0 : 1);
    Text(reset ? 42 : 28, y + 8, title, 13);
    if (reset) *reset = IconButton("reset-section", 470, y + 3, 2, UiText::ResetSection);
    if (toggle >= 0) Switch("enable", 504, y + 7, switches[toggle]);
    ImGui::PopID();
    return expanded;
}
struct Row { UiText label; Parameter parameter; bool clock = false; };
void Group(const char *id, float &y, UiText title, int toggle, std::initializer_list<Row> rows) {
    bool reset = false;
    bool expanded = Section(id, y, title, toggle, &reset);
    if (reset) {
        for (const auto &row : rows) values[row.parameter] = defaults[row.parameter];
        if (toggle >= 0) switches[toggle] = true;
    }
    y += 34;
    if (expanded) {
        const float firstRow = y;
        ImGui::BeginDisabled(toggle >= 0 && !switches[toggle]);
        for (const auto &row : rows) {
            Slider(y, row.label, row.parameter, row.clock);
            y += 28;
        }
        ImGui::EndDisabled();
        if (toggle >= 0 && !switches[toggle])
            Rect(17, firstRow, 526, y - firstRow, IM_COL32(29, 30, 34, 140), 0);
    }
    y += 10;
}
void ChangePage(int next, int section = 0) {
    page = next; subpage = section; pageStart = ImGui::GetTime();
}
void NavRow(const char *id, float y, UiText title, int target) {
    bool clicked = Hit(id, 16, y, 528, 29);
    if (ImGui::IsItemHovered()) Rect(16, y, 528, 29, IM_COL32(38, 39, 44, 255), 3);
    Text(28, y + 7, title, 13);
    Chevron(528, y + 15, 1, ImGui::IsItemHovered() ? Ink : Muted);
    draw->AddLine(P(16, y + 29), P(544, y + 29), Line);
    if (clicked) ChangePage(0, target);
}
void ResetDemo() {
    std::copy(std::begin(defaults), std::end(defaults), values);
    std::fill(std::begin(switches), std::end(switches), true);
    profile = weather = 0; aaMethod = 1;
}
void Effects() {
    if (subpage != 0) {
        if (Hit("back", 16, 91, 94, 28)) ChangePage(0);
        if (ImGui::IsItemHovered()) Rect(16, 91, 94, 28, IM_COL32(40, 42, 48, 255), 4);
        Chevron(28, 105, -1);
        Text(42, 98, UiText::V2Back, 12, Muted);
        const UiText heading = subpage == 1 ? UiText::LightAtmosphere : UiText::V2Water;
        Text(128, 98, heading, 13);
        float y = 132;
        if (subpage == 1) {
            Group("sky", y, UiText::V2Sky, SkyOn, {{UiText::V2Fog, Haze}, {UiText::Horizon, Horizon}});
            Group("rays", y, UiText::V2Rays, RaysOn, {{UiText::V2Intensity, Rays}, {UiText::V2Radius, RayRadius}});
            Group("depth", y, UiText::DepthOfField, DepthOn, {{UiText::Focus, Focus}, {UiText::Blur, Blur}});
        } else {
            Group("water", y, UiText::Surface, WaterOn, {{UiText::WaveHeight, Waves}, {UiText::Speed, Speed}, {UiText::Ripples, Ripples}});
            Group("depth", y, UiText::DepthReflections, ReflectionOn, {{UiText::Refraction, Refraction}, {UiText::Opacity, Opacity}, {UiText::SkyReflection, Reflection}});
        }
        return;
    }
    NavRow("atmosphere", 91, UiText::LightAtmosphere, 1);
    NavRow("water", 123, UiText::V2Water, 2);
    float y = 168;
    Group("grade", y, UiText::ColorCorrection, GradeOn,
        {{UiText::Exposure, Exposure}, {UiText::Contrast, Contrast}, {UiText::Saturation, Saturation}, {UiText::Temperature, Temperature}});
    Group("bloom", y, UiText::V2Bloom, BloomOn,
        {{UiText::V2Intensity, Bloom}, {UiText::V2Threshold, Threshold}, {UiText::V2Radius, Radius}});
    draw->AddLine(P(16, y), P(544, y), Line);
    Text(28, y + 11, UiText::V2AA, 12, Muted);
    const char *methods[] = {"FXAA", "SMAA"};
    if (Hit("aa-method", 366, y + 5, 124, 24)) ImGui::OpenPopup("aa-methods");
    Rect(366, y + 5, 124, 24, ImGui::IsItemHovered() ? IM_COL32(46, 48, 54, 255) : IM_COL32(37, 39, 44, 255), 3);
    Text(378, y + 11, methods[aaMethod], 12);
    Chevron(477, y + 17, 0);
    ImGui::SetNextWindowPos(P(366, y + 32));
    ImGui::SetNextWindowSize({124, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {6, 6});
    if (ImGui::BeginPopup("aa-methods")) {
        for (int i = 0; i < 2; ++i) if (ImGui::Selectable(methods[i], aaMethod == i)) aaMethod = i;
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar();
    Switch("aa", 504, y + 9, switches[AaOn]);
}
void World() {
    float y = 100;
    Group("time", y, UiText::V2Time, TimeOn, {{UiText::V2Time, Time, true}});
    bool reset = false;
    bool expanded = Section("weather", y, UiText::V2Weather, WeatherOn, &reset);
    if (reset) { weather = 0; switches[WeatherOn] = true; }
    y += 40;
    const UiText labels[] = {UiText::V2Clear, UiText::V2Rain, UiText::V2Snow};
    if (expanded) for (int i = 0; i < 3; ++i) {
        ImGui::PushID(i);
        float x = 28 + i * 168.f;
        ImGui::BeginDisabled(!switches[WeatherOn]);
        if (Hit("weather", x, y, 152, 28)) weather = i;
        const ImU32 selectedWeather = studio ? IM_COL32(67, 53, 83, 255) : vivid ? IM_COL32(81, 49, 125, 255) : IM_COL32(55, 57, 64, 255);
        Rect(x, y, 152, 28, weather == i ? selectedWeather : IM_COL32(37, 38, 43, 255), 3);
        CenterText(x, y + 8, 152, Tr(labels[i]), 12, weather == i ? (vivid ? Lavender : Ink) : Muted);
        ImGui::EndDisabled();
        ImGui::PopID();
    }
    y += expanded ? 52 : 0;
    Group("sky", y, UiText::V2Sky, WorldSkyOn, {{UiText::V2Fog, WorldHaze}, {UiText::Clouds, Clouds}});
}
void Profiles() {
    Text(20, 105, UiText::V2Profiles, 16);
    const UiText titles[] = {UiText::V2Natural, UiText::V2Golden, UiText::V2Moonlit};
    const UiText notes[] = {UiText::V2NaturalNote, UiText::V2GoldenNote, UiText::V2MoonlitNote};
    for (int i = 0; i < 3; ++i) {
        ImGui::PushID(i);
        float y = 146 + i * (studio ? 64.f : 91.f);
        const float h = studio ? 52 : 76;
        if (Hit("profile", 16, y, 528, h)) profile = i;
        Rect(16, y, 528, h, ImGui::IsItemHovered() ? IM_COL32(40, 41, 47, 255) : Surface, 4);
        draw->AddRect(P(16, y), P(544, y + h), profile == i ? (vivid ? Violet : IM_COL32(90, 91, 104, 255)) : Line, studio ? 2 : 4);
        Text(30, y + 16, titles[i], 14);
        if (!studio) Text(30, y + 42, notes[i], 12, Muted);
        draw->AddCircle(P(515, y + h / 2), 6, profile == i ? Violet : Muted, 24);
        if (profile == i) draw->AddCircleFilled(P(515, y + h / 2), 3, Violet, 16);
        ImGui::PopID();
    }
    if (!studio) Text(28, 452, UiText::V2ProfileHint, 12, Muted);
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
    if (!studio) Text(28, 257, UiText::V2MotionNote, 11, Muted);
    Switch("motion", 504, 233, motion);
    Rect(16, 307, 528, 179, IM_COL32(26, 27, 31, 255), 4);
    draw->AddRect(P(16, 307), P(544, 486), Line, studio ? 2 : 4);
    Text(32, 330, studio ? UiText::V2Cursor : UiText::V2CursorSample, 14);
    if (!studio) Text(32, 357, UiText::V2CursorSampleNote, 11, Muted);
    DrawPointer(draw, P(45, 409));
    Text(70, 410, "12 × 12 px", 11, Muted);
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
    if (!studio) Text(0, -23, vivid ? UiText::V2VioletStudy : UiText::V2CompactStudy, 10, Muted);
    for (int i = 10; i > 0; --i)
        draw->AddRect(P(-float(i), -float(i)), P(PanelWidth + i, PanelHeight + i), IM_COL32(0, 0, 0, 8), (studio ? 2.f : 5.f) + i);
    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize({PanelWidth, PanelHeight});
    ImGui::Begin("FENB-v2-compact", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    draw = ImGui::GetWindowDrawList();
    Rect(0, 0, PanelWidth, PanelHeight, IM_COL32(29, 30, 34, 255), 5);
    draw->AddRect(P(0, 0), P(PanelWidth, PanelHeight), Line, studio ? 2 : 5);
    if (studio && studioLogo) {
        const float h = 84.f * STUDIO_LOGO_HEIGHT / STUDIO_LOGO_WIDTH;
        draw->AddImage(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(studioLogo))),
            P(14, 22 - h / 2), P(98, 22 + h / 2));
    } else if (logo) {
        float h = 28.f * UI_LOGO_HEIGHT / UI_LOGO_WIDTH;
        draw->AddImage(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(logo))), P(11, 22 - h / 2), P(39, 22 + h / 2));
    }
    if (!studio) Text(47, 15, "FENB", 13, Ink);
    const UiText tabs[] = {UiText::Image, UiText::WorldTab};
    float tabX = studio ? 113 : 103;
    for (int i = 0; i < 2; ++i) {
        ImGui::PushID(i);
        const float textWidth = std::ceil(TextWidth(Tr(tabs[i]), 13));
        const float width = textWidth + 20;
        if (Hit("tab", tabX, 7, width, 32)) ChangePage(i);
        bool selected = page == i;
        const float hover = Animate("hover", ImGui::IsItemHovered() ? 1.f : 0.f);
        const float selection = Animate("selected", selected ? 1.f : 0.f);
        ImU32 tabColor = Mix(IM_COL32(29, 30, 34, 255), IM_COL32(40, 42, 48, 255), hover);
        if (vivid && !studio) tabColor = Mix(tabColor, IM_COL32(56, 37, 80, 255), selection);
        Rect(tabX, 7, width, 32, tabColor, 4);
        Text(tabX + 10, 15, tabs[i], 13, selected ? (vivid ? Lavender : Ink) : Muted);
        if (selected) Rect(tabX + 10, 37, textWidth, 2, Violet, 1);
        tabX += width + 8;
        ImGui::PopID();
    }
    const char *languages[] = {Tr(UiText::V2English), Tr(UiText::V2Russian)};
    for (int i = 0; i < 2; ++i) {
        ImGui::PushID(i + 10);
        if (Hit("language", 331 + i * 31.f, 8, 28, 27)) language = i ? Language::Russian : Language::English;
        Text(336 + i * 31.f, 16, languages[i], 11, int(language == Language::Russian) == i ? Ink : Muted);
        ImGui::PopID();
    }
    Switch("master", 408, 15, switches[MasterOn]);
    Text(442, 16, UiText::Effects, 11, Muted);
    if (Hit("appearance", 500, 8, 25, 27)) ChangePage(3);
    for (int dot = 0; dot < 3; ++dot)
        draw->AddCircleFilled(P(506 + dot * 5.f, 22), 1, page == 3 ? Ink : Muted, 8);
    Tip(UiText::V2Appearance);
    if (Hit("close", 529, 8, 23, 27)) running = false;
    if (ImGui::IsItemHovered()) Rect(529, 8, 23, 27, IM_COL32(46, 43, 48, 255), 4);
    draw->AddLine(P(537, 18), P(544, 25), Muted, 1.2f);
    draw->AddLine(P(544, 18), P(537, 25), Muted, 1.2f);
    Tip(UiText::V2CloseHelp);
    draw->AddLine(P(1, 44), P(559, 44), Line);
    Text(16, 62, UiText::Profile, 12, Muted);
    if (Hit("profiles", 89, 53, 314, 27)) ChangePage(2);
    Rect(89, 53, 314, 27, IM_COL32(39, 40, 45, 255), 3);
    const UiText profiles[] = {UiText::V2Natural, UiText::V2Golden, UiText::V2Moonlit};
    Text(100, 61, profiles[profile], 12);
    Chevron(387, 66, 0);
    if (Hit("reset", 421, 53, 123, 27)) ResetDemo();
    if (ImGui::IsItemHovered()) Rect(421, 53, 123, 27, IM_COL32(40, 41, 46, 255), 3);
    ResetIcon(432, 66, Muted);
    Text(447, 61, UiText::ResetAll, 11, Muted);
    Tip(UiText::V2Reset);
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
    if (!studio) {
        Text(66, 520, UiText::V2Sandbox, 10, Muted);
        Text(510, 520, "UI v2", 10, Muted);
    }
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
    wchar_t exePath[32768]{};
    GetModuleFileNameW(nullptr, exePath, 32768);
    const std::wstring executable = std::filesystem::path(exePath).stem().wstring();
    studio = executable.find(L"-studio") != std::wstring::npos || std::string(command).find("--studio") != std::string::npos;
    vivid = studio || executable.find(L"-violet") != std::wstring::npos || std::string(command).find("--violet") != std::string::npos;
    if (studio) {
        Purple = IM_COL32(144, 116, 177, 255);
        Lavender = IM_COL32(190, 171, 213, 255);
    }
    if (vivid) Violet = Purple;
    ResetDemo();
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
    Com<IDirect3DTexture9> horizontalLogo;
    if (studio) {
        if (FAILED(device->CreateTexture(STUDIO_LOGO_WIDTH, STUDIO_LOGO_HEIGHT, 1, 0, D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED, horizontalLogo.out(), nullptr))) return 5;
        if (FAILED(horizontalLogo->LockRect(0, &locked, nullptr, 0))) return 6;
        for (unsigned y = 0; y < STUDIO_LOGO_HEIGHT; ++y)
            memcpy(static_cast<BYTE *>(locked.pBits) + y * locked.Pitch,
                STUDIO_LOGO_BGRA + y * STUDIO_LOGO_WIDTH, STUDIO_LOGO_WIDTH * 4);
        horizontalLogo->UnlockRect(0);
        studioLogo = horizontalLogo;
    }
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
    theme.WindowRounding = studio ? 2 : 8;
    theme.PopupRounding = studio ? 2 : 0;
    theme.FrameRounding = studio ? 2 : 3;
    theme.Colors[ImGuiCol_PopupBg] = ImGui::ColorConvertU32ToFloat4(Surface);
    theme.Colors[ImGuiCol_Text] = ImGui::ColorConvertU32ToFloat4(Ink);
    theme.Colors[ImGuiCol_NavCursor] = ImGui::ColorConvertU32ToFloat4(Violet);
    theme.Colors[ImGuiCol_FrameBg] = ImColor(37, 39, 44);
    theme.Colors[ImGuiCol_FrameBgHovered] = ImColor(46, 48, 55);
    theme.Colors[ImGuiCol_FrameBgActive] = ImColor(49, 49, 60);
    theme.Colors[ImGuiCol_TextSelectedBg] = ImColor(105, 85, 135, 140);
    theme.Colors[ImGuiCol_Header] = ImColor(48, 50, 57);
    theme.Colors[ImGuiCol_HeaderHovered] = ImColor(58, 59, 68);
    theme.Colors[ImGuiCol_HeaderActive] = ImColor(63, 61, 73);
    if (vivid) {
        theme.Colors[ImGuiCol_FrameBgActive] = ImColor(64, 43, 89);
        theme.Colors[ImGuiCol_TextSelectedBg] = ImColor(143, 83, 239, 150);
        theme.Colors[ImGuiCol_Header] = ImColor(72, 43, 109);
        theme.Colors[ImGuiCol_HeaderHovered] = ImColor(91, 49, 145);
        theme.Colors[ImGuiCol_HeaderActive] = ImColor(111, 57, 183);
    }
    if (studio) {
        theme.Colors[ImGuiCol_FrameBgActive] = ImColor(58, 48, 73);
        theme.Colors[ImGuiCol_TextSelectedBg] = ImColor(144, 116, 177, 150);
        theme.Colors[ImGuiCol_Header] = ImColor(58, 47, 73);
        theme.Colors[ImGuiCol_HeaderHovered] = ImColor(73, 58, 91);
        theme.Colors[ImGuiCol_HeaderActive] = ImColor(91, 73, 111);
    }
    if (!ImGui_ImplWin32_Init(window) || !ImGui_ImplDX9_Init(device)) return 7;
    Gdiplus::GdiplusStartupInput startup;
    ULONG_PTR gdiplus = 0;
    if (Gdiplus::GdiplusStartup(&gdiplus, &startup, nullptr) != Gdiplus::Ok) return 8;
    auto output = std::filesystem::path(exePath).parent_path();
    if (vivid) output /= studio ? L"studio" : L"violet";
    if (capture) std::filesystem::create_directories(output);
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
            int view = (frame / 4) % 8;
            page = view < 4 ? view : view == 6 ? 1 : 0;
            subpage = view == 4 || view == 5 ? view - 3 : 0;
            language = frame < 32 ? Language::English : Language::Russian;
        }
        ImGui_ImplDX9_NewFrame();
        ImGui_ImplWin32_NewFrame();
        if (capture) {
            // Render actual hovered widget states, including the compact cursor.
            const int view = (frame / 4) % 8;
            const float hoverX = (Width - PanelWidth) / 2 + (studio ? 135 : 125);
            const float hoverY = (Height - PanelHeight) / 2 + 22;
            io.AddMousePosEvent(view >= 6 ? hoverX : -FLT_MAX, view >= 6 ? hoverY : -FLT_MAX);
        }
        ImGui::NewFrame();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) running = false;
        Panel();
        const char *utf8Title = Tr(studio ? UiText::V2StudioStudy : vivid ? UiText::V2VioletStudy : UiText::V2Window);
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
                    std::to_wstring((frame / 4) % 8) + L".png";
                if (!Capture(device, output / name)) { result = 9; break; }
            }
        }
        device->Present(nullptr, nullptr, nullptr, nullptr);
        if (capture && ++frame == 64) break;
    }
    Gdiplus::GdiplusShutdown(gdiplus);
    ImGui_ImplDX9_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    DestroyWindow(window);
    UnregisterClassW(wc.lpszClassName, instance);
    return result;
}
