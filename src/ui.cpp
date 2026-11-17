#include "imgui.h"
#include "runtime.hpp"
#include "actor_visibility.hpp"
#include "version.hpp"
#include "ui_branding.hpp"
#include "ui_fonts.hpp"
#include "generated/ui_logo.hpp"
#include "generated/ui_cursor.hpp"
#include <cstdio>

namespace {
thread_local Language uiLanguage = Language::English;
thread_local bool uiMotion = true;
thread_local bool uiParameterDisabled = false;
thread_local ImFont *uiEmphasis = nullptr;
const char *Tr(UiText text) {
    return Translate(uiLanguage, text);
}
std::string Label(UiText text) {
    return std::string(Tr(text)) + "###text-" + std::to_string(static_cast<int>(text));
}
constexpr ImU32 Background = IM_COL32(29, 30, 33, 255), Raised = IM_COL32(35, 36, 40, 255);
constexpr ImU32 Line = IM_COL32(54, 56, 61, 255), Text = IM_COL32(221, 223, 227, 255);
constexpr ImU32 Muted = IM_COL32(143, 147, 155, 255), Accent = IM_COL32(171, 157, 204, 255);
constexpr ImU32 Purple = IM_COL32(144, 116, 177, 255), Lavender = IM_COL32(190, 171, 213, 255);

ImU32 Opacity(ImU32 color) {
    auto a = static_cast<ImU32>((color >> 24) * ImGui::GetStyle().Alpha);
    return (color & 0x00ffffff) | (a << 24);
}
void TextAt(ImDrawList *draw, ImVec2 p, const char *text, ImU32 color = Text, float size = 15,
            ImFont *font = nullptr) {
    draw->AddText(font ? font : ImGui::GetFont(), size, {std::floor(p.x), std::floor(p.y)}, Opacity(color), text);
}
float Animate(const char *id, float target) {
    auto *state = ImGui::GetStateStorage();
    auto key = ImGui::GetID(id);
    float v = state->GetFloat(key, target);
    v = uiMotion ? v + (target - v) * (1 - std::exp(-ImGui::GetIO().DeltaTime * 18)) : target;
    state->SetFloat(key, v);
    return v;
}
ImU32 Blend(ImU32 a, ImU32 b, float t) {
    auto x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
    return ImGui::ColorConvertFloat4ToU32({x.x+(y.x-x.x)*t, x.y+(y.y-x.y)*t,
        x.z+(y.z-x.z)*t, x.w+(y.w-x.w)*t});
}
bool BeginCombo(const char *id, const char *preview, ImGuiComboFlags flags = 0) {
    auto p = ImGui::GetCursorScreenPos();
    float w = ImGui::CalcItemWidth(), h = ImGui::GetFrameHeight();
    auto *d = ImGui::GetWindowDrawList();
    auto *state = ImGui::GetStateStorage();
    auto key = ImGui::GetID(id);
    bool open = ImGui::BeginCombo(id, preview, flags | ImGuiComboFlags_NoArrowButton);
    float v = state->GetFloat(key, open ? 1.f : 0.f), target = open ? 1.f : 0.f;
    v = uiMotion ? v + (target-v)*(1-std::exp(-ImGui::GetIO().DeltaTime*18)) : target;
    state->SetFloat(key,v);
    float a = v * 3.14159265f, c = std::cos(a), sn = std::sin(a);
    auto point = [&](float x, float y) { return ImVec2(p.x+w-13+x*c-y*sn,p.y+h*.5f+x*sn+y*c); };
    d->AddLine(point(-3,-1.5f),point(0,1.5f),Opacity(Muted),1.2f);
    d->AddLine(point(0,1.5f),point(3,-1.5f),Opacity(Muted),1.2f);
    return open;
}
bool Combo(const char *id, int *selected, const char *const items[], int count) {
    bool changed = false;
    if (BeginCombo(id, items[*selected])) {
        for (int i=0;i<count;++i) {
            ImGui::PushID(i);
            if (ImGui::Selectable((std::string(items[i])+"###option").c_str(),*selected==i)) { *selected=i; changed=true; }
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    return changed;
}
void ResetAll(Settings &s) {
    auto controls = s;
    s = Settings{};
    s.preserveControls(controls);
}
float HeaderBrand(ImDrawList *d, ImVec2 origin, float panelWidth, const PanelBranding &branding) {
    auto layout = branding.layout;
    if (layout == PanelBranding::Layout::TextOnly || !branding.symbol ||
        (layout == PanelBranding::Layout::Horizontal && !branding.horizontal)) {
        TextAt(d, {origin.x + 16, origin.y + 12}, "FENB");
        return 68;
    }
    // Keep the language selector and actions usable in the existing narrow panel.
    if (panelWidth < 430)
        layout = PanelBranding::Layout::SymbolOnly;
    float width = layout == PanelBranding::Layout::Horizontal ? 100 : 30;
    float aspect = layout == PanelBranding::Layout::Horizontal ? branding.horizontalAspect
                                                              : branding.symbolAspect;
    auto *texture = layout == PanelBranding::Layout::Horizontal ? branding.horizontal : branding.symbol;
    if (!std::isfinite(aspect) || aspect <= 0)
        return 68;
    float height = std::min(width / aspect, 30.f);
    width = height * aspect;
    const ImVec2 p{origin.x + 12, origin.y + std::floor((44 - height) / 2) + 2};
    d->AddImage(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texture))), p,
                {p.x + width, p.y + height}, {0, 0}, {1, 1}, Opacity(IM_COL32_WHITE));
    if (layout == PanelBranding::Layout::SymbolText) {
        TextAt(d, {origin.x + 49, origin.y + 12}, "FENB");
        return 98;
    }
    return layout == PanelBranding::Layout::Horizontal ? 126.f : 51.f;
}
void Theme() {
    auto &s = ImGui::GetStyle();
    for (auto &c : s.Colors)
        c = ImVec4(.23f, .24f, .27f, 1);
    s.WindowPadding = {0, 0};
    s.WindowRounding = 2;
    s.WindowBorderSize = 1;
    s.ChildRounding = 0;
    s.ChildBorderSize = 0;
    s.FrameRounding = 2;
    s.FramePadding = {5, 3};
    s.FrameBorderSize = 0;
    s.ItemSpacing = {7, 0};
    s.ItemInnerSpacing = {5, 4};
    s.ScrollbarSize = 5;
    s.ScrollbarRounding = 2;
    s.GrabRounding = 1;
    s.PopupRounding = 2;
    s.PopupBorderSize = 1;
    s.Colors[ImGuiCol_Text] = {.87f, .88f, .90f, 1};
    s.Colors[ImGuiCol_TextDisabled] = {.48f, .50f, .54f, 1};
    s.Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(Background);
    s.Colors[ImGuiCol_ChildBg] = {0, 0, 0, 0};
    s.Colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(Line);
    s.Colors[ImGuiCol_FrameBg] = {.145f, .153f, .173f, 1};
    s.Colors[ImGuiCol_FrameBgHovered] = {.19f, .20f, .23f, 1};
    s.Colors[ImGuiCol_FrameBgActive] = {.23f, .24f, .28f, 1};
    s.Colors[ImGuiCol_Button] = {0, 0, 0, 0};
    s.Colors[ImGuiCol_ButtonHovered] = {.22f, .23f, .26f, 1};
    s.Colors[ImGuiCol_ButtonActive] = {.26f, .27f, .30f, 1};
    s.Colors[ImGuiCol_CheckMark] = {.79f, .80f, .83f, 1};
    s.Colors[ImGuiCol_SliderGrab] = {.65f, .66f, .69f, 1};
    s.Colors[ImGuiCol_SliderGrabActive] = ImGui::ColorConvertU32ToFloat4(Accent);
    s.Colors[ImGuiCol_ScrollbarBg] = {0, 0, 0, 0};
    s.Colors[ImGuiCol_ScrollbarGrab] = {.30f, .31f, .35f, 1};
    s.Colors[ImGuiCol_PopupBg] = {.13f, .14f, .16f, 1};
    s.Colors[ImGuiCol_Header] = {.227f, .184f, .286f, 1};
    s.Colors[ImGuiCol_HeaderHovered] = {.286f, .227f, .357f, 1};
    s.Colors[ImGuiCol_HeaderActive] = {.32f, .33f, .37f, 1};
    s.Colors[ImGuiCol_TextSelectedBg] = {.40f, .37f, .49f, .65f};
    s.Colors[ImGuiCol_NavCursor] = ImGui::ColorConvertU32ToFloat4(Accent);
    s.Colors[ImGuiCol_ModalWindowDimBg] = {.03f, .03f, .04f, .45f};
}
void Tip(const char *text, ImGuiHoveredFlags flags = ImGuiHoveredFlags_None) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | flags)) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(300);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
bool SquareSwitch(const char *id, bool &value, ImVec2 size = {28, 18}) {
    auto p = ImGui::GetCursorScreenPos();
    bool changed = ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
    if (changed)
        value = !value;
    auto *d = ImGui::GetWindowDrawList();
    ImGui::PushID(id);
    const float t = Animate("position", value ? 1.f : 0.f);
    ImGui::PopID();
    ImVec2 center{std::floor(p.x+size.x*.5f),std::floor(p.y+size.y*.5f)};
    d->AddRectFilled({center.x-14,center.y-7.5f},{center.x+14,center.y+7.5f},
        Opacity(Blend(IM_COL32(56,58,64,255),Purple,t)),8);
    d->AddCircleFilled({center.x-6.5f+13*t,center.y},4.5f,
        Opacity(Blend(Muted,IM_COL32(224,225,231,255),t)),20);
    return changed;
}
float FooterSwitch(const char *id, UiText label, bool &value, bool supported, UiText help,
                   float right, float top) {
    constexpr float fontSize=11, gap=6, switchWidth=28;
    const char *text=Tr(label);
    const float width=ImGui::GetFont()->CalcTextSizeA(fontSize,FLT_MAX,0,text).x;
    const float left=right-switchWidth-gap-width;
    const auto origin=ImGui::GetWindowPos();
    TextAt(ImGui::GetWindowDrawList(),{origin.x+left,origin.y+top+3},text,Muted,fontSize);
    ImGui::SetCursorPos({right-switchWidth,top});
    ImGui::BeginDisabled(!supported && !value);
    SquareSwitch(id,value);
    Tip(Tr(supported?help:UiText::UnsupportedClient),ImGuiHoveredFlags_AllowWhenDisabled);
    ImGui::EndDisabled();
    return left;
}
void ResetGlyph(ImDrawList *d, ImVec2 p, ImU32 color, ImVec2 size = {20, 18}) {
    ImVec2 c{p.x + size.x*.5f, p.y + size.y*.5f};
    d->PathArcTo(c, 5, -2.35f, 2.65f, 24);
    d->PathStroke(color, 1.35f);
    // Counter-clockwise arrow; the tip meets the arc at the upper left.
    const float a = -2.35f;
    ImVec2 start{c.x + 5 * std::cos(a), c.y + 5 * std::sin(a)};
    ImVec2 t{std::sin(a), -std::cos(a)};
    d->AddTriangleFilled({start.x + t.x * 2, start.y + t.y * 2},
                         {start.x - t.x * 2 - t.y * 2, start.y - t.y * 2 + t.x * 2},
                         {start.x - t.x * 2 + t.y * 2, start.y - t.y * 2 - t.x * 2}, color);
}
float Snap(float value, float low, float high, float neutral, float step) {
    value = std::clamp(std::round(value / step) * step, low, high);
    return std::abs(value - neutral) < step * .01f ? neutral : value;
}
void Row(UiText label, const char *help, float &value, float low, float high, float neutral,
         const char *format = "%.2f", float step = .01f, bool clock = false, const char *stableId = nullptr,
         const char *maximumLabel = nullptr) {
    ImGui::BeginDisabled(uiParameterDisabled);
    if (stableId) ImGui::PushID(stableId); else ImGui::PushID(&value);
    const auto p = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    auto *draw = ImGui::GetWindowDrawList();
    const float numberX = width - 98, trackLeft = p.x + 8, trackRight = p.x + width - 12;
    ImGui::InvisibleButton("label", {std::max(20.f, numberX - 18), 23});
    Tip(help);
    draw->PushClipRect({p.x + 4, p.y}, {p.x + numberX - 17, p.y + 23}, true);
    TextAt(draw, {p.x + 4, p.y + 5}, Tr(label), Text, 13);
    draw->PopClipRect();
    auto arrow = [&](const char *id, float x, bool right) {
        ImGui::SetCursorScreenPos({p.x + x, p.y + 2});
        ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
        const bool hit = ImGui::InvisibleButton(id, {14, 18}, ImGuiButtonFlags_EnableNav);
        ImGui::PopItemFlag();
        const auto box = ImGui::GetItemRectMin();
        if (ImGui::IsItemHovered() || ImGui::IsItemActive())
            draw->AddRectFilled(box, {box.x + 14, box.y + 18}, Opacity(Raised), 2);
        const float direction = right ? 1.f : -1.f, x0 = box.x + 7, y0 = box.y + 9;
        const auto color = Opacity(ImGui::IsItemHovered() ? Text : Muted);
        draw->AddLine({x0 - direction * 1.5f, y0 - 3}, {x0 + direction * 1.5f, y0}, color);
        draw->AddLine({x0 + direction * 1.5f, y0}, {x0 - direction * 1.5f, y0 + 3}, color);
        return hit;
    };
    if (arrow("less", numberX - 16, false)) value = Snap(value - step, low, high, neutral, step);
    ImGui::SetCursorScreenPos({p.x + numberX, p.y + 1});
    ImGui::SetNextItemWidth(60);
    ImGui::PushFont(nullptr, 13);
    if (clock) {
        char text[16]; const int minutes = std::clamp(int(value), 0, 1439);
        std::snprintf(text, sizeof(text), "%02d:%02d", minutes / 60, minutes % 60);
        if (ImGui::InputText("##clock", text, sizeof(text), ImGuiInputTextFlags_AutoSelectAll)) {
            int hours, mins; char tail;
            if (std::sscanf(text, "%d:%d%c", &hours, &mins, &tail) == 2 && hours >= 0 && hours < 24 && mins >= 0 && mins < 60)
                value = float(hours * 60 + mins);
        }
    } else if (ImGui::InputFloat("##value", &value, 0, 0, maximumLabel&&value>=high?maximumLabel:format, ImGuiInputTextFlags_AutoSelectAll)) {
        value = std::isfinite(value) ? std::clamp(value, low, high) : neutral;
        if (std::abs(value - neutral) < step * .01f) value = neutral;
    }
    Tip(Tr(clock ? UiText::TimeInputHelp : UiText::NumberInputHelp));
    ImGui::PopFont();
    if (arrow("more", numberX + 61, true)) value = Snap(value + step, low, high, neutral, step);
    ImGui::SetCursorScreenPos({p.x + width - 21, p.y + 2});
    const bool modified = std::abs(value - neutral) > step * .01f;
    ImGui::BeginDisabled(!modified);
    if (ImGui::InvisibleButton("reset", {20, 18}, ImGuiButtonFlags_EnableNav)) value = neutral;
    if (modified) { ResetGlyph(draw, ImGui::GetItemRectMin(), Opacity(ImGui::IsItemHovered() ? Text : Muted)); Tip(Tr(UiText::ResetValue)); }
    ImGui::EndDisabled();
    ImGui::SetCursorScreenPos({p.x + 4, p.y + 21});
    ImGui::InvisibleButton("track", {width - 12, 20}, ImGuiButtonFlags_EnableNav);
    const bool hover = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
    auto *state = ImGui::GetStateStorage(); const auto key = ImGui::GetID("resetGesture");
    bool suppress = state->GetBool(key);
    if (!ImGui::IsMouseDown(0)) suppress = false;
    if (hover && ImGui::IsMouseDoubleClicked(0)) { value = neutral; suppress = true; }
    else if (held && ImGui::IsMouseDown(0) && !suppress)
        value = Snap(low + std::clamp((ImGui::GetIO().MousePos.x - trackLeft) / (trackRight - trackLeft), 0.f, 1.f) * (high - low), low, high, neutral, step);
    state->SetBool(key, suppress);
    if (ImGui::IsItemFocused() && !uiParameterDisabled) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) value = Snap(value - step, low, high, neutral, step);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) value = Snap(value + step, low, high, neutral, step);
        draw->AddRect({trackLeft - 4, p.y + 21}, {trackRight + 4, p.y + 41}, Opacity(Accent), 2);
    }
    const float y = std::floor(p.y + 31);
    const float zero = std::floor(trackLeft + (trackRight - trackLeft) * (neutral - low) / (high - low));
    const float knob = std::floor(trackLeft + (trackRight - trackLeft) * std::clamp((value - low) / (high - low), 0.f, 1.f));
    draw->AddLine({trackLeft, y}, {trackRight, y}, Opacity(IM_COL32(73, 75, 83, 255)), 2);
    draw->AddLine({zero, y - 3}, {zero, y + 3}, Opacity(Muted));
    draw->AddLine({trackLeft, y}, {knob, y}, Opacity(Purple), 2);
    draw->AddRectFilled({knob - 3, y - 5}, {knob + 3, y + 5}, Opacity(hover || held ? IM_COL32(242, 231, 255, 255) : Lavender), 1);
    Tip(help);
    ImGui::SetCursorScreenPos(p); ImGui::Dummy({width, 44});
    ImGui::PopID(); ImGui::EndDisabled();
}

int MatchingPreset(const Settings &s) {
    return s.matchingPreset();
}
std::wstring ProfileDirectory(const Runtime &r) {
    return r.configPath.substr(0, r.configPath.find_last_of(L"\\/") + 1) + L"FENB-presets\\";
}
std::string UTF8(const std::wstring &text) {
    int n = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), int(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(n, 0);
    if (n)
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), int(text.size()), result.data(), n, nullptr, nullptr);
    return result;
}
std::wstring ProfileName(const char *text) {
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, nullptr, 0);
    if (n < 2 || n > 65)
        return {};
    std::wstring name(n, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, name.data(), n);
    name.pop_back();
    if (name.find_first_of(L"<>:\"/\\|?*") != std::wstring::npos || name.back() == L'.' ||
        name.back() == L' ' || name.front() == L' ')
        return {};
    for (auto c : name)
        if (c < 32)
            return {};
    auto stem = name.substr(0, name.find(L'.'));
    for (auto &c : stem)
        if (c >= L'a' && c <= L'z')
            c -= 32;
    if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL" ||
        (stem.size() == 4 && (stem.substr(0, 3) == L"COM" || stem.substr(0, 3) == L"LPT") &&
         stem[3] >= L'0' && stem[3] <= L'9'))
        return {};
    return name;
}
void ProfilePicker(Runtime &r, float width) {
    auto &s = r.settings;
    const auto rowOrigin = ImGui::GetCursorScreenPos();
    const float actionSize = ImGui::GetFrameHeight();
    ImGui::SetCursorScreenPos({rowOrigin.x + 12, rowOrigin.y});
    ImGui::PushFont(uiEmphasis, 13);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Accent), "%s", Tr(UiText::Profile));
    const float labelRight = ImGui::GetItemRectMax().x;
    ImGui::PopFont();
    const float pickerX = labelRight + 12;
    ImGui::SetCursorScreenPos({pickerX, rowOrigin.y});
    ImGui::SetNextItemWidth(ImGui::GetWindowPos().x + width - 76 - pickerX);
    const auto &presets = BuiltInProfileNames;
    bool createRequested = false;
    int matching = MatchingPreset(s);
    auto saved = r.selectedProfileSettings;
    saved.preserveControls(s);
    std::string caption = r.selectedProfile + (saved == s ? "" : " *");
    int selectedBuiltIn = r.selectedBuiltIn >= 0 ? r.selectedBuiltIn : matching;
    if (r.selectedProfile.empty())
        caption = selectedBuiltIn < 0 ? Tr(UiText::Custom) :
            std::string(Tr(presets[selectedBuiltIn])) + (matching == selectedBuiltIn ? "" : " *");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.16f, .17f, .19f, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 9));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
    if (BeginCombo("##profile", caption.c_str(),
                          ImGuiComboFlags_HeightLarge)) {
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0, .5f));
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(.19f, .20f, .22f, 1));
        ImGui::SeparatorText(Tr(UiText::BuiltInProfiles));
        for (int i : BuiltInProfileOrder)
            if (ImGui::Selectable(Label(presets[i]).c_str(), matching == i && r.selectedProfile.empty(), 0,
                                  {0, 24})) {
                s.preset(i);
                r.selectedProfile.clear();
                r.selectedBuiltIn = i;
            }
        ImGui::SeparatorText(Tr(UiText::MyProfiles));
        WIN32_FIND_DATAW data{};
        auto dir = ProfileDirectory(r);
        HANDLE find = FindFirstFileW((dir + L"*.ini").c_str(), &data);
        bool any = false;
        if (find != INVALID_HANDLE_VALUE) {
            do {
                if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    continue;
                any = true;
                std::wstring name = data.cFileName;
                auto label = UTF8(name.substr(0, name.size() - 4));
                if (ImGui::Selectable(label.c_str(), false, 0, {0, 24})) {
                    Settings next;
                    if (next.load(dir + name)) {
                        next.preserveControls(s);
                        s = next;
                        r.selectedProfile = label;
                        r.selectedBuiltIn = -1;
                        r.selectedProfileSettings = s;
                        r.profileError = UiText::None;
                    } else
                        r.message = UiText::ProfileReadFailed;
                }
            } while (FindNextFileW(find, &data));
            FindClose(find);
        }
        if (!any) {
            ImGui::PushFont(nullptr, 13);
            ImGui::TextDisabled("%s", Tr(UiText::CreateProfileHelp));
            ImGui::PopFont();
        }
        ImGui::Separator();
        if (ImGui::Selectable(Label(UiText::CreateProfile).c_str(), false, 0, {0, 24}))
            createRequested = true;
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    Tip(Tr(UiText::ProfileFolderHelp));
    auto action = [&](const char *id, float x, bool reset) {
        ImGui::SetCursorScreenPos({ImGui::GetWindowPos().x+x, rowOrigin.y});
        bool pressed = ImGui::InvisibleButton(id, {actionSize, actionSize}, ImGuiButtonFlags_EnableNav);
        const auto p = ImGui::GetItemRectMin();
        auto *draw = ImGui::GetWindowDrawList();
        const bool hovered = ImGui::IsItemHovered();
        if (hovered || ImGui::IsItemActive())
            draw->AddRectFilled(p, {p.x+actionSize,p.y+actionSize},
                ImGui::GetColorU32(ImGui::IsItemActive()?ImGuiCol_ButtonActive:ImGuiCol_ButtonHovered),2);
        const auto color = hovered ? Text : Muted;
        if (reset)
            ResetGlyph(draw,p,color,{actionSize,actionSize});
        else {
            ImVec2 c{p.x+actionSize*.5f,p.y+actionSize*.5f};
            draw->AddLine({c.x-5,c.y},{c.x+5,c.y},color,1.35f);
            draw->AddLine({c.x,c.y-5},{c.x,c.y+5},color,1.35f);
        }
        return pressed;
    };
    bool resetAll = action("reset-all", width-68, true);
    Tip(Tr(UiText::ResetAll));
    if (resetAll) ResetAll(s);
    bool create = action("+###create-profile", width-36, false);
    Tip(Tr(r.selectedProfile.empty() ? UiText::Create : UiText::SaveAs));
    if (create || createRequested) {
        r.profileError = UiText::None;
        if (createRequested)
            r.profileName[0] = 0;
        else if (!r.selectedProfile.empty())
            std::snprintf(r.profileName, sizeof(r.profileName), "%s", r.selectedProfile.c_str());
        ImGui::OpenPopup("saveProfile");
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));
    ImGui::SetNextWindowSize({330, 0});
    if (ImGui::BeginPopup("saveProfile")) {
        ImGui::TextUnformatted(Tr(UiText::SaveProfile));
        ImGui::SetNextItemWidth(298);
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputTextWithHint("##name", Tr(UiText::ProfileNameHint), r.profileName,
                                              sizeof(r.profileName), ImGuiInputTextFlags_EnterReturnsTrue);
        auto name = ProfileName(r.profileName), path = ProfileDirectory(r) + name + L".ini";
        bool exists = !name.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
        ImGui::BeginDisabled(name.empty());
        if (ImGui::Button(Label(exists ? UiText::ReplaceProfile : UiText::Save).c_str(), {182, 26}) ||
            (!name.empty() && enter && !exists)) {
            CreateDirectoryW(ProfileDirectory(r).c_str(), nullptr);
            if (s.save(path)) {
                r.message = UiText::ProfileSaved;
                r.selectedProfile = UTF8(name);
                r.selectedBuiltIn = -1;
                r.selectedProfileSettings = s;
                ImGui::CloseCurrentPopup();
            } else
                r.profileError = UiText::FileWriteFailed;
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(Label(UiText::Cancel).c_str(), {108, 26}))
            ImGui::CloseCurrentPopup();
        if (r.profileError != UiText::None)
            ImGui::TextWrapped("%s", Tr(r.profileError));
        ImGui::PushFont(nullptr, 12);
        ImGui::TextDisabled("%s", Tr(UiText::ProfileSavedHelp));
        ImGui::PopFont();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}
#include "ui_panel.inc"

void UpdateDialog(Runtime &r, ImVec2 origin, ImVec2 panelSize) {
    const auto &update = r.updater.status();
    const bool readyNotice = update.state == UpdateState::Ready || update.state == UpdateState::RollbackReady;
    const auto notice = (readyNotice ? "ready-" : "available-") + update.version;
    if ((readyNotice || update.state == UpdateState::Available) && r.updateNotified != notice) {
        r.updateNotified = notice;
        r.updateDialog = true;
    }
    if (r.updateDialog) {
        ImGui::OpenPopup("###fenb-update");
        r.updateDialog = false;
    }
    const float width = std::min(416.f, panelSize.x - 24);
    const auto display = ImGui::GetIO().DisplaySize;
    ImVec2 center{origin.x + panelSize.x*.5f, origin.y + panelSize.y*.5f};
    center.x = std::clamp(center.x, width*.5f+8, display.x-width*.5f-8);
    if (origin.y < 0 || origin.y + panelSize.y > display.y) center.y = display.y*.5f;
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, {.5f,.5f});
    ImGui::SetNextWindowSize({width,0}, ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints({width,0}, {width,display.y-16});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20,18));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8,12));
    const auto title = std::string(Tr(UiText::Updates)) + "###fenb-update";
    if (ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove)) {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX()+width-40);
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Lavender), "%s", Tr(UiText::Updates));
        ImGui::Separator();
        const bool ready = update.state == UpdateState::Ready || update.state == UpdateState::RollbackReady;
        UiText message = UiText::UpdateIdle;
        switch (update.state) {
        case UpdateState::Checking: message = UiText::UpdateChecking; break;
        case UpdateState::Available: message = UiText::UpdateAvailable; break;
        case UpdateState::Current: message = UiText::UpdateCurrent; break;
        case UpdateState::Downloading: message = UiText::UpdateDownloading; break;
        case UpdateState::Ready: message = UiText::UpdateRestart; break;
        case UpdateState::RollbackReady: message = UiText::RollbackRestart; break;
        case UpdateState::Installing: message = UiText::UpdateInstalling; break;
        case UpdateState::Installed: message = UiText::UpdateInstalled; break;
        case UpdateState::Restored: message = UiText::UpdateRestored; break;
        case UpdateState::Failed: message = UiText::UpdateFailed; break;
        default: break;
        }
        ImGui::TextWrapped("%s", Tr(message));
        if (!update.version.empty() && update.state != UpdateState::Installed)
            ImGui::Text("%s  >  %s", FENBVersion, update.version.c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(Muted));
        ImGui::TextWrapped("%s", Tr(UiText::UpdateSettingsSafe));
        ImGui::PopStyleColor();
        ImGui::PopTextWrapPos();
        const float buttonWidth = width-40;
        auto flushSettings = [&]() {
            if (r.pendingSave()) r.save();
            return !r.pendingSave() && r.message != UiText::ConfigWriteFailed;
        };
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(.32f,.26f,.40f,1));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(.42f,.33f,.52f,1));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(.49f,.39f,.60f,1));
        if (update.state == UpdateState::Available) {
            if (ImGui::Button(Label(UiText::UpdateInstall).c_str(), {buttonWidth,30})) {
                if (flushSettings()) r.updater.install(r.settings.language);
            }
        } else if (ready) {
            if (ImGui::Button(Label(update.state == UpdateState::RollbackReady ? UiText::RollbackCloseGame : UiText::UpdateCloseGame).c_str(), {buttonWidth,30})) {
                if (flushSettings()) r.closeForUpdate();
            }
        } else if (!r.updater.busy()) {
            if (ImGui::Button(Label(UiText::UpdateCheck).c_str(), {buttonWidth,30})) r.updater.check(r.settings.language);
        }
        ImGui::PopStyleColor(3);
        if (update.rollback && !r.updater.busy()) {
            if (ImGui::Button(Label(UiText::UpdateRollback).c_str(), {buttonWidth,30})) {
                if (flushSettings()) r.updater.rollback(r.settings.language);
            }
        }
        if (r.message == UiText::ConfigWriteFailed) ImGui::TextWrapped("%s",Tr(r.message));
        if (ImGui::Button(Label(UiText::UpdateLater).c_str(), {buttonWidth,28})) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}
} // namespace
void DrawPanel(Runtime &r, const PanelBranding &branding) {
    Theme();
    auto &io = ImGui::GetIO();
    auto &s = r.settings;
    const Settings before = s;
    uiLanguage = s.language;
    uiMotion = s.interfaceMotion;
    uiEmphasis = PanelEmphasisFont();
    if (r.centerPanel) r.updater.menuOpened(s.language);
    r.updater.poll();
    ImVec2 size{std::max(310.f, std::min(560.f, io.DisplaySize.x - 24)),
                std::max(250.f, std::min(540.f, io.DisplaySize.y - 24))};
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos({(io.DisplaySize.x - size.x) * .5f, (io.DisplaySize.y - size.y) * .5f},
                            r.centerPanel ? ImGuiCond_Always : ImGuiCond_Appearing);
    r.centerPanel = false;
    ImGui::Begin("FENB", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    auto origin = ImGui::GetWindowPos();
    auto *d = ImGui::GetWindowDrawList();
    const float brandEnd = HeaderBrand(d, origin, size.x, branding);
    float tabX = brandEnd + 2;
    const float tabFont=size.x<440?11.f:13.f;
    const float tabInset=size.x<440?5.f:8.f, tabGap=size.x<440?3.f:4.f;
    ImGui::PushFont(uiEmphasis, tabFont);
    const int tabPages[]={0,8,4};
    const UiText tabNames[]={UiText::Image,UiText::MaterialsTab,UiText::WorldTab};
    const char *tabIds[]={"image-tab","materials-tab","world-tab"};
    for (int i=0;i<3;++i) {
        const char *label = Tr(tabNames[i]);
        const float w = ImGui::CalcTextSize(label).x+2*tabInset;
        ImGui::SetCursorPos({tabX,7});
        if (ImGui::InvisibleButton(tabIds[i],{w,32},ImGuiButtonFlags_EnableNav)) r.editorPage=tabPages[i];
        const bool selected = i ? (r.editorPage==tabPages[i]||(i==2&&r.editorPage==9)) : r.editorPage<4 || r.editorPage==6 || r.editorPage==7;
        if (ImGui::IsItemHovered()) d->AddRectFilled({origin.x+tabX,origin.y+7},{origin.x+tabX+w,origin.y+39},Raised,2);
        TextAt(d,{origin.x+tabX+tabInset,origin.y+15},label,selected?Lavender:Muted,tabFont);
        if(selected) d->AddRectFilled({origin.x+tabX+tabInset,origin.y+37},{origin.x+tabX+w-tabInset,origin.y+39},Accent,1);
        tabX+=w+tabGap;
    }
    ImGui::PopFont();
    // On small displays move language controls onto a second header row.
    const bool narrow = tabX > size.x-230;
    const float languageY = narrow ? 43.f : 8.f;
    ImGui::SetCursorPos({narrow ? 16.f : size.x-230,languageY});
    ImGui::PushFont(nullptr, 12);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(5,8));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8,6));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(7,6));
    ImGui::SetNextItemWidth(62);
    ImGui::SetNextWindowSizeConstraints({160,0},{240,FLT_MAX});
    if (BeginCombo("##language", Tr(UiText::LanguageShort))) {
        for (std::size_t i=0;i<LanguageCount;++i) {
            ImGui::PushID(LanguageCodes[i]);
            const bool selected = LanguageIndex(s.language)==i;
            if (ImGui::Selectable((std::string(LanguageNames[i])+"###option").c_str(),selected))
                s.language=static_cast<Language>(i);
            if (selected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar(3);
    ImGui::PopFont();
    Tip(Tr(UiText::LanguageTab));
    uiLanguage=s.language;
    ImGui::SetCursorPos({size.x-152,narrow?46.f:14.f});
    SquareSwitch("master",s.enabled,{28,18});
    const auto master = ImGui::GetItemRectMin();
    Tip(Tr(UiText::MasterHelp));
    TextAt(d,{origin.x+size.x-118,std::floor(master.y+9-13*.5f)},Tr(UiText::Effects),Muted,13,uiEmphasis);
    ImGui::SetCursorPos({size.x - 57, 8});
    if (ImGui::InvisibleButton("menu", {24, 24}, ImGuiButtonFlags_EnableNav))
        ImGui::OpenPopup("actions");
    auto mp = ImGui::GetItemRectMin();
    for (int i = 0; i < 3; ++i)
        d->AddCircleFilled({mp.x + 6 + i * 5.f, mp.y + 12}, 1, Muted, 8);
    Tip(Tr(UiText::Actions));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 7));
    if (ImGui::BeginPopup("actions")) {
        if (ImGui::MenuItem(Label(UiText::Updates).c_str())) r.updateDialog = true;
        ImGui::Separator();
        if (ImGui::MenuItem(Label(UiText::V2Appearance).c_str())) r.editorPage=5;
        ImGui::Separator();
#ifdef FENB_DIAGNOSTICS
        if (ImGui::MenuItem(Label(UiText::CaptureFrames).c_str()))
            r.requestCapture();
        ImGui::Separator();
#endif
        ImGui::MenuItem(Label(UiText::WaterDepth24).c_str(), nullptr, &s.preciseWaterDepth);
        Tip(Tr(UiText::WaterDepth24Help));
        ImGui::Separator();
        if (ImGui::MenuItem(Label(UiText::SaveSettings).c_str(), "Ctrl+S"))
            r.save();
        if (ImGui::MenuItem(Label(UiText::ReloadSettings).c_str()))
            r.reload();
        ImGui::Separator();
        if (ImGui::MenuItem(Label(UiText::ResetAll).c_str())) {
            ResetAll(s);
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::SetCursorPos({size.x - 30, 8});
    if (ImGui::InvisibleButton("close", {22, 24}, ImGuiButtonFlags_EnableNav)) {
        r.visible = false;
        r.compare = false;
    }
    auto cp = ImGui::GetItemRectMin();
    d->AddLine({cp.x + 7, cp.y + 8}, {cp.x + 15, cp.y + 16}, Muted, 1);
    d->AddLine({cp.x + 15, cp.y + 8}, {cp.x + 7, cp.y + 16}, Muted, 1);
    d->AddLine({origin.x + 1, origin.y + (narrow?76:44)}, {origin.x + size.x - 1, origin.y + (narrow?76:44)}, Line, 1);
    const float headerExtra = narrow ? 32.f : 0.f;
    ImGui::SetCursorPos({16, 53 + headerExtra});
    ImGui::PushFont(nullptr, 12);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 7));
    ProfilePicker(r, size.x);
    ImGui::PopStyleVar();
    ImGui::PopFont();
    ImGui::SetCursorPos({16, 91 + headerExtra});
    ImGui::BeginChild("adjustments", {size.x - 32, size.y - 130 - headerExtra}, ImGuiChildFlags_None);
    ImGui::SetScrollY(0);
    DrawPanelEditor(r);
    ImGui::EndChild();
    float footer = size.y - 28;
    d->AddLine({origin.x + 1, origin.y + footer}, {origin.x + size.x - 1, origin.y + footer}, Line, 1);
    const char *status = r.message != UiText::None ? Tr(r.message)
                         : r.pendingSave()         ? Tr(UiText::Saving)
                                                   : Tr(UiText::Autosave);
    TextAt(d, {origin.x + 16, origin.y + footer + 7}, FENBVersion, Muted, 12);
    const float fpsLeft=FooterSwitch("unlock-fps",UiText::UnlockFPS,s.unlockFps,r.fpsUnlockSupported(),
        UiText::UnlockFPSHelp,size.x-28,footer+6);
    const float controlsLeft=FooterSwitch("background-activity",UiText::BackgroundActivityShort,
        s.backgroundActivity,r.backgroundActivitySupported(),UiText::BackgroundActivityHelp,fpsLeft-20,footer+6);
    if (size.x>=430 && controlsLeft-16>65) {
        d->PushClipRect({origin.x+65,origin.y+footer},{origin.x+controlsLeft-16,origin.y+size.y},true);
        TextAt(d, {origin.x + 65, origin.y + footer + 7}, status, Muted, 12);
        d->PopClipRect();
    }
    s.sanitize();
    if (!(s == before))
        r.settingsChanged();
    if (!r.visible && r.pendingSave())
        r.save();
    UpdateDialog(r, origin, size);
    ImGui::End();
    const auto mouse = io.MousePos;
    if (s.customCursor && r.visible && ImGui::IsMousePosValid() && mouse.x >= 0 && mouse.y >= 0 &&
        mouse.x < io.DisplaySize.x && mouse.y < io.DisplaySize.y) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        auto p=io.MousePos;
        ImVec2 polygon[4];
        for(int i=0;i<4;++i) polygon[i]={std::floor(p.x)+CursorPoints[i].x-1,std::floor(p.y)+CursorPoints[i].y-1};
        auto *foreground=ImGui::GetForegroundDrawList();
        foreground->AddConcavePolyFilled(polygon,4,CursorFill);
        foreground->AddPolyline(polygon,4,CursorEdge,CursorWeight,ImDrawFlags_Closed);
    }
}

PanelBranding Runtime::panelBranding() {
    if (!panelLogo_.p) {
        // Default-pool dynamic textures work on both D3D9 and D3D9Ex. Runtime
        // releases this resource before Reset and recreates it only when needed.
        Com<IDirect3DTexture9> texture;
        D3DLOCKED_RECT lock{};
        if (SUCCEEDED(device_->CreateTexture(UI_LOGO_WIDTH, UI_LOGO_HEIGHT, 1, D3DUSAGE_DYNAMIC,
                                               D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, texture.out(), nullptr)) &&
            SUCCEEDED(texture->LockRect(0, &lock, nullptr, D3DLOCK_DISCARD))) {
            for (unsigned y = 0; y < UI_LOGO_HEIGHT; ++y)
                memcpy(static_cast<unsigned char *>(lock.pBits) + size_t(y) * lock.Pitch,
                       UI_LOGO_BGRA + size_t(y) * UI_LOGO_WIDTH, UI_LOGO_WIDTH * sizeof(std::uint32_t));
            texture->UnlockRect(0);
            panelLogo_.p = texture.p;
            texture.p = nullptr;
        }
    }
    PanelBranding branding;
    branding.layout = PanelBranding::Layout::SymbolOnly;
    branding.symbol = panelLogo_;
    branding.symbolAspect = float(UI_LOGO_WIDTH) / UI_LOGO_HEIGHT;
    return branding;
}

void DrawPanel(Runtime &runtime) {
    DrawPanel(runtime, runtime.panelBranding());
}
