#include "imgui.h"
#include "runtime.hpp"
#include "version.hpp"
#include "ui_branding.hpp"
#include "generated/ui_logo.hpp"
#include <cstdio>

namespace {
thread_local Language uiLanguage = Language::English;
const char *Tr(UiText text) {
    return Translate(uiLanguage, text);
}
std::string Label(UiText text) {
    return std::string(Tr(text)) + "###text-" + std::to_string(static_cast<int>(text));
}
constexpr ImU32 Background = IM_COL32(29, 30, 33, 255), Raised = IM_COL32(35, 36, 40, 255);
constexpr ImU32 Line = IM_COL32(54, 56, 61, 255), Text = IM_COL32(221, 223, 227, 255);
constexpr ImU32 Muted = IM_COL32(143, 147, 155, 255), Accent = IM_COL32(171, 157, 204, 255);
constexpr float RowHeight = 26;
ImU32 Opacity(ImU32 color) {
    auto a = static_cast<ImU32>((color >> 24) * ImGui::GetStyle().Alpha);
    return (color & 0x00ffffff) | (a << 24);
}
void TextAt(ImDrawList *draw, ImVec2 p, const char *text, ImU32 color = Text, float size = 15) {
    draw->AddText(ImGui::GetFont(), size, {std::floor(p.x), std::floor(p.y)}, Opacity(color), text);
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
    const ImVec2 p{origin.x + 12, origin.y + std::floor((40 - height) / 2)};
    d->AddImage(ImTextureRef(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texture))), p,
                {p.x + width, p.y + height}, {0, 0}, {1, 1}, Opacity(IM_COL32_WHITE));
    if (layout == PanelBranding::Layout::SymbolText) {
        TextAt(d, {origin.x + 49, origin.y + 12}, "FENB");
        return 98;
    }
    return layout == PanelBranding::Layout::Horizontal ? 126.f : 51.f;
}
void HeaderTabHighlight(ImDrawList *draw, ImVec2 origin, float left, float right) {
    if (right <= left)
        return;
    draw->AddRectFilled({origin.x + left, origin.y + 6}, {origin.x + right, origin.y + 34},
                        Opacity(IM_COL32(171, 133, 225, 20)), 3);
    draw->AddRectFilled({origin.x + left + 6, origin.y + 32},
                        {origin.x + right - 6, origin.y + 34},
                        Opacity(IM_COL32(181, 144, 235, 210)), 1);
}
void Theme() {
    auto &s = ImGui::GetStyle();
    for (auto &c : s.Colors)
        c = ImVec4(.23f, .24f, .27f, 1);
    s.WindowPadding = {0, 0};
    s.WindowRounding = 5;
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
    s.PopupRounding = 3;
    s.PopupBorderSize = 1;
    s.Colors[ImGuiCol_Text] = {.87f, .88f, .90f, 1};
    s.Colors[ImGuiCol_TextDisabled] = {.48f, .50f, .54f, 1};
    s.Colors[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(Background);
    s.Colors[ImGuiCol_ChildBg] = {0, 0, 0, 0};
    s.Colors[ImGuiCol_Border] = ImGui::ColorConvertU32ToFloat4(Line);
    s.Colors[ImGuiCol_FrameBg] = {0, 0, 0, 0};
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
    s.Colors[ImGuiCol_Header] = {.24f, .25f, .28f, 1};
    s.Colors[ImGuiCol_HeaderHovered] = {.28f, .29f, .33f, 1};
    s.Colors[ImGuiCol_HeaderActive] = {.32f, .33f, .37f, 1};
    s.Colors[ImGuiCol_TextSelectedBg] = {.40f, .37f, .49f, .65f};
    s.Colors[ImGuiCol_NavCursor] = ImGui::ColorConvertU32ToFloat4(Accent);
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
bool SquareSwitch(const char *id, bool &value, ImVec2 size = {18, 18}) {
    auto p = ImGui::GetCursorScreenPos();
    bool changed = ImGui::InvisibleButton(id, size, ImGuiButtonFlags_EnableNav);
    if (changed)
        value = !value;
    auto *d = ImGui::GetWindowDrawList();
    auto center = ImVec2(std::floor(p.x + size.x / 2), std::floor(p.y + size.y / 2));
    d->AddRectFilled({center.x - 6, center.y - 6}, {center.x + 6, center.y + 6},
                     value ? IM_COL32(176, 179, 187, 255) : Raised, 2);
    d->AddRect({center.x - 6, center.y - 6}, {center.x + 6, center.y + 6},
               ImGui::IsItemHovered() ? Accent : IM_COL32(104, 107, 116, 255), 2);
    if (value) {
        d->AddLine({center.x - 3, center.y}, {center.x - 1, center.y + 2}, Background, 1.5f);
        d->AddLine({center.x - 1, center.y + 2}, {center.x + 3, center.y - 3}, Background, 1.5f);
    }
    return changed;
}
void ResetGlyph(ImDrawList *d, ImVec2 p, ImU32 color) {
    ImVec2 c{p.x + 10, p.y + 9};
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
         const char *format = "%.2f", float step = .01f, bool clock = false, const char *stableId = nullptr) {
    if (stableId)
        ImGui::PushID(stableId);
    else
        ImGui::PushID(&value);
    auto p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    auto *d = ImGui::GetWindowDrawList();
    const float trackX = std::min(146.f, width * .32f), inputX = width - 120,
                trackWidth = std::max(20.f, inputX - trackX - 10);
    ImGui::InvisibleButton("label", {trackX - 8, RowHeight});
    Tip(help);
    TextAt(d, {p.x, p.y + 5}, Tr(label));
    ImGui::SetCursorScreenPos({p.x + trackX, p.y});
    ImGui::InvisibleButton("track", {trackWidth, RowHeight}, ImGuiButtonFlags_EnableNav);
    bool hover = ImGui::IsItemHovered(), active = ImGui::IsItemActive();
    auto *storage = ImGui::GetStateStorage();
    auto resetKey = ImGui::GetID("resetGesture");
    bool suppress = storage->GetBool(resetKey);
    if (!ImGui::IsMouseDown(0))
        suppress = false;
    if (hover && ImGui::IsMouseDoubleClicked(0)) {
        value = neutral;
        suppress = true;
    } else if (active && ImGui::IsMouseDown(0) && !suppress)
        value = Snap(
            low + std::clamp((ImGui::GetIO().MousePos.x - p.x - trackX - 3) / (trackWidth - 6), 0.f, 1.f) *
                      (high - low),
            low, high, neutral, step);
    storage->SetBool(resetKey, suppress);
    if (ImGui::IsItemFocused()) {
        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
            value = Snap(value - step, low, high, neutral, step);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
            value = Snap(value + step, low, high, neutral, step);
    }
    float y = std::floor(p.y + RowHeight * .5f), left = p.x + trackX + 3, right = left + trackWidth - 6;
    float knob = std::floor(left + (right - left) * std::clamp((value - low) / (high - low), 0.f, 1.f));
    float zero = std::floor(left + (right - left) * (neutral - low) / (high - low));
    d->AddLine({left, y}, {right, y}, Opacity(IM_COL32(76, 79, 86, 255)), 1);
    d->AddLine({zero, y - 3}, {zero, y + 3}, Opacity(IM_COL32(99, 102, 110, 255)), 1);
    d->AddLine({zero, y}, {knob, y}, Opacity(IM_COL32(145, 148, 157, 255)), 2);
    d->AddRectFilled({knob - 2, y - 5}, {knob + 3, y + 5},
                     Opacity(active ? Accent : IM_COL32(191, 194, 201, 255)), 1);
    // All three fields keep their positions during text editing. Arrow buttons reserve their space.
    ImGui::PushFont(nullptr, 14);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(Muted));
    char displayed[48];
    std::snprintf(displayed, sizeof(displayed), format, value);
    if (clock) {
        int minutes = std::clamp(int(value), 0, 1439);
        std::snprintf(displayed, sizeof(displayed), "%02d:%02d", minutes / 60, minutes % 60);
    }
    float numberPadding = std::max(2.f, (40 - ImGui::CalcTextSize(displayed).x) * .5f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(numberPadding, 3));
    auto stepArrow = [&](const char *id, float x, bool rightward) {
        ImGui::SetCursorScreenPos({p.x + x, p.y + 4});
        ImGui::PushItemFlag(ImGuiItemFlags_ButtonRepeat, true);
        bool pressed = ImGui::InvisibleButton(id, {14, 18}, ImGuiButtonFlags_EnableNav);
        ImGui::PopItemFlag();
        auto min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
        bool hovered = ImGui::IsItemHovered(), held = ImGui::IsItemActive();
        if (hovered || held)
            d->AddRectFilled(min, max, Opacity(held ? IM_COL32(57, 59, 65, 255) : Raised), 2);
        float cx = std::floor(min.x + 7), cy = std::floor(min.y + 9), direction = rightward ? 1.f : -1.f;
        ImU32 color = Opacity(hovered || held ? Text : Muted);
        d->AddLine({cx - direction * 1.5f, cy - 3}, {cx + direction * 1.5f, cy}, color, 1);
        d->AddLine({cx + direction * 1.5f, cy}, {cx - direction * 1.5f, cy + 3}, color, 1);
        return pressed;
    };
    if (stepArrow("less", inputX + 15, false))
        value = Snap(value - step, low, high, neutral, step);
    ImGui::SetCursorScreenPos({p.x + inputX + 29, p.y + 2});
    ImGui::SetNextItemWidth(40);
    if (clock) {
        if (ImGui::InputText("##clock", displayed, sizeof(displayed), ImGuiInputTextFlags_AutoSelectAll)) {
            int hours, minutes;
            char tail;
            if (std::sscanf(displayed, "%d:%d%c", &hours, &minutes, &tail) == 2 &&
                hours >= 0 && hours < 24 && minutes >= 0 && minutes < 60)
                value = float(hours * 60 + minutes);
        }
    } else if (ImGui::InputFloat("##value", &value, 0, 0, format, ImGuiInputTextFlags_AutoSelectAll)) {
        value = std::isfinite(value) ? std::clamp(value, low, high) : neutral;
        if (std::abs(value - neutral) < step * .01f)
            value = neutral;
    }
    Tip(Tr(clock ? UiText::TimeInputHelp : UiText::NumberInputHelp));
    if (stepArrow("more", inputX + 69, true))
        value = Snap(value + step, low, high, neutral, step);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::PopFont();
    bool modified = std::abs(value - neutral) > step * .01f;
    ImGui::SetCursorScreenPos({p.x + width - 20, p.y + 4});
    ImGui::BeginDisabled(!modified);
    if (ImGui::InvisibleButton("reset", {20, 18}, ImGuiButtonFlags_EnableNav))
        value = neutral;
    if (std::abs(value - neutral) > step * .01f) {
        ResetGlyph(d, ImGui::GetItemRectMin(), Opacity(ImGui::IsItemHovered() ? Text : Muted));
        Tip(Tr(UiText::ResetValue));
    }
    ImGui::EndDisabled();
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({width, RowHeight});
    ImGui::PopID();
}
bool Section(const char *id, const char *title, bool &enabled, bool &reset, bool showEnable = true,
             bool initiallyOpen = true) {
    reset = false;
    ImGui::PushID(id);
    auto p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    auto *d = ImGui::GetWindowDrawList();
    auto key = ImGui::GetID("expanded");
    auto *state = ImGui::GetStateStorage();
    bool expanded = state->GetBool(key, initiallyOpen);
    d->AddLine(p, {p.x + width, p.y}, Line, 1);
    if (ImGui::InvisibleButton("heading", {width - 60, 28}, ImGuiButtonFlags_EnableNav)) {
        expanded = !expanded;
        state->SetBool(key, expanded);
    }
    ImU32 c = ImGui::IsItemHovered() ? Text : Muted;
    if (expanded)
        d->AddTriangleFilled({p.x + 1, p.y + 12}, {p.x + 8, p.y + 12}, {p.x + 4.5f, p.y + 16}, c);
    else
        d->AddTriangleFilled({p.x + 2, p.y + 10}, {p.x + 2, p.y + 17}, {p.x + 6, p.y + 13.5f}, c);
    TextAt(d, {p.x + 17, p.y + 6}, title);
    ImGui::SetCursorScreenPos({p.x + width - 50, p.y + 5});
    reset = ImGui::InvisibleButton("reset-section", {22, 18}, ImGuiButtonFlags_EnableNav);
    ResetGlyph(d, ImGui::GetItemRectMin(), ImGui::IsItemHovered() ? Text : Muted);
    Tip(Tr(UiText::ResetSection));
    if (showEnable) {
        ImGui::SetCursorScreenPos({p.x + width - 22, p.y + 5});
        SquareSwitch("enabled", enabled);
        Tip(enabled ? Tr(UiText::DisableEffect) : Tr(UiText::EnableEffect));
    }
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({width, 28});
    ImGui::PopID();
    return expanded;
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
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Muted), "%s", Tr(UiText::Profile));
    ImGui::SameLine(88);
    ImGui::SetNextItemWidth(width - 216);
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
    if (ImGui::BeginCombo("##profile", caption.c_str(),
                          ImGuiComboFlags_HeightLarge)) {
        ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextBorderSize, 1.f);
        ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0, .5f));
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(.19f, .20f, .22f, 1));
        ImGui::SeparatorText(Tr(UiText::BuiltInProfiles));
        for (int i = 0; i < BuiltInPresetCount; ++i)
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
    ImGui::SameLine(width - 112);
    if (ImGui::Button(Label(r.selectedProfile.empty() ? UiText::Create : UiText::SaveAs).c_str(), {96, 23}) ||
        createRequested) {
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
void SceneControls(Runtime &runtime) {
    auto &s = runtime.settings;
    auto status = runtime.sceneStatus();
    auto notice = [](UiText text) {
        ImGui::PushFont(nullptr, 13);
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", Tr(text));
        ImGui::PopTextWrapPos();
        ImGui::PopFont();
        ImGui::Dummy({0, 5});
    };
    const Settings defaults;
    bool reset = false;
    auto section = [&](const char *id, const char *title, bool &value, int group) {
        bool open = Section(id, title, value, reset, true, false);
        if (reset)
            s.resetSceneSection(group);
        return open;
    };
    if (section("ssao", "SSAO", s.ssao, 0)) {
        ImGui::BeginDisabled(!s.ssao || !s.enabled);
        Row(UiText::Strength, Tr(UiText::AOStrengthHelp), s.aoStrength, 0, 2, defaults.aoStrength, "%.2f",
            .05f);
        Row(UiText::Radius, Tr(UiText::AORadiusHelp), s.aoRadius, .1f, 5, defaults.aoRadius, "%.2f", .1f);
        Row(UiText::Bias, Tr(UiText::AOBiasHelp), s.aoBias, .005f, .3f, defaults.aoBias, "%.3f", .005f);
        Row(UiText::Distance, Tr(UiText::AODistanceHelp), s.aoDistance, 10, 500, defaults.aoDistance, "%.0f",
            10);
        ImGui::EndDisabled();
        if (s.enabled && s.ssao && !status.depth)
            notice(UiText::SceneDepthUnavailable);
    }
    if (section("rays", Tr(UiText::SunRays), s.sunRays, 1)) {
        ImGui::BeginDisabled(!s.sunRays || !s.enabled);
        Row(UiText::Strength, Tr(UiText::RayStrengthHelp), s.rayStrength, 0, 2, defaults.rayStrength, "%.2f",
            .05f);
        Row(UiText::Length, Tr(UiText::RayLengthHelp), s.rayLength, .1f, 1.5f, defaults.rayLength, "%.2f",
            .05f);
        Row(UiText::Decay, Tr(UiText::RayDecayHelp), s.rayDecay, .8f, .99f, defaults.rayDecay, "%.2f", .01f);
        ImGui::EndDisabled();
        if (s.enabled && s.sunRays) {
            if (!status.depth)
                notice(UiText::SceneDepthUnavailable);
            else if (!status.sun)
                notice(UiText::RaysNeedSun);
        }
    }
    if (section("sky", "SkyFX", s.sky, 2)) {
        ImGui::BeginDisabled(!s.sky || !s.enabled);
        Row(UiText::Atmosphere, Tr(UiText::SkyStrengthHelp), s.skyStrength, 0, 1, defaults.skyStrength,
            "%.2f", .05f);
        Row(UiText::Horizon, Tr(UiText::HorizonHelp), s.skyHorizon, .1f, 1, defaults.skyHorizon, "%.2f",
            .05f);
        Row(UiText::Clouds, Tr(UiText::CloudsHelp), s.cloudContrast, .5f, 2, defaults.cloudContrast, "%.2f",
            .05f);
        Row(UiText::SunHalo, Tr(UiText::SunHaloHelp), s.sunHalo, 0, 2, defaults.sunHalo, "%.2f", .05f);
        ImGui::EndDisabled();
        if (s.enabled && s.sky && !status.sky)
            notice(UiText::SkyNotDetected);
    }
    if (section("fog", Tr(UiText::Fog), s.fog, 3)) {
        ImGui::BeginDisabled(!s.fog || !s.enabled);
        Row(UiText::Density, Tr(UiText::FogDensityHelp), s.fogDensity, 0, .03f, defaults.fogDensity, "%.4f",
            .0005f);
        Row(UiText::Start, Tr(UiText::FogStartHelp), s.fogStart, 0, 250, defaults.fogStart, "%.0f", 5);
        Row(UiText::Height, Tr(UiText::FogHeightHelp), s.fogHeight, -500, 500, defaults.fogHeight, "%.0f", 5);
        Row(UiText::HeightFalloff, Tr(UiText::FogFalloffHelp), s.fogFalloff, 0, .2f, defaults.fogFalloff,
            "%.3f", .005f);
        float tint[] = {s.fogR, s.fogG, s.fogB};
        ImGui::SetNextItemWidth(230);
        if (ImGui::ColorEdit3(Label(UiText::FogColor).c_str(), tint,
                              ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoOptions |
                                  ImGuiColorEditFlags_NoSidePreview)) {
            s.fogR = tint[0];
            s.fogG = tint[1];
            s.fogB = tint[2];
        }
        ImGui::EndDisabled();
    }
    if (section("dof", Tr(UiText::DepthOfField), s.dof, 4)) {
        ImGui::BeginDisabled(!s.dof || !s.enabled);
        Row(UiText::Focus, Tr(UiText::FocusHelp), s.focusDistance, 1, 500, defaults.focusDistance, "%.0f", 1);
        Row(UiText::FocusRange, Tr(UiText::FocusRangeHelp), s.focusRange, 1, 200, defaults.focusRange, "%.0f",
            1);
        Row(UiText::Blur, Tr(UiText::BlurHelp), s.dofRadius, 0, 12, defaults.dofRadius, "%.1f", .5f);
        Row(UiText::NearBlur, Tr(UiText::NearBlurHelp), s.dofNear, 0, 1, defaults.dofNear, "%.2f", .05f);
        ImGui::EndDisabled();
    }
    if (section("adaptation", Tr(UiText::Adaptation), s.adaptation, 5)) {
        ImGui::BeginDisabled(!s.adaptation || !s.enabled);
        Row(UiText::AverageBrightness, Tr(UiText::AverageBrightnessHelp), s.adaptationKey, .04f, .5f,
            defaults.adaptationKey, "%.2f", .01f);
        Row(UiText::MinEV, Tr(UiText::MinEVHelp), s.minEV, -4, 0, defaults.minEV, "%+.2f", .1f);
        Row(UiText::MaxEV, Tr(UiText::MaxEVHelp), s.maxEV, 0, 4, defaults.maxEV, "%+.2f", .1f);
        Row(UiText::AdaptBright, Tr(UiText::AdaptBrightHelp), s.adaptBright, .1f, 5, defaults.adaptBright,
            "%.1f", .1f);
        Row(UiText::AdaptDark, Tr(UiText::AdaptDarkHelp), s.adaptDark, .1f, 5, defaults.adaptDark, "%.1f",
            .1f);
        ImGui::EndDisabled();
    }
    if (section("lighting-balance", Tr(UiText::LightingBalance), s.lightingBalance, 10)) {
        ImGui::BeginDisabled(!s.enabled || !s.lightingBalance);
        Row(UiText::Strength, Tr(UiText::LightingBalanceHelp), s.lightingBalanceStrength, 0, 1,
            defaults.lightingBalanceStrength, "%.2f", .05f);
        ImGui::EndDisabled();
    }
    bool vegetation = true;
    if (Section("foliage", Tr(UiText::Vegetation), vegetation, reset, false, false)) {
        if (reset)
            s.resetSceneSection(9);
        Row(UiText::NightBrightness, Tr(UiText::NightBrightnessHelp), s.nightFoliage, 0, 1,
            defaults.nightFoliage, "%.2f", .05f);
    } else if (reset)
        s.resetSceneSection(9);
    if (section("lighting", Tr(UiText::LocalLighting), s.localLighting, 6)) {
        ImGui::BeginDisabled(!s.localLighting || !s.enabled);
        Row(UiText::Strength, Tr(UiText::LocalStrengthHelp), s.localStrength, 0, 3, defaults.localStrength,
            "%.2f", .05f);
        Row(UiText::Distance, Tr(UiText::LocalRangeHelp), s.localRange, .25f, 3, defaults.localRange, "%.2f",
            .05f);
        ImGui::EndDisabled();
        if (s.enabled && s.localLighting) {
            if (!status.depth)
                notice(UiText::SceneDepthUnavailable);
            else if (!status.light)
                notice(UiText::LightNotDetected);
        }
    }
    ImGui::Spacing();
    auto qualityPos = ImGui::GetCursorScreenPos();
    float qualityWidth = ImGui::GetContentRegionAvail().x;
    TextAt(ImGui::GetWindowDrawList(), {qualityPos.x, qualityPos.y + 5}, Tr(UiText::SceneQuality));
    ImGui::SetCursorScreenPos({qualityPos.x + qualityWidth - 22, qualityPos.y + 4});
    SquareSwitch("sceneQuality", s.sceneQuality);
    ImGui::SetCursorScreenPos(qualityPos);
    ImGui::Dummy({qualityWidth, RowHeight});
}
void AntialiasControls(Runtime &r) {
    auto &s = r.settings;
    bool reset = false;
    bool hardware = true;
    if (Section("post-aa", Tr(UiText::PostAntialiasing), hardware, reset, false)) {
        if (reset) {
            s.resetSceneSection(8);
            s.fxaa = s.smaa = false;
        }
        ImGui::BeginDisabled(!s.enabled);
        int method = s.smaa ? 2 : s.fxaa ? 1 : 0;
        const char *methods[] = {Tr(UiText::Off), Tr(UiText::FXAAMethod), Tr(UiText::SMAAMethod)};
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##post-aa", methods[method])) {
            for (int i = 0; i < 3; ++i) {
                ImGui::PushID(i);
                if (ImGui::Selectable((std::string(methods[i]) + "###method").c_str(), method == i)) {
                    s.fxaa = i == 1;
                    s.smaa = i == 2;
                }
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
    } else if (reset) {
        s.resetSceneSection(8);
        s.fxaa = s.smaa = false;
    }
    ImGui::Dummy({0, 8});
    if (Section("msaa", Tr(UiText::MSAAMethod), hardware, reset, false)) {
        if (reset)
            s.msaaSamples = 0;
        const unsigned active = r.activeMSAA();
        int modes[] = {-1, 2, 4, 8}, choice = -1;
        for (int i = 0; i < 4; ++i)
            if (active == unsigned(modes[i] < 0 ? 0 : modes[i]))
                choice = i;
        const char *labels[] = {Tr(UiText::Off), Tr(UiText::MSAATwo), Tr(UiText::MSAAFour),
                                Tr(UiText::MSAAEight)};
        char nativeLabel[32];
        std::snprintf(nativeLabel, sizeof(nativeLabel), Tr(UiText::MSAAMultiplier), active);
        const char *current = choice >= 0   ? labels[choice]
                              : active == 1 ? Tr(UiText::MSAAMethod)
                                            : nativeLabel;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::BeginCombo("##msaa", current)) {
            if (choice < 0) {
                ImGui::BeginDisabled();
                ImGui::Selectable((std::string(current) + "###native-samples").c_str(), true);
                ImGui::EndDisabled();
            }
            for (int i = 0; i < 4; ++i) {
                bool available = r.msaaAvailable(modes[i]);
                ImGui::PushID(i);
                ImGui::BeginDisabled(!available);
                if (ImGui::Selectable((std::string(labels[i]) + "###samples").c_str(), choice == i))
                    s.msaaSamples = modes[i];
                ImGui::EndDisabled();
                if (!available && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ImGui::SetTooltip("%s", Tr(UiText::MSAAUnavailable));
                ImGui::PopID();
            }
            ImGui::EndCombo();
        }
    } else if (reset)
        s.msaaSamples = 0;
}
void EnvironmentControls(Runtime &r) {
    auto &s = r.settings;
    bool reset = false;
    bool open = Section("world-time", Tr(UiText::TimeOverride), s.timeOverride, reset);
    if (reset) { s.timeMinutes = 720; s.freezeTime = true; }
    if (open) {
        ImGui::BeginDisabled(!s.timeOverride || !s.enabled);
        float minutes = r.environmentMinutes(), previous = minutes;
        Row(UiText::TimeOfDay, Tr(UiText::TimeOfDayHelp), minutes, 0, 1439, 720, "%.0f", 1, true, "time-of-day");
        if (minutes != previous)
            s.timeMinutes = minutes;
        auto p = ImGui::GetCursorScreenPos();
        SquareSwitch("freeze-time", s.freezeTime, {18, RowHeight});
        Tip(Tr(UiText::FreezeTimeHelp));
        TextAt(ImGui::GetWindowDrawList(), {p.x + 25, p.y + 5}, Tr(UiText::FreezeTime));
        ImGui::EndDisabled();
    }
    ImGui::Dummy({0, 14});
    open = Section("world-weather", Tr(UiText::WeatherOverride), s.weatherOverride, reset);
    if (reset) { s.weatherType = 0; s.weatherIntensity = 100; }
    if (open) {
        ImGui::BeginDisabled(!s.weatherOverride || !s.enabled);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::ColorConvertU32ToFloat4(Raised));
        const UiText types[] = {UiText::WeatherClear, UiText::WeatherRain, UiText::WeatherSnow};
        for (int i = 0; i < 3; ++i) {
            if (i) ImGui::SameLine();
            bool selected = s.weatherType == i;
            if (ImGui::Checkbox(Label(types[i]).c_str(), &selected) && selected)
                s.weatherType = i;
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        ImGui::Dummy({0, 6});
        ImGui::BeginDisabled(s.weatherType == 0);
        Row(UiText::WeatherIntensity, Tr(UiText::WeatherIntensityHelp), s.weatherIntensity, 0, 100,
            100, "%.0f%%", 1);
        ImGui::EndDisabled();
        ImGui::EndDisabled();
    }
}
} // namespace
void DrawPanel(Runtime &r, const PanelBranding &branding) {
    Theme();
    auto &io = ImGui::GetIO();
    auto &s = r.settings;
    const Settings before = s;
    uiLanguage = s.language;
    ImVec2 size{std::max(310.f, std::min(540.f, io.DisplaySize.x - 24)),
                std::max(250.f, std::min(520.f, io.DisplaySize.y - 24))};
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    ImGui::SetNextWindowPos({(io.DisplaySize.x - size.x) * .5f, (io.DisplaySize.y - size.y) * .5f},
                            r.centerPanel ? ImGuiCond_Always : ImGuiCond_Appearing);
    r.centerPanel = false;
    ImGui::Begin("FENB", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar);
    auto origin = ImGui::GetWindowPos();
    auto *d = ImGui::GetWindowDrawList();
    float brandEnd = HeaderBrand(d, origin, size.x, branding);
    float titleX = brandEnd + 12;
    d->AddLine({origin.x + brandEnd, origin.y + 12}, {origin.x + brandEnd, origin.y + 27}, Line, 1);
    const char *pageTitle = r.editorPage == 1   ? "WaterFX"
                            : r.editorPage == 2 ? Tr(UiText::LightAtmosphere)
                            : r.editorPage == 3 ? Tr(UiText::Antialiasing)
                                                : Tr(UiText::Image);
    bool showEffectsLabel = size.x >= 480;
    float masterX = size.x - (showEffectsLabel ? 151 : 88);
    ImGui::PushFont(nullptr, 14);
    const char *languageTitle = Tr(size.x < 380 ? UiText::LanguageShort : UiText::LanguageTab);
    float languageWidth = ImGui::CalcTextSize(languageTitle).x + 16;
    float worldWidth = ImGui::CalcTextSize(Tr(UiText::WorldTab)).x + 16;
    float titleWidth = ImGui::CalcTextSize(pageTitle).x + 8;
    float languageX = std::min(titleX + titleWidth + worldWidth + 24, masterX - languageWidth - 8);
    float worldX = languageX - worldWidth - 12;
    ImGui::PopFont();
    ImGui::SetCursorPos({titleX - 4, 8});
    if (ImGui::InvisibleButton("image-tab", {std::max(1.f, worldX - titleX - 8), 24}, ImGuiButtonFlags_EnableNav))
        r.editorPage = 0;
    ImGui::SetCursorPos({worldX, 8});
    if (ImGui::InvisibleButton("world-tab", {worldWidth, 24}, ImGuiButtonFlags_EnableNav))
        r.editorPage = 4;
    bool worldHovered = ImGui::IsItemHovered();
    Tip(Tr(r.environmentSupported() ? UiText::WorldHelp : UiText::EnvironmentUnavailable));
    d->AddLine({origin.x + worldX - 6, origin.y + 12}, {origin.x + worldX - 6, origin.y + 27}, Line);
    TextAt(d, {origin.x + worldX + 8, origin.y + 12}, Tr(UiText::WorldTab),
           r.editorPage == 4 ? IM_COL32(217, 199, 245, 255) : worldHovered ? Text : Muted, 14);
    d->AddLine({origin.x + languageX - 6, origin.y + 12}, {origin.x + languageX - 6, origin.y + 27}, Line);
    ImGui::SetCursorPos({languageX, 8});
    if (ImGui::InvisibleButton("language", {languageWidth, 24}, ImGuiButtonFlags_EnableNav))
        ImGui::OpenPopup("languages");
    bool languageHovered = ImGui::IsItemHovered();
    bool languageSelected = ImGui::IsPopupOpen("languages");
    const ImU32 selectedText = IM_COL32(217, 199, 245, 255);
    if (languageSelected)
        HeaderTabHighlight(d, origin, languageX, languageX + languageWidth);
    else if (r.editorPage == 4)
        HeaderTabHighlight(d, origin, worldX, worldX + worldWidth);
    else
        HeaderTabHighlight(d, origin, titleX - 6, worldX - 10);
    ImVec4 titleClip{origin.x + titleX, origin.y + 8, origin.x + worldX - 10, origin.y + 31};
    d->AddText(ImGui::GetFont(), 14, {origin.x + titleX, origin.y + 12},
               Opacity(languageSelected || r.editorPage == 4 ? Muted : selectedText), pageTitle, nullptr, 0, &titleClip);
    if (languageHovered && !languageSelected)
        d->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), Raised, 2);
    TextAt(d, {origin.x + languageX + 8, origin.y + 12}, languageTitle,
           languageSelected ? selectedText : languageHovered ? Text : Muted, 14);
    ImGui::SetNextWindowPos({origin.x + languageX, origin.y + 36}, ImGuiCond_Appearing);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 5));
    if (ImGui::BeginPopup("languages")) {
        if (ImGui::Selectable("English", s.language == Language::English, 0, {140, 24}))
            s.language = Language::English;
        if (ImGui::Selectable("Русский", s.language == Language::Russian, 0, {140, 24}))
            s.language = Language::Russian;
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    uiLanguage = s.language;
    ImGui::SetCursorPos({masterX, 8});
    SquareSwitch("master", s.enabled, {20, 24});
    Tip(Tr(UiText::MasterHelp));
    if (showEffectsLabel)
        TextAt(d, {origin.x + size.x - 125, origin.y + 13}, Tr(UiText::Effects), s.enabled ? Text : Muted,
               13);
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
        bool fpsSupported = r.fpsUnlockSupported();
        ImGui::MenuItem(Label(UiText::UnlockFPS).c_str(), nullptr, &s.unlockFps, fpsSupported || s.unlockFps);
        Tip(fpsSupported ? Tr(UiText::UnlockFPSHelp) : Tr(UiText::UnsupportedClient),
            ImGuiHoveredFlags_AllowWhenDisabled);
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
            bool open = s.openOnStart;
            bool unlock = s.unlockFps, precise = s.preciseWaterDepth;
            int msaa = s.msaaSamples;
            auto language = s.language;
            s = Settings{};
            s.openOnStart = open;
            s.unlockFps = unlock;
            s.preciseWaterDepth = precise;
            s.msaaSamples = msaa;
            s.language = language;
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
    d->AddLine({origin.x + 1, origin.y + 40}, {origin.x + size.x - 1, origin.y + 40}, Line, 1);
    ImGui::SetCursorPos({16, 48});
    ProfilePicker(r, size.x);
    ImGui::SetCursorPos({16, 80});
    ImGui::BeginChild("adjustments", {size.x - 32, size.y - 116}, ImGuiChildFlags_None);
    const Settings defaults;
    bool reset = false;
    if (r.editorPage == 0) {
        auto *navDraw = ImGui::GetWindowDrawList();
        float waterWidth = ImGui::GetContentRegionAvail().x;
        auto p = ImGui::GetCursorScreenPos();
        navDraw->AddLine(p, {p.x + waterWidth, p.y}, Line);
        if (ImGui::InvisibleButton("scene-settings", {waterWidth, 30}, ImGuiButtonFlags_EnableNav))
            r.editorPage = 2;
        TextAt(navDraw, {p.x + 17, p.y + 7}, Tr(UiText::LightAtmosphere), Text);
        TextAt(navDraw, {p.x + waterWidth - 44, p.y + 7}, "→", Muted);
        p = ImGui::GetCursorScreenPos();
        navDraw->AddLine(p, {p.x + waterWidth, p.y}, Line);
        if (ImGui::InvisibleButton("aa-settings", {waterWidth, 30}, ImGuiButtonFlags_EnableNav))
            r.editorPage = 3;
        TextAt(navDraw, {p.x + 17, p.y + 7}, Tr(UiText::Antialiasing), Text);
        TextAt(navDraw, {p.x + waterWidth - 44, p.y + 7}, "→", Muted);
        ImGui::Dummy({0, 0});
        auto waterPos = ImGui::GetCursorScreenPos();
        navDraw->AddLine(waterPos, {waterPos.x + waterWidth, waterPos.y}, Line);
        if (ImGui::InvisibleButton("water-settings", {waterWidth - 30, 30}, ImGuiButtonFlags_EnableNav))
            r.editorPage = 1;
        ImU32 navColor = ImGui::IsItemHovered() ? Text : Muted;
        TextAt(navDraw, {waterPos.x + 17, waterPos.y + 7}, "WaterFX", Text, 15);
        float arrowX = waterPos.x + waterWidth - 39;
        navDraw->AddLine({arrowX - 5, waterPos.y + 15}, {arrowX + 5, waterPos.y + 15}, navColor, 1.3f);
        navDraw->AddLine({arrowX + 1, waterPos.y + 11}, {arrowX + 5, waterPos.y + 15}, navColor, 1.3f);
        navDraw->AddLine({arrowX + 5, waterPos.y + 15}, {arrowX + 1, waterPos.y + 19}, navColor, 1.3f);
        ImGui::SetCursorScreenPos({waterPos.x + waterWidth - 22, waterPos.y + 6});
        SquareSwitch("waterMain", s.water);
        ImGui::SetCursorScreenPos(waterPos);
        ImGui::Dummy({waterWidth, 30});
        ImGui::Dummy({0, 2});
        bool expanded = Section("grade", Tr(UiText::ColorCorrection), s.color, reset);
        if (reset)
            s.resetColor();
        if (expanded) {
            ImGui::BeginDisabled(!s.color || !s.enabled);
            Row(UiText::Exposure, Tr(UiText::ExposureHelp), s.exposure, -2, 2, defaults.exposure, "%+.2f");
            Row(UiText::Contrast, Tr(UiText::ContrastHelp), s.contrast, .5f, 1.5f, defaults.contrast);
            Row(UiText::Highlights, Tr(UiText::HighlightsHelp), s.highlights, -.5f, .5f, defaults.highlights,
                "%+.2f");
            Row(UiText::Shadows, Tr(UiText::ShadowsHelp), s.shadows, -.5f, .5f, defaults.shadows, "%+.2f");
            Row(UiText::Temperature, Tr(UiText::TemperatureHelp), s.temperature, -1, 1, defaults.temperature,
                "%+.2f");
            Row(UiText::Tint, Tr(UiText::TintHelp), s.tint, -1, 1, defaults.tint, "%+.2f");
            Row(UiText::Saturation, Tr(UiText::SaturationHelp), s.saturation, 0, 2, defaults.saturation);
            Row(UiText::Gamma, Tr(UiText::GammaHelp), s.gamma, .5f, 2, defaults.gamma);
            ImGui::EndDisabled();
        }
        ImGui::Dummy({0, 4});
        expanded = Section("bloom", "Bloom", s.bloom, reset);
        if (reset)
            s.resetBloom();
        if (expanded) {
            ImGui::BeginDisabled(!s.bloom || !s.enabled);
            Row(UiText::Strength, Tr(UiText::BloomStrengthHelp), s.bloomStrength, 0, 2,
                defaults.bloomStrength);
            Row(UiText::BrightnessThreshold, Tr(UiText::BrightnessThresholdHelp), s.threshold, 0, 1,
                defaults.threshold);
            Row(UiText::ThresholdSoftness, Tr(UiText::ThresholdSoftnessHelp), s.knee, .001f, .5f,
                defaults.knee);
            Row(UiText::Radius, Tr(UiText::BloomRadiusHelp), s.radius, .5f, 4, defaults.radius);
            auto p = ImGui::GetCursorScreenPos();
            float width = ImGui::GetContentRegionAvail().x;
            TextAt(ImGui::GetWindowDrawList(), {p.x, p.y + 5}, Tr(UiText::Quality));
            ImGui::SetCursorScreenPos({p.x + 146, p.y + 2});
            ImGui::SetNextItemWidth(width - 146 - 25);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.14f, .15f, .17f, 1));
            int quality = s.bloomScale == 4 ? 0 : 1;
            const char *options[] = {Tr(UiText::Economical), Tr(UiText::High)};
            if (ImGui::Combo("##quality", &quality, options, 2))
                s.bloomScale = quality ? 2 : 4;
            ImGui::PopStyleColor();
            Tip(Tr(UiText::BloomQualityHelp));
            ImGui::SetCursorScreenPos(p);
            ImGui::Dummy({width, RowHeight});
            ImGui::EndDisabled();
        }
    } else if (r.editorPage == 1) {
        auto back = ImGui::GetCursorScreenPos();
        auto *draw = ImGui::GetWindowDrawList();
        if (ImGui::InvisibleButton("back", {86, 26}, ImGuiButtonFlags_EnableNav))
            r.editorPage = 0;
        TextAt(draw, {back.x + 20, back.y + 6}, Tr(UiText::Back), ImGui::IsItemHovered() ? Text : Muted, 14);
        draw->AddLine({back.x + 10, back.y + 9}, {back.x + 6, back.y + 13}, Muted);
        draw->AddLine({back.x + 6, back.y + 13}, {back.x + 10, back.y + 17}, Muted);
        ImGui::SetCursorScreenPos(back);
        ImGui::Dummy({280, 34});
        bool expanded = Section("waterMotion", Tr(UiText::Surface), s.water, reset);
        if (reset)
            s.resetWaterSurface();
        if (expanded) {
            ImGui::BeginDisabled(!s.water || !s.enabled);
            Row(UiText::WaveHeight, Tr(UiText::WaveHeightHelp), s.waterWaves, 0, .15f, defaults.waterWaves,
                "%.3f", .005f);
            Row(UiText::Wavelength, Tr(UiText::WavelengthHelp), s.waterLength, 1, 15, defaults.waterLength,
                "%.2f", .25f);
            Row(UiText::Speed, Tr(UiText::SpeedHelp), s.waterSpeed, 0, 3, defaults.waterSpeed, "%.2f", .05f);
            Row(UiText::Ripples, Tr(UiText::RipplesHelp), s.waterRipples, 0, 1, defaults.waterRipples, "%.2f",
                .02f);
            Row(UiText::RippleFrequency, Tr(UiText::RippleFrequencyHelp), s.waterRippleScale, .25f, 6,
                defaults.waterRippleScale, "%.2f", .05f);
            ImGui::Dummy({0, 8});
            Row(UiText::Opacity, Tr(UiText::OpacityHelp), s.waterOpacity, .15f, 1, defaults.waterOpacity,
                "%.2f", .02f);
            Row(UiText::Refraction, Tr(UiText::RefractionHelp), s.waterRefraction, 0, 1,
                defaults.waterRefraction, "%.2f", .02f);
            Row(UiText::OriginalTexture, Tr(UiText::OriginalTextureHelp), s.waterTexture, 0, 1,
                defaults.waterTexture, "%.2f", .02f);
            Row(UiText::TurquoiseTint, Tr(UiText::TurquoiseTintHelp), s.waterTint, 0, 1, defaults.waterTint,
                "%.2f", .02f);
            Row(UiText::SkyReflection, Tr(UiText::SkyReflectionHelp), s.waterSky, 0, 1, defaults.waterSky,
                "%.2f", .02f);
            Row(UiText::SunGlints, Tr(UiText::SunGlintsHelp), s.waterGlints, 0, 2, defaults.waterGlints,
                "%.2f", .05f);
            Row(UiText::Smoothness, Tr(UiText::SmoothnessHelp), s.waterGloss, 0, 1, defaults.waterGloss,
                "%.2f", .02f);
            ImGui::EndDisabled();
        }
        ImGui::Dummy({0, 6});
        bool sceneSection = true;
        if (Section("waterScene", Tr(UiText::DepthReflections), sceneSection, reset, false)) {
            if (reset)
                s.resetWaterScene();
            ImGui::BeginDisabled(!s.water || !s.enabled);
            Row(UiText::Absorption, Tr(UiText::AbsorptionHelp), s.waterDepth, 0, 2, defaults.waterDepth,
                "%.2f", .05f);
            Row(UiText::ShoreTransition, Tr(UiText::ShoreTransitionHelp), s.waterShore, .05f, 3,
                defaults.waterShore, "%.2f", .05f);
            Row(UiText::WorldReflections, Tr(UiText::WorldReflectionsHelp), s.waterSSR, 0, 1,
                defaults.waterSSR, "%.2f", .05f);
            auto p = ImGui::GetCursorScreenPos();
            float width = ImGui::GetContentRegionAvail().x;
            TextAt(ImGui::GetWindowDrawList(), {p.x, p.y + 5}, Tr(UiText::SSRQuality));
            ImGui::SetCursorScreenPos({p.x + 146, p.y + 2});
            ImGui::SetNextItemWidth(width - 171);
            const char *qualities[] = {Tr(UiText::Economical), Tr(UiText::Detailed)};
            ImGui::Combo("##waterSSRQuality", &s.waterSSRQuality, qualities, 2);
            Tip(Tr(UiText::SSRQualityHelp));
            ImGui::SetCursorScreenPos(p);
            ImGui::Dummy({width, RowHeight});
            Row(UiText::Caustics, Tr(UiText::CausticsHelp), s.waterCaustics, 0, 2, defaults.waterCaustics,
                "%.2f", .05f);
            Row(UiText::Dispersion, Tr(UiText::DispersionHelp), s.waterDispersion, 0, 1,
                defaults.waterDispersion, "%.2f", .05f);
            p = ImGui::GetCursorScreenPos();
            TextAt(ImGui::GetWindowDrawList(), {p.x, p.y + 5}, Tr(UiText::CausticQuality));
            ImGui::SetCursorScreenPos({p.x + 146, p.y + 2});
            ImGui::SetNextItemWidth(width - 171);
            int causticQuality = s.waterCausticScale == 2 ? 1 : 0;
            if (ImGui::Combo("##causticQuality", &causticQuality, qualities, 2))
                s.waterCausticScale = causticQuality ? 2 : 4;
            ImGui::SetCursorScreenPos(p);
            ImGui::Dummy({width, RowHeight});
            ImGui::EndDisabled();
        } else if (reset)
            s.resetWaterScene();
    }
    if (r.editorPage == 2) {
        if (ImGui::Button(Label(UiText::BackButton).c_str()))
            r.editorPage = 0;
        ImGui::Dummy({0, 8});
        SceneControls(r);
    }
    if (r.editorPage == 3) {
        if (ImGui::Button(Label(UiText::BackButton).c_str()))
            r.editorPage = 0;
        ImGui::Dummy({0, 8});
        AntialiasControls(r);
    }
    if (r.editorPage == 4)
        EnvironmentControls(r);
    ImGui::EndChild();
    float footer = size.y - 28;
    d->AddLine({origin.x + 1, origin.y + footer}, {origin.x + size.x - 1, origin.y + footer}, Line, 1);
    const char *status = r.message != UiText::None ? Tr(r.message)
                         : r.pendingSave()         ? Tr(UiText::Saving)
                                                   : Tr(UiText::Autosave);
    TextAt(d, {origin.x + 16, origin.y + footer + 7}, FENBVersion, Muted, 12);
    TextAt(d, {origin.x + 65, origin.y + footer + 7}, status, Muted, 12);
    TextAt(d, {origin.x + size.x - 113, origin.y + footer + 7}, "Shift + Enter", Muted, 12);
    s.sanitize();
    if (!(s == before))
        r.settingsChanged();
    if (!r.visible && r.pendingSave())
        r.save();
    ImGui::End();
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
