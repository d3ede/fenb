#include "imgui.h"
#include "runtime.hpp"
#include <cstdio>

namespace {
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
void Tip(const char *text) {
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
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
    d->PathArcTo(c, 5, -.6f, 4.35f, 20);
    d->PathStroke(color, 1.35f);
    d->AddTriangleFilled({c.x - 5.6f, c.y - 5.5f}, {c.x - .4f, c.y - 5.5f}, {c.x - 3.2f, c.y - 1.f}, color);
}
float Snap(float value, float low, float high, float neutral, float step) {
    value = std::clamp(std::round(value / step) * step, low, high);
    return std::abs(value - neutral) < step * .01f ? neutral : value;
}
void Row(const char *label, const char *help, float &value, float low, float high, float neutral,
         const char *format = "%.2f", float step = .01f) {
    ImGui::PushID(label);
    auto p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    auto *d = ImGui::GetWindowDrawList();
    const float trackX = std::min(146.f, width * .32f), inputX = width - 120,
                trackWidth = std::max(20.f, inputX - trackX - 10);
    ImGui::InvisibleButton("label", {trackX - 8, RowHeight});
    Tip(help);
    TextAt(d, {p.x, p.y + 5}, label);
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
    Tip("Перетащи для настройки. Двойной щелчок — исходное значение.");
    // All three fields keep their positions during text editing. Arrow buttons reserve their space.
    ImGui::PushFont(nullptr, 14);
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(Muted));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 3));
    ImGui::SetCursorScreenPos({p.x + inputX, p.y + 2});
    if (ImGui::ArrowButton("less", ImGuiDir_Left))
        value = Snap(value - step, low, high, neutral, step);
    ImGui::SetCursorScreenPos({p.x + inputX + 20, p.y + 2});
    ImGui::SetNextItemWidth(58);
    if (ImGui::InputFloat("##value", &value, 0, 0, format, ImGuiInputTextFlags_AutoSelectAll)) {
        value = std::isfinite(value) ? std::clamp(value, low, high) : neutral;
        if (std::abs(value - neutral) < step * .01f)
            value = neutral;
    }
    Tip("Щелчок — ввести число. Стрелки рядом — изменить на один шаг.");
    ImGui::SetCursorScreenPos({p.x + inputX + 80, p.y + 2});
    if (ImGui::ArrowButton("more", ImGuiDir_Right))
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
        Tip("Сбросить значение");
    }
    ImGui::EndDisabled();
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({width, RowHeight});
    ImGui::PopID();
}
bool Section(const char *id, const char *title, bool &enabled) {
    ImGui::PushID(id);
    auto p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    auto *d = ImGui::GetWindowDrawList();
    auto key = ImGui::GetID("expanded");
    auto *state = ImGui::GetStateStorage();
    bool expanded = state->GetBool(key, true);
    d->AddLine(p, {p.x + width, p.y}, Line, 1);
    if (ImGui::InvisibleButton("heading", {width - 30, 28}, ImGuiButtonFlags_EnableNav)) {
        expanded = !expanded;
        state->SetBool(key, expanded);
    }
    ImU32 c = ImGui::IsItemHovered() ? Text : Muted;
    if (expanded)
        d->AddTriangleFilled({p.x + 1, p.y + 12}, {p.x + 8, p.y + 12}, {p.x + 4.5f, p.y + 16}, c);
    else
        d->AddTriangleFilled({p.x + 2, p.y + 10}, {p.x + 2, p.y + 17}, {p.x + 6, p.y + 13.5f}, c);
    TextAt(d, {p.x + 17, p.y + 6}, title);
    ImGui::SetCursorScreenPos({p.x + width - 22, p.y + 5});
    SquareSwitch("enabled", enabled);
    Tip(enabled ? "Выключить эффект" : "Включить эффект");
    ImGui::SetCursorScreenPos(p);
    ImGui::Dummy({width, 28});
    ImGui::PopID();
    return expanded;
}
int MatchingPreset(const Settings &s) {
    for (int i = 0; i < 4; ++i) {
        Settings candidate;
        candidate.preset(i);
        candidate.enabled = s.enabled;
        candidate.openOnStart = s.openOnStart;
        candidate.panelPage = s.panelPage;
        if (candidate == s)
            return i;
    }
    return -1;
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
    ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(Muted), "Профиль");
    ImGui::SameLine(88);
    ImGui::SetNextItemWidth(width - 216);
    const char *presets[] = {"Нейтральный", "Тёплый", "Холодный", "Мягкий свет"};
    int matching = MatchingPreset(s);
    auto saved = r.selectedProfileSettings;
    saved.enabled = s.enabled;
    saved.openOnStart = s.openOnStart;
    saved.panelPage = s.panelPage;
    std::string caption = r.selectedProfile + (saved == s ? "" : " *");
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.16f, .17f, .19f, 1));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 9));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
    if (ImGui::BeginCombo("##profile",
                          !r.selectedProfile.empty() ? caption.c_str()
                                                     : (matching < 0 ? "Свой" : presets[matching]),
                          ImGuiComboFlags_HeightLarge)) {
        ImGui::TextDisabled("Встроенные");
        for (int i = 0; i < 4; ++i)
            if (ImGui::Selectable(presets[i], matching == i && r.selectedProfile.empty(), 0, {0, 24})) {
                s.preset(i);
                r.selectedProfile.clear();
            }
        ImGui::Separator();
        ImGui::TextDisabled("Мои профили");
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
                        next.openOnStart = s.openOnStart;
                        next.panelPage = s.panelPage;
                        next.enabled = s.enabled;
                        s = next;
                        r.selectedProfile = label;
                        r.selectedProfileSettings = s;
                        r.profileError.clear();
                    } else
                        r.message = "Не удалось прочитать профиль";
                }
            } while (FindNextFileW(find, &data));
            FindClose(find);
        }
        if (!any)
            ImGui::TextDisabled("Пока нет сохранённых профилей");
        ImGui::EndCombo();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
    Tip("Свои .ini можно положить в папку FENB-presets рядом с DLL.");
    ImGui::SameLine(width - 112);
    if (ImGui::Button("Сохранить…", {96, 23})) {
        r.profileError.clear();
        if (!r.selectedProfile.empty())
            std::snprintf(r.profileName, sizeof(r.profileName), "%s", r.selectedProfile.c_str());
        ImGui::OpenPopup("saveProfile");
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16, 14));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));
    ImGui::SetNextWindowSize({330, 0});
    if (ImGui::BeginPopup("saveProfile")) {
        ImGui::TextUnformatted("Сохранить профиль");
        ImGui::SetNextItemWidth(298);
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        bool enter = ImGui::InputTextWithHint("##name", "Название", r.profileName, sizeof(r.profileName),
                                              ImGuiInputTextFlags_EnterReturnsTrue);
        auto name = ProfileName(r.profileName), path = ProfileDirectory(r) + name + L".ini";
        bool exists = !name.empty() && GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
        ImGui::BeginDisabled(name.empty());
        if (ImGui::Button(exists ? "Заменить профиль" : "Сохранить", {182, 26}) ||
            (!name.empty() && enter && !exists)) {
            CreateDirectoryW(ProfileDirectory(r).c_str(), nullptr);
            if (s.save(path)) {
                r.message = "Профиль сохранён";
                r.selectedProfile = UTF8(name);
                r.selectedProfileSettings = s;
                ImGui::CloseCurrentPopup();
            } else
                r.profileError = "Не удалось записать файл";
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Отмена", {108, 26}))
            ImGui::CloseCurrentPopup();
        if (!r.profileError.empty())
            ImGui::TextWrapped("%s", r.profileError.c_str());
        ImGui::PushFont(nullptr, 12);
        ImGui::TextDisabled("Файлы: FENB-presets/*.ini");
        ImGui::PopFont();
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}
} // namespace
void DrawPanel(Runtime &r) {
    Theme();
    auto &io = ImGui::GetIO();
    auto &s = r.settings;
    const Settings before = s;
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
    TextAt(d, {origin.x + 16, origin.y + 12}, "FENB");
    d->AddLine({origin.x + 68, origin.y + 12}, {origin.x + 68, origin.y + 27}, Line, 1);
    TextAt(d, {origin.x + 80, origin.y + 12}, r.editorPage == 1 ? "WaterFX" : "Изображение", Muted, 14);
    ImGui::SetCursorPos({size.x - 151, 8});
    SquareSwitch("master", s.enabled, {20, 24});
    Tip("Включить или выключить все эффекты FENB");
    TextAt(d, {origin.x + size.x - 125, origin.y + 13}, "Эффекты", s.enabled ? Text : Muted, 13);
    ImGui::SetCursorPos({size.x - 57, 8});
    if (ImGui::InvisibleButton("menu", {24, 24}, ImGuiButtonFlags_EnableNav))
        ImGui::OpenPopup("actions");
    auto mp = ImGui::GetItemRectMin();
    for (int i = 0; i < 3; ++i)
        d->AddCircleFilled({mp.x + 6 + i * 5.f, mp.y + 12}, 1, Muted, 8);
    Tip("Действия");
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 7));
    if (ImGui::BeginPopup("actions")) {
#ifdef FENB_DIAGNOSTICS
        if (ImGui::MenuItem("Записать 3 кадра DX9"))
            r.requestCapture();
        ImGui::Separator();
#endif
        if (ImGui::MenuItem("Сохранить настройки", "Ctrl+S"))
            r.save();
        if (ImGui::MenuItem("Перечитать FENB.ini"))
            r.reload();
        ImGui::Separator();
        if (ImGui::MenuItem("Сбросить всё")) {
            bool open = s.openOnStart;
            s = Settings{};
            s.openOnStart = open;
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
    if (r.editorPage == 0) {
        auto waterPos = ImGui::GetCursorScreenPos();
        float waterWidth = ImGui::GetContentRegionAvail().x;
        auto *wd = ImGui::GetWindowDrawList();
        wd->AddLine(waterPos, {waterPos.x + waterWidth, waterPos.y}, Line);
        if (ImGui::Selectable("WaterFX   >", false, 0, {waterWidth - 32, 30}))
            r.editorPage = 1;
        ImGui::SetCursorScreenPos({waterPos.x + waterWidth - 22, waterPos.y + 6});
        SquareSwitch("waterMain", s.water);
        ImGui::SetCursorScreenPos({waterPos.x, waterPos.y + 32});
        ImGui::Dummy({waterWidth, 1});

        if (Section("grade", "Цветокоррекция", s.color)) {
            ImGui::BeginDisabled(!s.color || !s.enabled);
            Row("Экспозиция", "Изменение яркости в ступенях экспозиции (EV).", s.exposure, -2, 2,
                defaults.exposure, "%+.2f");
            Row("Контраст", "Разница между светлыми и тёмными участками.", s.contrast, .5f, 1.5f,
                defaults.contrast);
            Row("Света", "Настройка ярких участков.", s.highlights, -.5f, .5f, defaults.highlights, "%+.2f");
            Row("Тени", "Настройка тёмных участков.", s.shadows, -.5f, .5f, defaults.shadows, "%+.2f");
            Row("Температура", "От холодных оттенков к тёплым.", s.temperature, -1, 1, defaults.temperature,
                "%+.2f");
            Row("Оттенок", "Баланс зелёного и пурпурного.", s.tint, -1, 1, defaults.tint, "%+.2f");
            Row("Насыщенность", "Интенсивность цвета. Ноль — чёрно-белое изображение.", s.saturation, 0, 2,
                defaults.saturation);
            Row("Гамма", "Яркость средних тонов.", s.gamma, .5f, 2, defaults.gamma);
            ImGui::EndDisabled();
        }
        ImGui::Dummy({0, 4});
        if (Section("bloom", "Bloom", s.bloom)) {
            ImGui::BeginDisabled(!s.bloom || !s.enabled);
            Row("Интенсивность", "Сила свечения ярких деталей.", s.bloomStrength, 0, 2,
                defaults.bloomStrength);
            Row("Порог яркости", "Минимальная яркость, при которой появляется свечение.", s.threshold, 0, 1,
                defaults.threshold);
            Row("Мягкость порога", "Плавность появления свечения около порога.", s.knee, .001f, .5f,
                defaults.knee);
            Row("Радиус", "Размер ореола вокруг ярких деталей.", s.radius, .5f, 4, defaults.radius);
            auto p = ImGui::GetCursorScreenPos();
            float width = ImGui::GetContentRegionAvail().x;
            TextAt(ImGui::GetWindowDrawList(), {p.x, p.y + 5}, "Качество");
            ImGui::SetCursorScreenPos({p.x + 146, p.y + 2});
            ImGui::SetNextItemWidth(width - 146 - 25);
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(.14f, .15f, .17f, 1));
            int quality = s.bloomScale == 4 ? 0 : 1;
            const char *options[] = {"Экономное", "Высокое"};
            if (ImGui::Combo("##quality", &quality, options, 2))
                s.bloomScale = quality ? 2 : 4;
            ImGui::PopStyleColor();
            Tip("Экономное: текстуры в четверть ширины и высоты кадра. Высокое: в половину.");
            ImGui::SetCursorScreenPos(p);
            ImGui::Dummy({width, RowHeight});
            ImGui::EndDisabled();
        }
        ImGui::Dummy({0, 6});
    } else {
        if (ImGui::Button("<  Назад", {86, 26}))
            r.editorPage = 0;
        ImGui::SameLine();
        ImGui::TextDisabled("Волны и освещение");
        ImGui::Dummy({0, 8});
        if (Section("waterMotion", "Поверхность", s.water)) {
            ImGui::BeginDisabled(!s.water || !s.enabled);
            Row("Высота волн", "Небольшое смещение вершин воды. Крупная сетка ограничивает детализацию.",
                s.waterWaves, 0, .15f, defaults.waterWaves, "%.3f", .005f);
            Row("Длина волн", "Расстояние между гребнями волн в координатах мира.", s.waterLength, 1, 15,
                defaults.waterLength, "%.2f", .25f);
            Row("Скорость", "Скорость волн и ряби FENB. Ноль останавливает их движение.", s.waterSpeed, 0, 3,
                defaults.waterSpeed, "%.2f", .05f);
            Row("Рябь", "Мелкие волны в нормалях поверхности. Ноль отключает рябь.", s.waterRipples, 0, 1,
                defaults.waterRipples, "%.2f", .02f);
            Row("Частота ряби", "Количество мелких волн на поверхности.", s.waterRippleScale, .25f, 6,
                defaults.waterRippleScale, "%.2f", .05f);
            ImGui::Dummy({0, 8});
            Row("Непрозрачность",
                "Меньше — лучше видно уже отрисованное дно. Без чтения глубины и без преломления.",
                s.waterOpacity, .15f, 1, defaults.waterOpacity, "%.2f", .02f);
            Row("Бирюзовый тон", "Примесь цвета к исходной текстуре воды.", s.waterTint, 0, 1,
                defaults.waterTint, "%.2f", .02f);
            Row("Отражение неба", "Приближённый цвет неба с эффектом Френеля. Объекты мира не отражаются.",
                s.waterSky, 0, 1, defaults.waterSky, "%.2f", .02f);
            Row("Солнечные блики", "Блики от направленного света игры. Ноль отключает блики.", s.waterGlints,
                0, 2, defaults.waterGlints, "%.2f", .05f);
            Row("Гладкость", "Выше — уже и резче солнечный блик.", s.waterGloss, 0, 1, defaults.waterGloss,
                "%.2f", .02f);
            ImGui::EndDisabled();
        }
        ImGui::Dummy({0, 10});
        ImGui::PushFont(nullptr, 12);
        ImGui::TextDisabled(!s.water || !s.enabled ? "WaterFX выключен"
                            : r.lastWaterDraws     ? "WaterFX активен"
                                                   : "В кадре нет распознанной воды");
        ImGui::TextDisabled("Отражение неба приблизительное. Без отражений объектов.");
        ImGui::PopFont();
    }
    ImGui::EndChild();
    float footer = size.y - 28;
    d->AddLine({origin.x + 1, origin.y + footer}, {origin.x + size.x - 1, origin.y + footer}, Line, 1);
    const char *status = !r.message.empty() ? r.message.c_str()
                         : r.pendingSave()  ? "Сохраняется…"
                                            : "Автосохранение";
    TextAt(d, {origin.x + 16, origin.y + footer + 7}, status, Muted, 12);
    TextAt(d, {origin.x + size.x - 113, origin.y + footer + 7}, "Shift + Enter", Muted, 12);
    s.sanitize();
    if (!(s == before))
        r.settingsChanged();
    if (!r.visible && r.pendingSave())
        r.save();
    ImGui::End();
}
